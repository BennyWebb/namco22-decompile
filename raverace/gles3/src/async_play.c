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
    unsigned char digest[32];
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
    /* Diagnostic recordings stream checkpoints to disk through rr-writer too:
     * retaining a long recording must not hit the old 256KiB reserved limit. */
#ifdef GLES3_RECORD_DIAGNOSTICS
    s->checkpoint = 0;
#else
    s->checkpoint = strstr(path, "states.jsonl") != NULL;
#endif
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
void rr_p1b_packet(unsigned t, const void *p, size_t n)
{
    (void)t;
    (void)p;
    (void)n;
}
void rr_p1b_pixel(unsigned t, const char *s, const void *p, size_t n)
{
    (void)t;
    (void)s;
    (void)p;
    (void)n;
}
void rr_c1d_frame(unsigned t, const void *m, const void *c, const void *r)
{
    (void)t;
    (void)m;
    (void)c;
    (void)r;
}
static void finish(void)
{
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
