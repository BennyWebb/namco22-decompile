#include "rr_sound.h"
#include "renderer_api.h"
/* Simulation-owned video capture and renderer-owned immutable snapshots.
 * Only mailbox ownership/metrics are shared; no lock spans copying or GL. */
#define _GNU_SOURCE
#include "rr_presentation.h"
#include "rr_scene.h"
#include "rr_gles3.h"
#include "timing.h"
#include <SDL.h>
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>


static int p1_slots=GLES3_QUEUE_DEPTH;
#define SLOTS p1_slots
#define RECORDS 8192

enum { FREE, WRITING, READY, READING, PREPARING, PREPARED };
static bool enabled = true, serial, started, closing, worker_failed, attached;
static pthread_t worker;
#ifdef RR_PARALLEL_FRAME_PREP
static pthread_t prep_worker;
static bool parallel, prep_started, prep_done;
/* Defer preparation briefly after GLES claims its previous frame. This lets
 * a newer simulation snapshot replace an old READY one without spending the
 * entire next emulated tick waiting for it. */
static double consumer_claimed_at;
static double prep_cpu_sum, hud_cpu_sum, prep_wall_sum;
static unsigned prepared_count, dropped_prepared;
#ifdef RR_PARALLEL_PREP_PROFILE
static double direct_cpu_sum, list_cpu_sum, sort_cpu_sum;
#endif
#endif
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t changed = PTHREAD_COND_INITIALIZER;
static rr_presentation *slots;
static int state[3];
static SDL_Window *window;
static SDL_GLContext context;
static unsigned every, capture, limit_hz, first, last_frame;
static const char *csv_path;
static bool sync_profile;
typedef struct { double acquire, maximum, idle; unsigned calls; } sync_record;
static sync_record producer_sync, consumer_sync;
#ifdef RR_PARALLEL_FRAME_PREP
static sync_record preparer_sync;
#endif
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
    double begin, end, cpu, scene, age, presented;
    unsigned age_ticks, core;
} render_record;
static render_record records[RECORDS];
static unsigned record_count, render_count, record_overflow;
static FILE *profile_csv;
static char profile_csv_buffer[65536];
static double render_cpu, render_scene, render_age, tail_end[3];
static double producer_cpu, live_report;
static unsigned worker_cores[4], sim_cores[4];
static void free_slots(void)
{
    if(!started&&profile_csv){fclose(profile_csv);profile_csv=NULL;}
    if(!slots)return;
#ifdef RR_PARALLEL_FRAME_PREP
    for(int i=0;i<SLOTS;i++){
        free(slots[i].gles3_payload);free(slots[i].g1b_dynamic);

    }
#endif
    free(slots);slots=NULL;
}

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
    if(!on||reference)gles3_unsupported(0,0,0,-1,-1,NULL,"unsupported pipeline configuration");
    enabled=true;serial=false;
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
    double start=now(),cpu=stamp(CLOCK_THREAD_CPUTIME_ID);
    double scene=snapshot->scene_wall;
    bool ok=rr_gles3_present_prepared(snapshot);
    double end = now(), cost = stamp(CLOCK_THREAD_CPUTIME_ID) - cpu;
    double presented=rr_gles3_last_presentation();
    rr_gl_presentation_release();

    metadata_lock();
    unsigned age = last_frame - snapshot->frame;
    unsigned ready_depth=0;for(int i=0;i<SLOTS;i++)ready_depth+=state[i]==READY||state[i]==PREPARED;
    unsigned observed_drops=dropped;
    unsigned core = (unsigned)sched_getcpu();
    if (snapshot->frame >= first) {
        render_cpu += cost;
        render_scene += scene;
        render_age += presented - snapshot->produced;
        tail_end[render_count % SLOTS] = end;
        render_count++;
        if (core < 4) worker_cores[core]++;
    }
    pthread_mutex_unlock(&mutex);
    if (snapshot->frame >= first) {
        if (record_count < RECORDS)
            records[record_count++] = (render_record){ snapshot->frame, start, end,
                cost, scene, presented - snapshot->produced, presented, age, core };
        else record_overflow++;
        if(profile_csv) {
            rr_presentation_metrics m=rr_gles3_last_metrics();
            fprintf(profile_csv,"%u,%.9f,%.9f,%.9f,%.9f,%.9f,%u,%u,%.9f,%.9f,%.9f,%.9f,%u,%u,%u,%u,%u,%u,%zu,%u,%u,%.9f,%.9f,%.9f,%.0f,%.9f,%llu,%llu",
                snapshot->frame,start,end,cost,scene,presented-snapshot->produced,age,core,presented,
                m.draw_wall,m.submission_wall,m.swap_wall,m.draws,m.triangles,m.binds,m.uploads,m.misses,m.evictions,m.texture_bytes,ready_depth,observed_drops,m.lookup_wall,m.decode_wall,m.upload_wall,m.decoded_texels,m.eviction_wall,m.clock_probes,m.delete_calls);
#ifdef RR_PARALLEL_FRAME_PREP
            fprintf(profile_csv,",%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f",parallel?snapshot->prep_cpu:0,
                    parallel?snapshot->hud_cpu:0,parallel?snapshot->prepared_at-snapshot->produced:0,
                    parallel?start-snapshot->prepared_at:0,
                    parallel?snapshot->ready_at-snapshot->produced:0,
                    parallel?snapshot->prep_started_at-snapshot->ready_at:0,
                    parallel?snapshot->prepared_at-snapshot->prep_started_at:0);
#ifdef RR_PARALLEL_PREP_PROFILE
            fprintf(profile_csv,",%.9f,%.9f,%.9f",parallel?snapshot->direct_cpu:0,
                    parallel?snapshot->list_cpu:0,parallel?snapshot->sort_cpu:0);
#endif
#endif
            fprintf(profile_csv,",%u,%.9f,%.9f,%.9f,%.9f,%.9f,%u,%u",snapshot->gles3_tag,snapshot->p1_sim_ms,snapshot->gles3_walk_ms,snapshot->gles3_compile_ms,snapshot->p1_block_ms,snapshot->p1_audio_ratio,snapshot->p1_underruns,snapshot->p1_depth);
#ifdef GLES3_RECORD_DIAGNOSTICS
            fprintf(profile_csv,",%u,%u,%u,%u,%u",snapshot->record_splits[0],snapshot->record_splits[1],snapshot->record_splits[2],snapshot->record_splits[3],snapshot->record_protected_visible);
#endif
            fputc('\n',profile_csv);
            if(ferror(profile_csv))ok=false;
        }
    }
    return ok;
}

/* Caller holds mutex. WRITING/READING slots are never candidates. */
static int newest(int wanted)
{
    int best = -1;
    for (int i = 0; i < SLOTS; i++)
        if (state[i] == wanted && (best < 0 || slots[i].frame < slots[best].frame)) best = i;
    return best;
}
static void discard_ready(int slot)
{
#ifdef RR_PARALLEL_FRAME_PREP
    if(state[slot]==PREPARED&&slots[slot].frame>=first)dropped_prepared++;
#endif
    state[slot] = FREE;
    dropped++;
    if (slots[slot].frame >= first) range_dropped++;
}

#ifdef RR_PARALLEL_FRAME_PREP
static void *prepare(void *unused)
{
    (void)unused;
    sync_owner=&preparer_sync;
    pthread_setname_np(pthread_self(),"rr-prepare");
    fprintf(stderr,"[PREP] tid=%ld affinity=OS\n",(long)syscall(SYS_gettid));
    for(;;) {
        metadata_lock();
        int slot;
        /* Keep at most one completed picture ahead of GL. Without this gate
         * the preparer decoded nearly every 60 Hz snapshot while GL displayed
         * only ~40 Hz, wasting a third of its CPU work and bandwidth. READY
         * holds the latest undecoded state while GL is busy. */
        for(;;) {
            int ready=newest(READY);
            if(worker_failed)break;
            if(newest(PREPARED)>=0 || (ready<0&&!closing)) {
                metadata_wait();
                continue;
            }
            break;
        }
        slot=worker_failed?-1:newest(READY);
        if(slot<0){prep_done=true;pthread_cond_broadcast(&changed);pthread_mutex_unlock(&mutex);break;}
        state[slot]=PREPARING;
        /* Strict FIFO: no READY payload is discarded. */
        pthread_mutex_unlock(&mutex);

        rr_presentation *s=&slots[slot];
        double wall=now(),cpu=stamp(CLOCK_THREAD_CPUTIME_ID);
        s->prep_started_at=wall;
        bool ok=rr_p1_prepare(s);
        rr_gl_p1_select(s);
        s->scene_wall=now()-wall;
        double hud_start=stamp(CLOCK_THREAD_CPUTIME_ID);
        rr_video_layers_profile_gate(s->frame>=rr_gles3_profile_from());
        if(ok)ok=rr_gl_video_prepare(&s->video);
        s->hud_cpu=stamp(CLOCK_THREAD_CPUTIME_ID)-hud_start;
        s->prep_cpu=stamp(CLOCK_THREAD_CPUTIME_ID)-cpu;
        s->prepared_at=now();
        rr_gl_presentation_release();
        metadata_lock();
        if(!ok){worker_failed=true;closing=true;state[slot]=FREE;}
        else {
            state[slot]=PREPARED;
            if(s->frame>=first){
                prep_cpu_sum+=s->prep_cpu;
                hud_cpu_sum+=s->hud_cpu;
                prep_wall_sum+=s->scene_wall;
                prepared_count++;
#ifdef RR_PARALLEL_PREP_PROFILE
                direct_cpu_sum+=s->direct_cpu;
                list_cpu_sum+=s->list_cpu;
                sort_cpu_sum+=s->sort_cpu;
#endif
            }
        }
        pthread_cond_broadcast(&changed);
        pthread_mutex_unlock(&mutex);
        if(!ok)break;
    }
    metadata_lock();prep_done=true;pthread_cond_broadcast(&changed);pthread_mutex_unlock(&mutex);
    return NULL;
}
#endif

static void *consume(void *unused)
{
    (void)unused;
    sync_owner=&consumer_sync;
    pthread_setname_np(pthread_self(), "rr-present");
    bool ok = SDL_GL_MakeCurrent(window, context) == 0;
    if(ok && SDL_GL_SetSwapInterval(1) && !getenv("ENG_GLES3_HOST"))ok=false;
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
        while ((slot = newest(
#ifdef RR_PARALLEL_FRAME_PREP
                   parallel ? PREPARED : READY
#else
                   READY
#endif
                   )) < 0 && (!closing
#ifdef RR_PARALLEL_FRAME_PREP
                   || (parallel && !prep_done)
#endif
                   )) metadata_wait();
        if (slot < 0) {
            pthread_mutex_unlock(&mutex);
            break;
        }
        state[slot] = READING;
#ifdef RR_PARALLEL_FRAME_PREP
        if(parallel)consumer_claimed_at=now();
#endif
        pthread_cond_broadcast(&changed);
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
    p1_slots=2;
    if(p1_slots!=2&&p1_slots!=3)return false;
    sync_owner=&producer_sync;
    memset(&producer_sync,0,sizeof producer_sync);
    memset(&consumer_sync,0,sizeof consumer_sync);
#ifdef RR_PARALLEL_FRAME_PREP
    memset(&preparer_sync,0,sizeof preparer_sync);
    parallel=!serial;
    prep_started=prep_done=false;
    consumer_claimed_at=0;
    prep_cpu_sum=hud_cpu_sum=prep_wall_sum=0;
    prepared_count=dropped_prepared=0;
#ifdef RR_PARALLEL_PREP_PROFILE
    direct_cpu_sum=list_cpu_sum=sort_cpu_sum=0;
#endif
#endif
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
    if(csv_path) {
        profile_csv=fopen(csv_path,"wx");
        if(!profile_csv){perror("thread profile CSV");return false;}
        setvbuf(profile_csv,profile_csv_buffer,_IOFBF,sizeof profile_csv_buffer);
        fputs("frame,start,end,cpu,scene_wall,publication_age,age_ticks,core,presented,draw_wall,submission_wall,swap_wall,draws,triangles,binds,uploads,misses,evictions,texture_bytes,queue_ready,drops_cumulative,lookup_decode_wall,decode_wall,upload_wall,decoded_texels,eviction_wall,clock_probes,delete_calls",profile_csv);
#ifdef RR_PARALLEL_FRAME_PREP
        fputs(",prep_cpu,hud_cpu,prep_queue_age,prepared_wait,snapshot_copy_wall,snapshot_queue_wait,prep_wall",profile_csv);
#ifdef RR_PARALLEL_PREP_PROFILE
        fputs(",direct_cpu,list_cpu,sort_cpu",profile_csv);
#endif
#endif
        fputs(",payload_gles3,simulation_ms,gles3_walk_ms,gles3_compile_ms,simulation_blocked_ms,resampler_ratio,audio_underruns,occupied_slots",profile_csv);
#ifdef GLES3_RECORD_DIAGNOSTICS
        fputs(",split_protected,split_page,split_scissor,split_mode,protected_visible",profile_csv);
#endif
        fputc('\n',profile_csv);
    }
    slots = calloc(SLOTS, sizeof *slots);
    if (!slots) { perror("presentation snapshots"); free_slots(); return false; }
#ifdef RR_PARALLEL_FRAME_PREP

#endif
    fprintf(stderr, "[THREAD] simulation_tid=%ld mode=%s slots=%d bytes=%zu "
            "point_copy_bytes=%zu target_hz=%.9f\n", (long)syscall(SYS_gettid),
            serial ? "snapshot-reference" :
#ifdef RR_PARALLEL_FRAME_PREP
            parallel ? "three-stage" :
#endif
            "concurrent", SLOTS,
            SLOTS * sizeof *slots, sizeof slots[0].point, 1.0 / RR_FRAME_SECONDS);
    if (!serial) {
        if (SDL_GL_MakeCurrent(window, NULL) != 0) {
            fprintf(stderr, "[THREAD] main context detach: %s\n", SDL_GetError());
            free_slots();return false;
        }
        int error = pthread_create(&worker, NULL, consume, NULL);
        if (error) {
            fprintf(stderr, "[THREAD] pthread_create: %s\n", strerror(error));
            SDL_GL_MakeCurrent(window, context);
            free_slots();return false;
        }
        metadata_lock();
        while (!attached) metadata_wait();
        bool failed = worker_failed;
        pthread_mutex_unlock(&mutex);
        if (failed) {
            pthread_join(worker, NULL);
            SDL_GL_MakeCurrent(window, context);
            free_slots();return false;
        }
#ifdef RR_PARALLEL_FRAME_PREP
        if(parallel){
            int error=pthread_create(&prep_worker,NULL,prepare,NULL);
            if(error){
                fprintf(stderr,"[PREP] pthread_create: %s\n",strerror(error));
                metadata_lock();closing=true;pthread_cond_broadcast(&changed);pthread_mutex_unlock(&mutex);
                pthread_join(worker,NULL);
                SDL_GL_MakeCurrent(window,context);
                free_slots();return false;
            }
            prep_started=true;
            fprintf(stderr,"[PREP] slots=%d fixed_hud_bytes=%zu queue=latest\n",SLOTS,
                    rr_video_layers_gpu()&&!rr_video_layers_verify()?0:(size_t)SLOTS*2*640*480*4);
        }
#endif
    }
    started = true;
    cpu_last = stamp(CLOCK_THREAD_CPUTIME_ID);
    wall_last = now();
    return true;
}

bool rr_presentation_video(uint32_t frame, bool slave)
{
    double p1_sim_end=now();
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
    if (!rr_gles3_pump()) return false;
    bool selected=true; /* Presentation-paced FIFO; no catch-up/resync/drop. */

    /* Every emulated video boundary, whether or not a picture is published. */
    palette_tick();
    bool walk = rr_scene_frame(slave);
    unsigned direct = rr_scene_direct_count();
    rr_gles3_scene_tick(walk, direct);
    if(direct>RR_PRESENT_DIRECT_MAX)gles3_unsupported(frame,
      (g_rr.wram[0x8da0]<<8)|g_rr.wram[0x8da1],(g_rr.wram[0xa32e]<<8)|g_rr.wram[0xa32f],
      -1,direct,NULL,"snapshot direct capacity");

    double p1_block=0;
    metadata_lock();
    last_frame = frame;
    bool failed = worker_failed;
    int slot = -1;
    if (selected && !failed) {
        for (int i = 0; i < SLOTS; i++)
            if (state[i] == FREE) { slot = i; break; }
        while(slot<0&&!worker_failed){double b=now();metadata_wait();p1_block+=now()-b;for(int i=0;i<SLOTS;i++)if(state[i]==FREE){slot=i;break;}}
        if (slot >= 0) {
            if (state[slot] == READY) discard_ready(slot);
            state[slot] = WRITING;
        } else {
#ifdef RR_PARALLEL_FRAME_PREP
            if(parallel){dropped++;if(frame>=first)range_dropped++;}
#endif
        }
    }
    pthread_mutex_unlock(&mutex);

    if (slot >= 0) {
        double copy_cpu = stamp(CLOCK_THREAD_CPUTIME_ID);
        rr_presentation *snapshot = &slots[slot];
        snapshot->p1_sim_ms=(p1_sim_end-wall_last)*1000;snapshot->p1_block_ms=p1_block*1000;
        snapshot->p1_audio_ratio=rr_p1_audio_ratio();rr_audio_output_stats p1_audio;rr_audio_output_status(&p1_audio);snapshot->p1_underruns=p1_audio.underruns;
        snapshot->g1_main=(g_rr.wram[0x8da0]<<8)|g_rr.wram[0x8da1];snapshot->g1_sub=(g_rr.wram[0xa32e]<<8)|g_rr.wram[0xa32f];snapshot->g1_screen=(g_rr.wram[0xa332]<<8)|g_rr.wram[0xa333];snapshot->g1_course=(g_rr.wram[0xa046]<<8)|g_rr.wram[0xa047];
        snapshot->g1b_major=(g_rr.wram[0xa048]<<8)|g_rr.wram[0xa049];snapshot->gles3_tag=0; snapshot->frame = frame; snapshot->walk = walk;
        snapshot->direct_count = direct; snapshot->produced = now();
        memcpy(snapshot->poly, g_rr.poly, sizeof snapshot->poly);
        memcpy(snapshot->pal, g_rr.pal, sizeof snapshot->pal);
        memcpy(snapshot->mixer, g_rr.mixer, sizeof snapshot->mixer);
        memcpy(snapshot->czram, g_rr.czram, sizeof snapshot->czram);
        memcpy(snapshot->cgram,g_rr.cgram,sizeof snapshot->cgram);
        memcpy(snapshot->text,g_rr.text,sizeof snapshot->text);
        memcpy(snapshot->tilemapattr,g_rr.tilemapattr,sizeof snapshot->tilemapattr);
        memcpy(snapshot->generation, generation, sizeof generation);
        snapshot->point_valid = rr_dsp_copy_point(snapshot->point);
        for (unsigned i = 0; i < direct; i++)
            memcpy(snapshot->direct[i], rr_scene_direct(i), sizeof snapshot->direct[i]);
        if (frame >= first) copy_sum += stamp(CLOCK_THREAD_CPUTIME_ID) - copy_cpu;

        metadata_lock();
#ifdef RR_PARALLEL_FRAME_PREP
        snapshot->ready_at=now();
#endif
        state[slot] = READY;
        unsigned depth = 0;
        for (int i = 0; i < SLOTS; i++) depth += state[i] == READY || state[i] == PREPARED;
        unsigned occupied=0;for(int i=0;i<SLOTS;i++)occupied+=state[i]!=FREE;snapshot->p1_depth=occupied;
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
#ifdef RR_PARALLEL_FRAME_PREP
        if(prep_started){pthread_join(prep_worker,NULL);prep_started=false;}
#endif
        pthread_join(worker, NULL);
        if (SDL_GL_MakeCurrent(window, context) != 0) {
            fprintf(stderr, "[THREAD] cleanup context: %s\n", SDL_GetError());
            worker_failed = true;
        }
    }
    bool ok = !worker_failed;
    if(profile_csv) {
        if(ferror(profile_csv))ok=false;
        if(fclose(profile_csv)){perror("thread profile CSV close");ok=false;}
        profile_csv=NULL;
    }
    /* At most two READY plus one READING can survive the final producer tick.
     * All later completions are in this bounded tail, independent of CSV overflow. */
    unsigned drain = 0;
    for (unsigned i = 0; i < (render_count < SLOTS ? render_count : SLOTS); i++)
        drain += tail_end[i] > sim_end;
    unsigned completed = render_count - drain;
    double elapsed = sim_end - sim_start;
    if (steps && elapsed > 0) {
        double overlap = (cpu_sum + producer_cpu + render_cpu
#ifdef RR_PARALLEL_FRAME_PREP
            + prep_cpu_sum
#endif
            ) / elapsed;
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
#ifdef RR_PARALLEL_FRAME_PREP
    if(parallel)fprintf(stderr,"[PREP-RATE] prepared=%u prep_cpu_ms=%.6f hud_cpu_ms=%.6f scene_wall_ms=%.6f dropped_prepared=%u extra_hud_bytes=%zu\n",
        prepared_count,prepared_count?prep_cpu_sum*1000/prepared_count:0,
        prepared_count?hud_cpu_sum*1000/prepared_count:0,
        prepared_count?prep_wall_sum*1000/prepared_count:0,dropped_prepared,
        (size_t)SLOTS*2*640*480*4);
#ifdef RR_PARALLEL_PREP_PROFILE
    if(parallel)fprintf(stderr,"[PREP-STAGES] prepared=%u direct_cpu_ms=%.6f list_cpu_ms=%.6f sort_cpu_ms=%.6f\n",
        prepared_count,prepared_count?direct_cpu_sum*1000/prepared_count:0,
        prepared_count?list_cpu_sum*1000/prepared_count:0,
        prepared_count?sort_cpu_sum*1000/prepared_count:0);
#endif
#endif
    if(sync_profile)fprintf(stderr,"[THREAD-SYNC] scope=whole-run producer_calls=%u producer_acquire_ms=%.6f producer_acquire_max_us=%.6f producer_cond_wait_ms=%.6f consumer_calls=%u consumer_acquire_ms=%.6f consumer_acquire_max_us=%.6f consumer_idle_ms=%.6f\n",
        producer_sync.calls,producer_sync.acquire*1000,producer_sync.maximum*1e6,producer_sync.idle*1000,
        consumer_sync.calls,consumer_sync.acquire*1000,consumer_sync.maximum*1e6,consumer_sync.idle*1000);
#ifdef RR_PARALLEL_FRAME_PREP
    if(sync_profile&&parallel)fprintf(stderr,"[PREP-SYNC] calls=%u acquire_ms=%.6f acquire_max_us=%.6f idle_ms=%.6f\n",
        preparer_sync.calls,preparer_sync.acquire*1000,preparer_sync.maximum*1e6,
        preparer_sync.idle*1000);
#endif
    free_slots();
    started = false;
    return ok;
}

/* Called by the consumer after swapping; metadata lock never covers GL. */
void rr_presentation_overlay(unsigned *frame,unsigned *drops,unsigned *queued,double *age_ms,double timestamp)
{
    metadata_lock();
    *frame=last_frame; *drops=dropped; *queued=0;
    for(int i=0;i<SLOTS;i++){
        if(state[i]==READY||state[i]==PREPARED)(*queued)++;
        if(state[i]==READING)*age_ms=(timestamp-slots[i].produced)*1000.;
    }
    pthread_mutex_unlock(&mutex);
}

