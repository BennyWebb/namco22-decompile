/* Lifted execution only. The shared memory/runtime expose checker seams, but
 * this target never links readable replacements or arms checker transactions.
 * Keep the ABI and fail loudly if a checker-only callback is ever reached. */
#include "rr_mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int rd_on, rd_stop_on, rd_poll_rec, rd_cov_on;
int rd_read_log, rd_quiet, rd_io_written, rd_io_touched, rd_ior_replay;
static void checker_unavailable(const char *operation)
{
    fprintf(stderr, "GLES3: unexpected checker operation: %s\n", operation);
    abort();
}
void rd_init(void)
{
    const char *mode = getenv("RR_RD");
    if (!mode || strcmp(mode, "0"))
        checker_unavailable("readable replacements requested");
}
void rd_budget_out(void)
{ /* No checker probe exists in this composition. */
}
void rd_poll_snap(void)
{
    checker_unavailable(__func__);
}
void rd_cov_ins(uint32_t pc)
{
    (void)pc;
    checker_unavailable(__func__);
}
int rd_hook(uint32_t pc)
{
    (void)pc;
    checker_unavailable(__func__);
    return 0;
}
int rd_jump_stop(uint32_t target, uint32_t at)
{
    (void)target;
    (void)at;
    checker_unavailable(__func__);
    return 0;
}
void rd_io_log(vaddr_t a, int size, uint32_t v)
{
    (void)a;
    (void)size;
    (void)v;
    checker_unavailable(__func__);
}
void rd_ior_log(vaddr_t a, int size, uint32_t v)
{
    (void)a;
    (void)size;
    (void)v;
    checker_unavailable(__func__);
}
uint32_t rd_ior_serve(vaddr_t a, int size)
{
    (void)a;
    (void)size;
    checker_unavailable(__func__);
    return 0;
}
void rd_read_byte(vaddr_t a, uint8_t *p)
{
    (void)a;
    (void)p;
    checker_unavailable(__func__);
}
void rd_dict_byte(uint8_t v)
{
    (void)v;
    checker_unavailable(__func__);
}
void rd_journal_byte(vaddr_t a, uint8_t *p)
{
    (void)a;
    (void)p;
    checker_unavailable(__func__);
}
