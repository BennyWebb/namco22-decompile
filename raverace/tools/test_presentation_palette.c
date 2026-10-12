/* Test the actual producer ledger against the original palette conversion.
 * Function-section GC drops unrelated worker/SDL entry points, not this logic.
 * cc -O2 -ffunction-sections -fdata-sections -DRR_THREADED_PRESENTATION
 *   -Iraverace/include -Iengine $(pkg-config --cflags sdl2)
 *   raverace/tools/test_presentation_palette.c engine/eng.c
 *   -Wl,--gc-sections -o /tmp/test-presentation-palette
 */
#define _GNU_SOURCE
#include <assert.h>
#include "../src/rr_presentation.c"
#include "eng.h"
rr_sys_t g_rr;
static rr_presentation retained;
static void tick(void)
{
    palette_tick();
    eng_palette_from_planar(g_rr.pal, 0x8000);
    assert(!memcmp(generation, g_eng_pal_gen, sizeof generation));
}
int main(void)
{
    tick();
    /* All groups, all planes, boundary entries; pending pictures may be dropped. */
    for (unsigned g = 0; g < 128; g++) {
        for (unsigned p = 0; p < 3; p++) {
            g_rr.pal[p * 0x8000 + g * 256] = (uint8_t)(g + 1);
            g_rr.pal[p * 0x8000 + g * 256 + 255] = (uint8_t)(g + 2);
        }
        tick();
    }
    memcpy(retained.pal, g_rr.pal, sizeof retained.pal);
    memcpy(retained.generation, generation, sizeof generation);
    unsigned entry = 42 * 256 + 37;
    uint16_t old = generation[42];
    g_rr.pal[entry] = 123; tick();
    g_rr.pal[entry] = 0; tick();
    assert(generation[42] == (uint16_t)(old + 2));
    assert(retained.pal[entry] == 0 && retained.generation[42] == old);
    tick();
    assert(generation[42] == (uint16_t)(old + 2));
    /* Preserve the original uint16 generation wrap, not a new key scheme. */
    for (unsigned i = 0; i < 65536; i++) { g_rr.pal[entry] ^= 1; tick(); }
    assert(generation[42] == (uint16_t)(old + 2));
    puts("Palette planes/groups/edges, unchanged ticks, retained snapshot, change/revert and uint16 wrap match original conversion");
}
