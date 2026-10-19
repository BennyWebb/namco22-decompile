/* Deliberately separate from headline profiling: two clocks per measured
 * region affect small loops. Disabled builds contain neither clocks nor counters.
 * Select an inclusive emulated-frame range with GEO_PROFILE_FROM/TO. */
#ifdef RR_GEOMETRY_PROJECTION_PROFILE
#include <time.h>
static unsigned gp_from = ~0u, gp_to;
typedef struct { int64_t base, v, z; int32_t mant, shift, negate, reserved; } gp_record;
static gp_record gp_records[32768];
static unsigned gp_records_n, gp_records_overflow;
static const char *gp_trace;
static uint64_t gp_transforms, gp_quads, gp_reusable, gp_raw_calls, gp_near_calls, gp_reads[2];
static double gp_transform, gp_raw, gp_near;
static double gp_clock(void) {
    struct timespec t;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}
__attribute__((constructor)) static void gp_init(void) {
    gp_trace = getenv("GEO_PROJECTION_TRACE");
    const char *s = getenv("GEO_PROFILE_FROM");
    if (s) { gp_from = strtoul(s, NULL, 10); gp_to = ~0u;
        s = getenv("GEO_PROFILE_TO"); if (s) gp_to = strtoul(s, NULL, 10); }
}
__attribute__((destructor)) static void gp_report(void) {
    if (gp_trace) {
        FILE *f = fopen(gp_trace, "wb");
        int ok = f && fwrite(gp_records, sizeof gp_records[0], gp_records_n, f) == gp_records_n;
        if (f && fclose(f)) ok = 0;
        fprintf(stderr, "[PROJECTION-TRACE] records=%u overflow=%u written=%d\n", gp_records_n, gp_records_overflow, ok);
    }
    if (gp_from == ~0u) return;
    double calibration = 0;
    for (int i = 0; i < 20000; i++) { double t = gp_clock(); calibration += gp_clock()-t; }
    fprintf(stderr, "[GEOMETRY-PROFILE-OVERHEAD] transforms=%llu clock_interval_ns=%.3f; timings include counter/clock perturbation\n", (unsigned long long)gp_transforms, calibration*1e9/20000);
    fprintf(stderr, "[GEOMETRY-PROJECTION] quads=%llu reusable=%llu raw_calls=%llu near_calls=%llu rom_reads=%llu dynamic_reads=%llu transform_cpu_ms=%.6f raw_cpu_ms=%.6f near_cpu_ms=%.6f\n",
        (unsigned long long)gp_quads, (unsigned long long)gp_reusable,
        (unsigned long long)gp_raw_calls, (unsigned long long)gp_near_calls,
        (unsigned long long)gp_reads[0], (unsigned long long)gp_reads[1],
        gp_transform*1000, gp_raw*1000, gp_near*1000);
}
#define GP_ACTIVE (g_eng_frame >= gp_from && g_eng_frame <= gp_to)
#endif
