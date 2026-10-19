/* Memory-fetch test oracle, restored from the documented shared-semantics
 * execution contract. No generated instruction bytes or polling fast-forward. */
#include <stdlib.h>
#include "m377_sem.h"
static FILE *coverage;
static uint8_t seen[0x1000000];
/* Preserve RR_SNDCOV before the replay host isolates RR_ environment settings. */
__attribute__((constructor)) static void coverage_open(void)
{
    const char *p = getenv("RR_SNDCOV");
    if (!p) p = getenv("ENG_SNDCOV");
    if (p && *p) { coverage = fopen(p, "wx"); if (!coverage) { perror(p); exit(2); } }
}
int m37710_run(m37710_t *c, int cycles)
{
    uint64_t start = c->cycles, end = start + cycles;
    while (c->cycles < end && !c->unimpl_hit) {
        m37710_service(c);
        if (c->stopped) { c->cycles = end; break; }
        uint32_t pc = ((uint32_t)c->pg << 16) | c->pc;
        unsigned mx = (c->ps >> 4) & 3u;
        uint64_t before = c->fetches;
        c->ops = NULL;
        uint8_t op = fetch8(c);
        c->use_b = op == 0x42;
        if (c->use_b) op = fetch8(c);
        c->cycles++;
        m377_exec(c, op);
        if (coverage && !(seen[pc] & (1u << mx))) {
            seen[pc] |= 1u << mx;
            if (fprintf(coverage, "%06X %u %llu\n", pc, mx,
                        (unsigned long long)(c->fetches - before)) < 0) exit(2);
        }
    }
    return (int)(c->cycles - start);
}
