/* Isolated OBJECT support. All timestamps/samples retained; no simulation linked. */
#include <stdarg.h>
#include <math.h>
static int gles3_front, gles3_ram;
static char trace[16 * 1024 * 1024];
static size_t trace_used;
static double trace_wall, trace_cpu, check_wall, check_cpu, end_cpu;
static struct rusage end_usage;
static double gles3_stamp(clockid_t k)
{
    struct timespec t;
    clock_gettime(k, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}
static int gles3_printf(const char *fmt, ...)
{
    double wall = gles3_stamp(CLOCK_MONOTONIC), cpu = gles3_stamp(CLOCK_THREAD_CPUTIME_ID);
    va_list args;
    va_start(args, fmt);
    int n;
    if (gles3_ram) {
        size_t left = sizeof trace - trace_used;
        n = vsnprintf(trace + trace_used, left, fmt, args);
        if (n < 0 || (size_t)n >= left) {
            fprintf(stderr, "bounded trace exhausted\n");
            exit(2);
        }
        trace_used += (size_t)n;
    } else
        n = vfprintf(stdout, fmt, args);
    va_end(args);
    trace_wall += gles3_stamp(CLOCK_MONOTONIC) - wall;
    trace_cpu += gles3_stamp(CLOCK_THREAD_CPUTIME_ID) - cpu;
    return n;
}
static void gles3_flush(void)
{
    double start = gles3_stamp(CLOCK_MONOTONIC);
    if (gles3_ram) {
        if (fwrite(trace, 1, trace_used, stdout) != trace_used)
            exit(2);
    }
    if (fflush(stdout))
        exit(2);
    fprintf(stderr,
            "TRACE_DRAIN mode=%s bytes=%zu wall_ms=%.6f outside_race=1\n",
            gles3_ram ? "ram" : "sync",
            trace_used,
            (gles3_stamp(CLOCK_MONOTONIC) - start) * 1000);
}
#define printf gles3_printf
