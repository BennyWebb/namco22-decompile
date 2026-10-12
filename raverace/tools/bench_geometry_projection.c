/* Replay actual frame-1500 projector inputs; not an FPS benchmark.
 * Compile without diagnostic/optimisation macros. No clock inside hot loop. */
#include <time.h>
#include "../../engine/geo_hw.c"
const int32_t *g_eng_pointrom;
uint32_t g_eng_pointrom_n;
eng_point_ext_fn g_eng_pointram;
typedef struct { int64_t base, v, z; int32_t mant, shift, negate, reserved; } record;
static record records[32768];
static double now(void) { struct timespec t; clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t); return t.tv_sec+t.tv_nsec*1e-9; }
int main(int argc, char **argv) {
    if (argc != 2) return 2;
    FILE *f = fopen(argv[1], "rb"); if (!f) return 2;
    size_t n = fread(records,sizeof records[0],32768,f);
    if (ferror(f) || !n || fgetc(f)!=EOF) return 2;
    fclose(f);
    volatile uint64_t checksum = 0;
    double start = now();
    for (int repeat = 0; repeat < 500; repeat++) {
        for (size_t i = 0; i < n; i++) {
            record *r = &records[i];
            checksum += (uint32_t)proj_sat(r->base,r->v,r->mant,r->shift,r->z,r->negate);
        }
    }
    double elapsed = now()-start;
    printf("records=%zu repeats=500 cpu_ms_per_frame=%.6f ns_per_call=%.3f checksum=%llu\n",n,elapsed*2,elapsed*1e9/(n*500),(unsigned long long)checksum);
    return 0;
}
