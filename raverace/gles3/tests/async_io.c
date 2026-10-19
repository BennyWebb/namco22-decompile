/* Diagnostic-only bounded stdio transport. Game threads write memory cookies;
 * only rr-writer opens/writes persistent output. No writer owns a game FILE lock.
 * Checkpoint streams are reserved until shutdown, never dropped. */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <sys/resource.h>
#include <time.h>
/* One-shot SHA256 ABI from the toolchain OpenSSL sha.h; no opaque context layout. */
extern unsigned char *SHA256(const unsigned char *, size_t, unsigned char *);
#define POOL (16u * 1024 * 1024)
#define STREAMS 32
static unsigned char pool[POOL];
static size_t used;
typedef struct {
    char path[4096];
    unsigned char *data;
    size_t cap;
    _Atomic size_t head, tail;
    _Atomic unsigned drops, drop_bytes;
    _Atomic int ready, closed;
    int fd, flags, checkpoint;
} Stream;
static Stream streams[STREAMS];
static unsigned count;
static pthread_t writer;
static _Atomic int stopping, errors;
static int out_fd, err_fd, summary_fd;
static int idle_result, nice_result;
static unsigned packet_count, pixel_count;
static struct {
    unsigned tick;
    char suffix[16];
    unsigned char digest[32];
} pixels[64];
static struct {
    unsigned tick, bytes;
    unsigned char digest[32], normalized[32];
    unsigned normalized_bytes;
} packets[8192];
FILE *__real_fopen(const char *, const char *);
FILE *__real_fopen64(const char *, const char *);
static void output(Stream *s)
{
    size_t tail = atomic_load_explicit(&s->tail, memory_order_relaxed),
           head = atomic_load_explicit(&s->head, memory_order_acquire);
    if (s->checkpoint && !atomic_load(&stopping))
        return;
    if (head == tail) {
        if (atomic_load(&s->closed) && s->fd >= 0) {
            close(s->fd);
            s->fd = -2;
        }
        return;
    }
    if (s->fd == -1) {
        s->fd = open(s->path, s->flags, 0644);
        if (s->fd < 0) {
            atomic_fetch_add(&errors, 1);
            s->fd = -2;
        }
    }
    while (tail < head) {
        size_t pos = tail % s->cap, n = head - tail;
        if (n > s->cap - pos)
            n = s->cap - pos;
        if (s->fd >= 0) {
            ssize_t w = write(s->fd, s->data + pos, n);
            if (w < 0 && errno == EINTR)
                continue;
            if (w <= 0) {
                atomic_fetch_add(&errors, 1);
                s->fd = -2;
            } else
                n = (size_t)w;
        }
        tail += n;
        atomic_store_explicit(&s->tail, tail, memory_order_release);
    }
}
static void *drain(void *unused)
{
    (void)unused;
    pthread_setname_np(pthread_self(), "rr-writer");
    struct sched_param sp = {0};
    idle_result = pthread_setschedparam(pthread_self(), SCHED_IDLE, &sp);
    nice_result = setpriority(PRIO_PROCESS, 0, 19);
    do {
        for (unsigned i = 0; i < STREAMS; i++)
            if (atomic_load_explicit(&streams[i].ready, memory_order_acquire))
                output(streams + i);
        if (!atomic_load(&stopping)) {
            struct timespec t = {0, 10000000};
            nanosleep(&t, NULL);
        }
    } while (!atomic_load(&stopping));
    for (unsigned i = 0; i < STREAMS; i++)
        if (atomic_load(&streams[i].ready)) {
            output(streams + i);
            if (streams[i].fd >= 0) {
                if (close(streams[i].fd))
                    atomic_fetch_add(&errors, 1);
                streams[i].fd = -2;
            }
        }
    return NULL;
}
static ssize_t append(void *cookie, const char *p, size_t n)
{
    Stream *s = cookie;
    size_t h = atomic_load_explicit(&s->head, memory_order_relaxed),
           t = s->checkpoint ? 0 : atomic_load_explicit(&s->tail, memory_order_acquire);
    if (n > s->cap - (h - t)) {
        atomic_fetch_add(&s->drops, 1);
        atomic_fetch_add(&s->drop_bytes, n);
        if (s->checkpoint)
            atomic_fetch_add(&errors, 1);
        return n;
    }
    size_t pos = h % s->cap, k = n;
    if (k > s->cap - pos)
        k = s->cap - pos;
    memcpy(s->data + pos, p, k);
    memcpy(s->data, p + k, n - k);
    atomic_store_explicit(&s->head, h + n, memory_order_release);
    return n;
}
static int retire(void *cookie)
{
    atomic_store(&((Stream *)cookie)->closed, 1);
    return 0;
}
static FILE *memory_file(const char *path, const char *mode, int fd)
{
    if (count == STREAMS) {
        errno = ENOSPC;
        atomic_fetch_add(&errors, 1);
        return NULL;
    }
    Stream *s = streams + count++;
    memset(s, 0, sizeof *s);
    snprintf(s->path, sizeof s->path, "%s", path);
    s->fd = fd;
    s->flags = O_WRONLY | O_CREAT | (strchr(mode, 'a') ? O_APPEND : O_TRUNC) | (strchr(mode, 'x') ? O_EXCL : 0);
    s->checkpoint = strstr(path, "states.jsonl") != NULL;
    s->cap = strstr(path, "thread.csv")                   ? 8u * 1024 * 1024
             : strstr(path, "ticks.csv") || s->checkpoint ? 256u * 1024
                                                          : 1024u * 1024;
    if (used + s->cap > POOL) {
        errno = ENOSPC;
        atomic_fetch_add(&errors, 1);
        return NULL;
    }
    s->data = pool + used;
    used += s->cap;
    cookie_io_functions_t ops = {.write = append, .close = retire};
    FILE *f = fopencookie(s, "w", ops);
    if (!f)
        return NULL;
    atomic_store_explicit(&s->ready, 1, memory_order_release);
    return f;
}
FILE *__wrap_fopen(const char *p, const char *m)
{
    return strchr(m, 'w') || strchr(m, 'a') ? memory_file(p, m, -1) : __real_fopen(p, m);
}
FILE *__wrap_fopen64(const char *p, const char *m)
{
    return strchr(m, 'w') || strchr(m, 'a') ? memory_file(p, m, -1) : __real_fopen64(p, m);
}
void rr_p1b_packet(unsigned tick, const void *bytes, size_t n)
{
    if (packet_count >= 8192) {
        atomic_fetch_add(&errors, 1);
        return;
    }
    unsigned i = packet_count++;
    packets[i].tick = tick;
    packets[i].bytes = n;
    SHA256(bytes, n, packets[i].digest);
    /* Packet v3: preserve every byte except group boundaries; expand draw state
     * per visible command, so different state or order cannot pass this gate. */
    if (getenv("ENG_GLES3_NORMALIZED_RECEIPTS")) {
        const uint32_t *h = bytes;
        size_t at = 60 + h[6] * 80 + h[3] * 32 + ((h[4] * 2 + 3) & ~3u), tail = at + h[5] * 32;
        if (n < 60 || h[0] != 0x33504e52 || h[1] != 3 || tail > n)
            abort();
        size_t size = at + h[4] * 24 + n - tail;
        unsigned char *out = malloc(size);
        if (!out)
            abort();
        memcpy(out, bytes, at);
        memset(out + 20, 0, 4);
        size_t pos = at;
        unsigned accounted = 0;
        for (unsigned g = 0; g < h[5]; g++) {
            const uint32_t *v = (const uint32_t *)((const unsigned char *)bytes + at + g * 32);
            if (v[0] != accounted || !v[1] || v[1] > h[4] - accounted)
                abort();
            for (unsigned j = 0; j < v[1]; j++) {
                memcpy(out + pos, v + 2, 24);
                pos += 24;
            }
            accounted += v[1];
        }
        if (accounted != h[4])
            abort();
        memcpy(out + pos, (const unsigned char *)bytes + tail, n - tail);
        SHA256(out, size, packets[i].normalized);
        packets[i].normalized_bytes = size;
        free(out);
    }
}
static void c1e_colour(unsigned, const char *, const void *, size_t);
void rr_p1b_pixel(unsigned tick, const char *suffix, const void *bytes, size_t n)
{
    if (pixel_count >= 64) {
        atomic_fetch_add(&errors, 1);
        return;
    }
    unsigned i = pixel_count++;
    pixels[i].tick = tick;
    snprintf(pixels[i].suffix, 16, "%s", suffix);
    SHA256(bytes, n, pixels[i].digest);
    if (getenv("ENG_GLES3_IMAGES"))
        c1e_colour(tick, suffix, bytes, n);
}
static unsigned c1_nt;
static struct {
    unsigned tick;
    size_t n;
    unsigned char hash[32];
} c1_sem[8192];
void rr_c1b_semantic(unsigned t, const void *p, size_t n)
{
    if (c1_nt >= 8192)
        abort();
    unsigned i = c1_nt++;
    c1_sem[i].tick = t;
    c1_sem[i].n = n;
    SHA256(p, n, c1_sem[i].hash);
}
/* Validation only. Bounded transport; rr-present only copies to memory. */
#define C1E_SLOTS 4
#define C1E_BYTES (640 * 480 * 24)
typedef struct {
    unsigned kind, tick;
    char suffix[16];
    uint8_t bytes[C1E_BYTES];
} C1eJob;
static C1eJob c1e_jobs[C1E_SLOTS];
static _Atomic unsigned c1e_head, c1e_tail, c1e_stop, c1e_drops, c1e_errors;
static pthread_t c1e_writer;
static bool c1e_started;
static void *c1e_drain(void *unused)
{
    (void)unused;
    pthread_setname_np(pthread_self(), "c1e-capture");
    struct sched_param sp = {0};
    pthread_setschedparam(pthread_self(), SCHED_IDLE, &sp);
    setpriority(PRIO_PROCESS, 0, 19);
    for (;;) {
        unsigned t = atomic_load_explicit(&c1e_tail, memory_order_relaxed),
                 h = atomic_load_explicit(&c1e_head, memory_order_acquire);
        if (t == h) {
            if (atomic_load(&c1e_stop))
                break;
            struct timespec d = {0, 2000000};
            nanosleep(&d, NULL);
            continue;
        }
        C1eJob *j = c1e_jobs + (t % C1E_SLOTS);
        const char *dir = getenv(j->kind == 1 ? "ENG_GLES3_IMAGES" : "ENG_GLES3_PROVENANCE_DIR");
        for (unsigned k = 0; k < (j->kind == 1 ? 1 : 3); k++) {
            char path[4096];
            const char *ext = j->kind == 1 ? j->suffix
                              : k == 0     ? "provenance.u32"
                              : k == 1     ? "proof.rgba"
                                           : "texel.rgba";
            snprintf(path, sizeof path, j->kind == 1 ? "%s/tick-%u.%s.rgba" : "%s/tick-%u.%s", dir, j->tick, ext);
            size_t offset = j->kind == 1 ? 0 : k == 0 ? 0 : k == 1 ? 640 * 480 * 16 : 640 * 480 * 20;
            size_t n = j->kind == 1 || k ? 640 * 480 * 4 : 640 * 480 * 16;
            FILE *f = __real_fopen(path, "wbx");
            if (!f) {
                atomic_fetch_add(&c1e_errors, 1);
                continue;
            }
            if (fwrite(j->bytes + offset, 1, n, f) != n || fclose(f))
                atomic_fetch_add(&c1e_errors, 1);
        }
        atomic_store_explicit(&c1e_tail, t + 1, memory_order_release);
    }
    return NULL;
}
static C1eJob *c1e_job(unsigned kind, unsigned tick, const char *suffix)
{
    if (!c1e_started) {
        c1e_started = true;
        if (pthread_create(&c1e_writer, NULL, c1e_drain, NULL))
            abort();
    }
    unsigned h = atomic_load_explicit(&c1e_head, memory_order_relaxed),
             t = atomic_load_explicit(&c1e_tail, memory_order_acquire);
    if (h - t == C1E_SLOTS) {
        atomic_fetch_add(&c1e_drops, 1);
        return NULL;
    }
    C1eJob *j = c1e_jobs + (h % C1E_SLOTS);
    j->kind = kind;
    j->tick = tick;
    if (suffix)
        snprintf(j->suffix, sizeof j->suffix, "%s", suffix);
    return j;
}
static void c1e_colour(unsigned tick, const char *suffix, const void *p, size_t n)
{
    C1eJob *j = c1e_job(1, tick, suffix);
    if (j) {
        memcpy(j->bytes, p, n);
        atomic_fetch_add_explicit(&c1e_head, 1, memory_order_release);
    }
}
void rr_c1d_frame(unsigned tick, const void *m, const void *c, const void *r)
{
    C1eJob *j = c1e_job(2, tick, NULL);
    if (j) {
        memcpy(j->bytes, m, 640 * 480 * 16);
        memcpy(j->bytes + 640 * 480 * 16, c, 640 * 480 * 4);
        memcpy(j->bytes + 640 * 480 * 20, r, 640 * 480 * 4);
        atomic_fetch_add_explicit(&c1e_head, 1, memory_order_release);
    }
}
static void c1e_capture_finish(void)
{
    if (c1e_started) {
        atomic_store(&c1e_stop, 1);
        pthread_join(c1e_writer, NULL);
    }
    dprintf(summary_fd,
            "[C1E-CAPTURE] frames=%u drops=%u errors=%u slots=%u bytes=%u fully_drained=1\n",
            atomic_load(&c1e_head),
            atomic_load(&c1e_drops),
            atomic_load(&c1e_errors),
            C1E_SLOTS,
            C1E_SLOTS * C1E_BYTES);
    if (atomic_load(&c1e_errors) || atomic_load(&c1e_drops))
        _exit(96);
}
static void finish(void)
{
    c1e_capture_finish();
    const char *path = getenv("ENG_GLES3_RECEIPTS");
    if (path) {
        FILE *f = __wrap_fopen(path, "wx");
        if (f) {
            fputs("tick,bytes,sha256\n", f);
            for (unsigned i = 0; i < packet_count; i++) {
                fprintf(f, "%u,%u,", packets[i].tick, packets[i].bytes);
                for (int j = 0; j < 32; j++)
                    fprintf(f, "%02x", packets[i].digest[j]);
                fputc('\n', f);
            }
            fclose(f);
        } else
            atomic_fetch_add(&errors, 1);
    }

    const char *norm = getenv("ENG_GLES3_NORMALIZED_RECEIPTS");
    if (norm) {
        FILE *f = __wrap_fopen(norm, "wx");
        if (!f)
            abort();
        fputs("tick,bytes,sha256\n", f);
        for (unsigned i = 0; i < packet_count; i++) {
            fprintf(f, "%u,%u,", packets[i].tick, packets[i].normalized_bytes);
            for (int j = 0; j < 32; j++)
                fprintf(f, "%02x", packets[i].normalized[j]);
            fputc(10, f);
        }
        fclose(f);
    }
    const char *sem = getenv("ENG_GLES3_SEMANTIC");
    if (sem) {
        FILE *f = __wrap_fopen(sem, "wx");
        if (!f)
            abort();
        fprintf(f, "tick,bytes,sha256\n");
        for (unsigned i = 0; i < c1_nt; i++) {
            fprintf(f, "%u,%zu,", c1_sem[i].tick, c1_sem[i].n);
            for (int j = 0; j < 32; j++)
                fprintf(f, "%02x", c1_sem[i].hash[j]);
            fputc(10, f);
        }
        fclose(f);
    }
    const char *image = getenv("ENG_GLES3_IMAGE_HASH");
    if (image) {
        FILE *f = __wrap_fopen(image, "wx");
        if (f) {
            fputs("tick,suffix,sha256\n", f);
            for (unsigned i = 0; i < pixel_count; i++) {
                fprintf(f, "%u,%s,", pixels[i].tick, pixels[i].suffix);
                for (int j = 0; j < 32; j++)
                    fprintf(f, "%02x", pixels[i].digest[j]);
                fputc('\n', f);
            }
            fclose(f);
        } else
            atomic_fetch_add(&errors, 1);
    }
    fflush(NULL);
    atomic_store(&stopping, 1);
    pthread_join(writer, NULL);
    unsigned drops = 0, bytes = 0;
    for (unsigned i = 0; i < count; i++) {
        drops += atomic_load(&streams[i].drops);
        bytes += atomic_load(&streams[i].drop_bytes);
    }
    dprintf(summary_fd,
            "[P1B-IO] streams=%u reserved=%zu dropped_records=%u dropped_bytes=%u errors=%d packets=%u "
            "writer_idle_result=%d nice_result=%d fully_drained=1\n",
            count,
            used,
            drops,
            bytes,
            atomic_load(&errors),
            packet_count,
            idle_result,
            nice_result);
    if (atomic_load(&errors))
        _exit(95);
}
__attribute__((constructor)) static void init(void)
{
    for (size_t i = 0; i < POOL; i += 4096)
        ((volatile unsigned char *)pool)[i] = 0;
    out_fd = dup(1);
    err_fd = dup(2);
    summary_fd = dup(2);
    unsigned char warm[32];
    SHA256((const unsigned char *)"warm", 4, warm);
    stdout = memory_file("<stdout>", "w", out_fd);
    stderr = memory_file("<stderr>", "w", err_fd);
    if (!stdout || !stderr)
        _exit(95);
    static char ob[65536], eb[65536];
    setvbuf(stdout, ob, _IOFBF, sizeof ob);
    setvbuf(stderr, eb, _IOFBF, sizeof eb);
    if (pthread_create(&writer, NULL, drain, NULL))
        _exit(95);
    atexit(finish);
    struct timespec mt, rt;
    clock_gettime(CLOCK_MONOTONIC, &mt);
    clock_gettime(CLOCK_REALTIME, &rt);
    fprintf(stderr,
            "[P1B-CLOCK] monotonic=%.9f realtime=%.9f\n",
            mt.tv_sec + mt.tv_nsec * 1e-9,
            rt.tv_sec + rt.tv_nsec * 1e-9);
}
