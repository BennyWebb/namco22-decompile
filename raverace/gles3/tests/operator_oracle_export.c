/* Host-test capture only. Runs the unmodified lifted TEST/SERVICE program;
 * recording and seed metadata are checked by the normal replay opener. */
#include "rr_mem.h"
#include <stdio.h>
#include <stdlib.h>

static void capture(void)
{
    const char *path = getenv("OPERATOR_ORACLE_OUTPUT");
    if (!path) {
        fprintf(stderr, "[OPERATOR-ORACLE] missing output path\n");
        _Exit(2);
    }
    FILE *f = fopen(path, "wx");
    int ok = f && fwrite(g_rr.eeprom, 1, sizeof g_rr.eeprom, f) == sizeof g_rr.eeprom;
    if (f && fclose(f))
        ok = 0;
    if (!ok) {
        fprintf(stderr, "[OPERATOR-ORACLE] capture failed\n");
        _Exit(2);
    }
}

__attribute__((constructor)) static void install_capture(void)
{
    if (atexit(capture))
        _Exit(2);
}
