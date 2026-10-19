/* GLES3 board assets and immutable GPU-HUD input. No world renderer/cache. */
#include "renderer_api.h"
#include "rr_game.h"
#include "rr_gl.h"
#include "rr_dsp.h"
#include "rr_video_layers.h"
#include "eng.h"
#include <stdio.h>
#include <string.h>
static uint8_t gamma_prom[3][256];
static _Thread_local const rr_presentation *selected;
static int32_t point_read(uint32_t a)
{
    a &= 0xffffff;
    if (selected)
        return selected->point_valid && a >= 0xf00000 && a < 0xf20000
                   ? (int32_t)(selected->point[a - 0xf00000] << 8) >> 8
                   : -1;
    return rr_dsp_pointram_read(a);
}
bool rr_gl_init(const char *dir)
{
    if (!eng_load_texture_roms(dir, g_rr_game->cg, g_rr_game->ccrl, g_rr_game->ccrh))
        return false;
    if (g_rr_game->tex_fixup)
        eng_texture_tilemap_sys22_fixup();
    for (unsigned c = 0; c < 3; c++) {
        char p[1024];
        snprintf(p, sizeof p, "%s/%s", dir, g_rr_game->gamma[c]);
        FILE *f = fopen(p, "rb");
        if (!f)
            return false;
        bool ok = fread(gamma_prom[c], 1, 256, f) == 256 && fgetc(f) == EOF;
        fclose(f);
        if (!ok)
            return false;
    }
    g_eng_pointrom = g_pointrom;
    g_eng_pointrom_n = g_pointrom_words;
    g_eng_pointram = point_read;
    return g_eng_pointrom != NULL;
}
void rr_gl_p1_select(const rr_presentation *s)
{
    selected = s;
}
void rr_gl_presentation_release(void)
{
    selected = NULL;
}
bool rr_gl_video_prepare(rr_video_cpu_frame *out)
{
    const rr_presentation *s = selected;
    if (!s)
        return false;
    out->characters = s->cgram;
    out->tilemap = s->text;
    out->scroll_x = ((s->tilemapattr[0] << 8 | s->tilemapattr[1]) - 0x35c) & 1023;
    out->scroll_y = (s->tilemapattr[2] << 8 | s->tilemapattr[3]) & 1023;
    out->shadow_enabled = s->mixer[0] & 1;
    unsigned base = (s->mixer[7] << 8) & 0x7f00;
    for (unsigned i = 0; i < 256; i++) {
        for (unsigned c = 0; c < 3; c++)
            out->palette[i][c] = s->pal[base + i + c * 32768];
        out->palette[i][3] = 255;
    }
    for (unsigned k = 0; k < 3; k++)
        for (unsigned c = 0; c < 3; c++)
            out->multipliers[k][c] = s->mixer[8 + k * 3 + c];
    for (unsigned v = 0; v < 256; v++) {
        for (unsigned c = 0; c < 3; c++) {
            unsigned fade = s->mixer[0x11 + c * 2] << 8 | s->mixer[0x12 + c * 2];
            unsigned value = v;
            if (fade > 256 && !value)
                value = 1;
            value = value * fade >> 8;
            if (value > 255)
                value = 255;
            out->lut[v][c] = gamma_prom[c][value];
        }
        out->lut[v][3] = 255;
    }
    return true;
}
bool rr_gl_video_submit(const rr_video_cpu_frame *f)
{
    return rr_video_layers_compose_gpu(f);
}
/* Scalar ROM addressing, extracted from the board's specified tile format.
 * This decodes pens only; no texture cache, baking, GL allocation or substitute. */
uint8_t gles3_texture_pen(int u, int v, int bank)
{
    u &= 4095;
    v = (v & 4095) | (bank * 4096);
    unsigned ix = ((v & 0xfff0) << 4) | ((u & 0xff0) >> 4);
    unsigned off = ix * 2;
    if (off + 1 >= 0x200000)
        gles3_unsupported(0, 0, 0, bank, ix, NULL, "tile address bounds");
    unsigned attr = (g_texture_tilemap[0x200000 + (ix >> 1)] >> ((ix & 1) ? 0 : 4)) & 15;
    unsigned tile = g_texture_tilemap[off] | g_texture_tilemap[off + 1] << 8;
    if ((attr & 1) && !g_eng_tex_tile16)
        tile |= 65536;
    unsigned x = u & 15, y = v & 15;
    if (attr & 4)
        x = 15 - x;
    if (attr & 2)
        y = 15 - y;
    if (attr & 8) {
        unsigned t = x;
        x = y;
        y = t;
    }
    off = tile * 256 + y * 16 + x;
    if (off >= TEXTURE_TOTAL_SIZE)
        gles3_unsupported(0, 0, 0, bank, ix, NULL, "pen ROM bounds");
    return g_texture_data[off];
}
