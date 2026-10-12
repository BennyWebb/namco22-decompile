/* Simulation-owned video capture and renderer-owned immutable snapshots.
 * Only mailbox ownership/metrics are shared; no lock spans copying or GL. */
#define _GNU_SOURCE
#include "rr_presentation.h"
#include "rr_scene.h"
#include "rr_gles2.h"
#include "rr_gles2_timing.h"
#include <SDL.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>

#define SLOTS 3
#define RECORDS 8192

enum { FREE, WRITING, READY, READING };
static bool enabled = true, serial, started, closing, worker_failed, attached;
static pthread_t worker;
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static rr_presentation *slots;
static int state[SLOTS];
static SDL_Window *window;
static SDL_GLContext context;
static unsigned every, capture, limit_hz, first, last_frame;
static const char *csv_path;
static bool sync_profile;
typedef struct { double acquire, maximum, idle; unsigned calls; } sync_record;
static sync_record producer_sync, consumer_sync;
/* TLS selects statistics ownership, not any emulated or rendering state. */
static _Thread_local sync_record *sync_owner;

/* Producer-only palette ledger: generations must advance on dropped ticks. */
static uint8_t previous_palette[RR_PAL_SIZE];
static uint16_t generation[128];
static rr_frame_clock clock_state;
static double cpu_last, wall_last, sim_start, sim_end, cpu_sum, copy_sum;
static unsigned steps, dropped, queue_peak, range_dropped, range_resyncs;
static double range_discarded;
typedef struct {
    uint32_t frame;
    double begin, end, cpu, scene, age;
    unsigned age_ticks, core;
} render_record;
static render_record records[RECORDS];
static unsigned record_count, render_count, record_overflow;
static double render_cpu, render_scene, render_age, tail_end[SLOTS];
static double producer_cpu, live_report;
static unsigned worker_cores[4], sim_cores[4];

static double stamp(clockid_t clock)
{
    struct timespec t;
    clock_gettime(clock, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}
static double now(void) { return stamp(CLOCK_MONOTONIC); }
/* Optional diagnostic only; timings include acquisition and clock overhead,
 * not work done while holding a lock. Condvar idle is reported separately. */
static void metadata_lock(void)
{
    double start = sync_profile ? now() : 0;
    pthread_mutex_lock(&mutex);
    if(sync_profile) {
        double cost = now()-start;
        sync_owner->acquire += cost;
        sync_owner->calls++;
        if(cost>sync_owner->maximum)sync_owner->maximum=cost;
    }
}
static void metadata_wait(void)
{
    double start=sync_profile ? now() : 0;
    pthread_cond_wait(&changed,&mutex);
    if(sync_profile)sync_owner->idle+=now()-start;
}
bool rr_presentation_enabled(void) { return enabled; }
void rr_presentation_configure(bool on, bool reference, const char *csv, bool profile_sync)
{
    enabled = on;
    serial = reference;
    csv_path = csv;
    sync_profile = profile_sync;
}

static void palette_tick(void)
{
    for (unsigned g = 0; g < 128; g++) {
        unsigned off = g * 256;
        bool different = false;
        for (unsigned p = 0; p < 3; p++)
            if (memcmp(previous_palette + off + p * 0x8000,
                       g_rr.pal + off + p * 0x8000, 256)) different = true;
        if (different) {
            generation[g]++;
            for (unsigned p = 0; p < 3; p++)
                memcpy(previous_palette + off + p * 0x8000,
                       g_rr.pal + off + p * 0x8000, 256);
        }
    }
}

/* Consumer only (or producer in explicit serial snapshot-reference mode). */
static bool render(const rr_presentation *snapshot)
{
    double start = now(), cpu = stamp(CLOCK_THREAD_CPUTIME_ID);
    rr_gl_presentation(snapshot);
    int count;
    const geo_quad *quads = rr_gles2_quads(&count);
    float bg[3];
    rr_gles2_background(bg);
    double scene = now() - start;
    bool ok = rr_gles2_present(snapshot->frame, quads, count, bg, scene);
    double end = now(), cost = stamp(CLOCK_THREAD_CPUTIME_ID) - cpu;
    rr_gl_presentation_release();

    metadata_lock();
    unsigned age = last_frame - snapshot->frame;
    unsigned core = (unsigned)sched_getcpu();
    if (snapshot->frame >= first) {
        render_cpu += cost;
        render_scene += scene;
        render_age += end - snapshot->produced;
        tail_end[render_count % SLOTS] = end;
        render_count++;
        if (core < 4) worker_cores[core]++;
    }
    pthread_mutex_unlock(&mutex);
    if (snapshot->frame >= first) {
        if (record_count < RECORDS)
            records[record_count++] = (render_record){ snapshot->frame, start, end,
                cost, scene, end - snapshot->produced, age, core };
        else record_overflow++;
    }
    return ok;
}

/* Caller holds mutex. WRITING/READING slots are never candidates. */
static int newest(void)
{
    int best = -1;
    for (int i = 0; i < SLOTS; i++)
        if (state[i] == READY && (best < 0 || slots[i].frame > slots[best].frame)) best = i;
    return best;
}
static void discard_ready(int slot)
{
    state[slot] = FREE;
    dropped++;
    if (slots[slot].frame >= first) range_dropped++;
}

static void *consume(void *unused)
{
    (void)unused;
    sync_owner=&consumer_sync;
    pthread_setname_np(pthread_self(), "rr-present");
    bool ok = SDL_GL_MakeCurrent(window, context) == 0;
    metadata_lock();
    attached = true;
    worker_failed = !ok;
    pthread_cond_broadcast(&changed);
    pthread_mutex_unlock(&mutex);
    if (!ok) {
        fprintf(stderr, "[THREAD] context attach: %s\n", SDL_GetError());
        return NULL;
    }
    fprintf(stderr, "[THREAD] presentation_tid=%ld affinity=OS\n", (long)syscall(SYS_gettid));
    for (;;) {
        metadata_lock();
        int slot;
        while ((slot = newest()) < 0 && !closing) metadata_wait();
        if (slot < 0) {
            pthread_mutex_unlock(&mutex);
            break;
        }
        state[slot] = READING;
        for (int i = 0; i < SLOTS; i++)
            if (state[i] == READY) discard_ready(i);
        pthread_mutex_unlock(&mutex);

        ok = render(&slots[slot]);
        metadata_lock();
        state[slot] = FREE;
        if (!ok) { worker_failed = true; closing = true; }
        pthread_cond_broadcast(&changed);
        pthread_mutex_unlock(&mutex);
        if (!ok) break;
    }
    if (SDL_GL_MakeCurrent(window, NULL) != 0) {
        fprintf(stderr, "[THREAD] context detach: %s\n", SDL_GetError());
        metadata_lock();
        worker_failed = true;
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

bool rr_presentation_start(void *w, void *c, unsigned cadence, unsigned shot,
                           unsigned hz, unsigned from)
{
    sync_owner=&producer_sync;
    memset(&producer_sync,0,sizeof producer_sync);
    memset(&consumer_sync,0,sizeof consumer_sync);
    memset(state, 0, sizeof state);
    memset(previous_palette, 0, sizeof previous_palette);
    memset(generation, 0, sizeof generation);
    memset(&clock_state, 0, sizeof clock_state);
    memset(worker_cores, 0, sizeof worker_cores);
    memset(sim_cores, 0, sizeof sim_cores);
    memset(tail_end, 0, sizeof tail_end);
    closing = worker_failed = attached = false;
    steps = dropped = range_dropped = range_resyncs = queue_peak = 0;
    record_count = render_count = record_overflow = 0;
    range_discarded = 0;
    cpu_sum = copy_sum = producer_cpu = render_cpu = render_scene = render_age = 0;
    sim_start = sim_end = live_report = 0;
    window = w; context = c; every = cadence; capture = shot; limit_hz = hz; first = from;
    slots = calloc(SLOTS, sizeof *slots);
    if (!slots) { perror("presentation snapshots"); return false; }
    fprintf(stderr, "[THREAD] simulation_tid=%ld mode=%s slots=%d bytes=%zu "
            "point_copy_bytes=%zu target_hz=%.9f\n", (long)syscall(SYS_gettid),
            serial ? "snapshot-reference" : "concurrent", SLOTS,
            SLOTS * sizeof *slots, sizeof slots[0].point, 1.0 / RR_FRAME_SECONDS);
    if (!serial) {
        if (SDL_GL_MakeCurrent(window, NULL) != 0) {
            fprintf(stderr, "[THREAD] main context detach: %s\n", SDL_GetError());
            free(slots); slots = NULL; return false;
        }
        int error = pthread_create(&worker, NULL, consume, NULL);
        if (error) {
            fprintf(stderr, "[THREAD] pthread_create: %s\n", strerror(error));
            SDL_GL_MakeCurrent(window, context);
            free(slots); slots = NULL; return false;
        }
        metadata_lock();
        while (!attached) metadata_wait();
        bool failed = worker_failed;
        pthread_mutex_unlock(&mutex);
        if (failed) {
            pthread_join(worker, NULL);
            SDL_GL_MakeCurrent(window, context);
            free(slots); slots = NULL; return false;
        }
    }
    started = true;
    cpu_last = stamp(CLOCK_THREAD_CPUTIME_ID);
    wall_last = now();
    return true;
}

bool rr_presentation_video(uint32_t frame, bool slave)
{
    double cpu = stamp(CLOCK_THREAD_CPUTIME_ID);
    double serial_cpu_before = serial ? render_cpu : 0;
    if (frame >= first) {
        if (!steps) {
            sim_start = wall_last; live_report = sim_start;
            range_resyncs = clock_state.resyncs; range_discarded = clock_state.discarded;
        }
        steps++;
        cpu_sum += cpu - cpu_last;
        unsigned core = (unsigned)sched_getcpu();
        if (core < 4) sim_cores[core]++;
    }
    if (!rr_gles2_pump()) return false;
    rr_clock_begin(&clock_state, now());
    while (now() < clock_state.deadline) {
        SDL_Delay(1);
        if (!rr_gles2_pump()) return false;
    }
    bool selected = rr_clock_render(&clock_state, now(), limit_hz, frame == capture);
    if (every) selected = frame % every == 0 || frame == capture;

    /* Every emulated video boundary, whether or not a picture is published. */
    palette_tick();
    bool walk = rr_scene_frame(slave);
    unsigned direct = rr_scene_direct_count();
    rr_gles2_scene_tick(walk, direct);
    if (direct > RR_PRESENT_DIRECT_MAX) {
        fprintf(stderr, "[THREAD] direct FIFO exceeds snapshot capacity\n");
        metadata_lock();
        worker_failed = true;
        pthread_mutex_unlock(&mutex);
        return false;
    }

    metadata_lock();
    last_frame = frame;
    bool failed = worker_failed;
    int slot = -1;
    if (selected && !failed) {
        for (int i = 0; i < SLOTS; i++)
            if (state[i] == FREE) { slot = i; break; }
        if (slot < 0)
            for (int i = 0; i < SLOTS; i++)
                if (state[i] == READY && (slot < 0 || slots[i].frame < slots[slot].frame)) slot = i;
        if (slot >= 0) {
            if (state[slot] == READY) discard_ready(slot);
            state[slot] = WRITING;
        }
    }
    pthread_mutex_unlock(&mutex);

    if (slot >= 0) {
        double copy_cpu = stamp(CLOCK_THREAD_CPUTIME_ID);
        rr_presentation *snapshot = &slots[slot];
        snapshot->frame = frame; snapshot->walk = walk;
        snapshot->direct_count = direct; snapshot->produced = now();
        memcpy(snapshot->poly, g_rr.poly, sizeof snapshot->poly);
        memcpy(snapshot->pal, g_rr.pal, sizeof snapshot->pal);
        memcpy(snapshot->mixer, g_rr.mixer, sizeof snapshot->mixer);
        memcpy(snapshot->czram, g_rr.czram, sizeof snapshot->czram);
        memcpy(snapshot->generation, generation, sizeof generation);
        snapshot->point_valid = rr_dsp_copy_point(snapshot->point);
        for (unsigned i = 0; i < direct; i++)
            memcpy(snapshot->direct[i], rr_scene_direct(i), sizeof snapshot->direct[i]);
        if (frame >= first) copy_sum += stamp(CLOCK_THREAD_CPUTIME_ID) - copy_cpu;

        metadata_lock();
        state[slot] = READY;
        unsigned depth = 0;
        for (int i = 0; i < SLOTS; i++) depth += state[i] == READY;
        if (depth > queue_peak) queue_peak = depth;
        pthread_cond_broadcast(&changed);
        pthread_mutex_unlock(&mutex);
        if (serial) {
            bool ok = render(snapshot);
            metadata_lock();
            state[slot] = FREE; worker_failed = !ok;
            pthread_mutex_unlock(&mutex);
        } else if (every || frame == capture) {
            /* Correctness only: consume selected checkpoints; never benchmark this. */
            metadata_lock();
            while (state[slot] != FREE && !worker_failed) metadata_wait();
            pthread_mutex_unlock(&mutex);
        }
    }
    rr_scene_consume();
    sim_end = now();
    cpu_last = stamp(CLOCK_THREAD_CPUTIME_ID);
    wall_last = sim_end;
    if (frame >= first) {
        producer_cpu += cpu_last - cpu - (serial ? render_cpu - serial_cpu_before : 0);
        if (sim_end - live_report >= 5) {
            metadata_lock();
            unsigned count = render_count;
            pthread_mutex_unlock(&mutex);
            fprintf(stderr, "[THREAD-WINDOW] frame=%u steps=%u presentations=%u sim_hz=%.3f "
                    "render_fps=%.3f\n", frame, steps, count,
                    steps / (sim_end - sim_start), count / (sim_end - sim_start));
            live_report = sim_end;
        }
    }
    metadata_lock();
    failed = worker_failed;
    pthread_mutex_unlock(&mutex);
    return !failed;
}

bool rr_presentation_finish(void)
{
    if (!started) return true;
    metadata_lock();
    closing = true;
    pthread_cond_broadcast(&changed);
    pthread_mutex_unlock(&mutex);
    if (!serial) {
        pthread_join(worker, NULL);
        if (SDL_GL_MakeCurrent(window, context) != 0) {
            fprintf(stderr, "[THREAD] cleanup context: %s\n", SDL_GetError());
            worker_failed = true;
        }
    }
    bool ok = !worker_failed;
    FILE *csv = csv_path ? fopen(csv_path, "w") : NULL;
    if (csv_path && !csv) { perror("thread profile CSV"); ok = false; }
    if (csv) {
        fprintf(csv, "frame,start,end,cpu,scene_wall,publication_age,age_ticks,core\n");
        for (unsigned i = 0; i < record_count; i++) {
            const render_record *r = &records[i];
            fprintf(csv, "%u,%.9f,%.9f,%.9f,%.9f,%.9f,%u,%u\n", r->frame,
                    r->begin, r->end, r->cpu, r->scene, r->age, r->age_ticks, r->core);
        }
        if (ferror(csv)) ok = false;
        if (fclose(csv)) { perror("thread profile CSV close"); ok = false; }
    }
    /* At most two READY plus one READING can survive the final producer tick.
     * All later completions are in this bounded tail, independent of CSV overflow. */
    unsigned drain = 0;
    for (unsigned i = 0; i < (render_count < SLOTS ? render_count : SLOTS); i++)
        drain += tail_end[i] > sim_end;
    unsigned completed = render_count - drain;
    double elapsed = sim_end - sim_start;
    if (steps && elapsed > 0) {
        double overlap = (cpu_sum + producer_cpu + render_cpu) / elapsed;
        fprintf(stderr, "[THREAD-RATE] from=%u steps=%u rendered=%u drain=%u sim_hz=%.6f "
                "render_fps=%.6f sim_cpu_ms=%.6f copy_cpu_ms=%.6f presentation_cpu_ms=%.6f "
                "scene_wall_ms=%.6f publication_age_ms=%.6f cpu_cores_equivalent=%.6f "
                "dropped=%u queue_peak=%u resyncs=%u discarded_lag_ms=%.6f "
                "worker_core_counts=%u/%u/%u/%u\n", first, steps, completed, drain,
                steps / elapsed, completed / elapsed, cpu_sum * 1000 / steps,
                copy_sum * 1000 / steps, render_count ? render_cpu * 1000 / render_count : 0,
                render_count ? render_scene * 1000 / render_count : 0,
                render_count ? render_age * 1000 / render_count : 0, overlap,
                range_dropped, queue_peak, clock_state.resyncs - range_resyncs,
                (clock_state.discarded - range_discarded) * 1000,
                worker_cores[0], worker_cores[1], worker_cores[2], worker_cores[3]);
    }
    fprintf(stderr, "[THREAD-PERIOD] start=%.9f end=%.9f total_dropped=%u\n",
            sim_start, sim_end, dropped);
    fprintf(stderr, "[THREAD-DETAIL] producer_cpu_ms=%.6f includes_copy=1 record_overflow=%u "
            "simulation_core_counts=%u/%u/%u/%u presentation_core_counts=%u/%u/%u/%u\n",
            steps ? producer_cpu * 1000 / steps : 0, record_overflow,
            sim_cores[0], sim_cores[1], sim_cores[2], sim_cores[3],
            worker_cores[0], worker_cores[1], worker_cores[2], worker_cores[3]);
    if(sync_profile)fprintf(stderr,"[THREAD-SYNC] scope=whole-run producer_calls=%u producer_acquire_ms=%.6f producer_acquire_max_us=%.6f producer_cond_wait_ms=%.6f consumer_calls=%u consumer_acquire_ms=%.6f consumer_acquire_max_us=%.6f consumer_idle_ms=%.6f\n",
        producer_sync.calls,producer_sync.acquire*1000,producer_sync.maximum*1e6,producer_sync.idle*1000,
        consumer_sync.calls,consumer_sync.acquire*1000,consumer_sync.maximum*1e6,consumer_sync.idle*1000);
    free(slots);
    slots = NULL;
    started = false;
    return ok;
}
