/* Compare tile-run decoding with the original per-texel addressing/lookup.
 * Covers every orientation/cmode, tile-edge guards, U wrap, skipped tiles,
 * invalid ROM tiles, mapping bounds, palette alpha and tile16 board behaviour. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define TEXTURE_TOTAL_SIZE 65536u
static uint8_t pixels[TEXTURE_TOTAL_SIZE], map[0x280000];
static uint8_t *g_texture_data = pixels, *g_texture_tilemap = map;
static int g_eng_tex_tile16;
static int get_tile_attr(int idx)
{
    int a = 0x200000 + (idx >> 1);
    return a < 0x280000 ? (idx & 1 ? map[a] & 15 : map[a] >> 4) : 0;
}
#include "texture_tile_runs.inc"
static unsigned ref_fetch(int u, int vrow, int y)
{
    u &= 0xfff;
    int idx = vrow | ((u & 0xff0) >> 4), off = idx * 2;
    unsigned tile = 0, attr = 0;
    if (off + 1 < 0x200000) {
        tile = map[off] | map[off + 1] << 8;
        attr = get_tile_attr(idx);
        if ((attr & 1) && !g_eng_tex_tile16)
            tile |= 0x10000;
    }
    int x = u & 15;
    if (attr & 4)
        x = 15 - x;
    if (attr & 2)
        y = 15 - y;
    if (attr & 8) {
        int t = x;
        x = y;
        y = t;
    }
    unsigned po = tile * 256 + y * 16 + x;
    return po < TEXTURE_TOTAL_SIZE ? pixels[po] : 0;
}
int main(void)
{
    for (unsigned i = 0; i < TEXTURE_TOTAL_SIZE; i++)
        pixels[i] = (i * 37 + (i >> 8) * 13) & 255;
    for (unsigned i = 0; i < 0x100000; i++) {
        unsigned tile = (i * 17) % 300;
        map[i * 2] = tile;
        map[i * 2 + 1] = tile >> 8;
    }
    uint8_t lut[256][4], out[1025 * 4];
    for (unsigned i = 0; i < 256; i++) {
        lut[i][0] = i;
        lut[i][1] = i ^ 57;
        lut[i][2] = 255 - i;
        lut[i][3] = (i % 11) ? 255 : 0;
    }
    int starts[] = {0, 1, 7, 15, 16, 4090, 4095, 4096, 8191};
    int steps[] = {1, 2, 3, 7, 15, 16, 17, 31, 257};
    unsigned long tests = 0;
    for (g_eng_tex_tile16 = 0; g_eng_tex_tile16 < 2; g_eng_tex_tile16++)
        for (unsigned attr = 0; attr < 16; attr++) {
            memset(map + 0x200000, attr | (attr << 4), 0x80000);
            for (int y = 0; y < 16; y++)
                for (int mode = 0; mode < 16; mode++) {
                    int offset, shift, mask;
                    if (mode & 4) {
                        offset = 0xec + ((mode & 8) << 1);
                        mask = 3;
                        shift = 2 * (~mode & 3);
                    } else if (mode & 2) {
                        offset = 0xe0 + ((mode & 8) << 1);
                        mask = 15;
                        shift = 4 * (~mode & 1);
                    } else {
                        offset = 0;
                        mask = 255;
                        shift = 0;
                    }
                    for (unsigned a = 0; a < sizeof starts / sizeof *starts; a++)
                        for (unsigned b = 0; b < sizeof steps / sizeof *steps; b++) {
                            int width = (a * 13 + b * 17) % 70 + 1, vrow = (y % 3) * 256;
                            texture_decode_tile_runs(
                                out, width, starts[a], steps[b], vrow, y, lut, offset, shift, mask);
                            for (int x = 0; x < width; x++) {
                                unsigned pen =
                                    (offset + ((ref_fetch(starts[a] + x * steps[b], vrow, y) >> shift) & mask)) & 255;
                                assert(!memcmp(out + x * 4, lut[pen], 4));
                            }
                            tests++;
                        }
                }
        }
    /* Change the orientation on every adjacent tile, including across U wrap. */
    for (unsigned idx = 0; idx < 0x100000; idx += 2)
        map[0x200000 + (idx >> 1)] = ((idx * 7 & 15) << 4) | ((idx * 11 + 3) & 15);
    for (g_eng_tex_tile16 = 0; g_eng_tex_tile16 < 2; g_eng_tex_tile16++)
        for (int y = 0; y < 16; y++)
            for (unsigned a = 0; a < sizeof starts / sizeof *starts; a++)
                for (unsigned b = 0; b < sizeof steps / sizeof *steps; b++) {
                    int width = 1025, vrow = (y % 3) * 256;
                    texture_decode_tile_runs(out, width, starts[a], steps[b], vrow, y, lut, 0, 0, 255);
                    for (int x = 0; x < width; x++)
                        assert(!memcmp(out + x * 4, lut[ref_fetch(starts[a] + x * steps[b], vrow, y)], 4));
                    tests++;
                }
    texture_decode_tile_runs(out, 1025, 4095, 1, 0x100000, 15, lut, 0, 0, 255);
    for (int x = 0; x < 1025; x++)
        assert(!memcmp(out + x * 4, lut[ref_fetch(4095 + x, 0x100000, 15)], 4));
    printf("%lu sampled row cases plus max-width/invalid-map guard passed\n", tests);
}
