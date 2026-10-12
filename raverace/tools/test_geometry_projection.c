/* Compile twice, with/without RR_GEOMETRY_REUSE_PROJECTION; compare stdout.
 * Exercise the actual decoder, including all raw/near/guard metadata. */
#include "../../engine/geo_hw.c"
static int32_t points[32];
const int32_t *g_eng_pointrom = points;
uint32_t g_eng_pointrom_n = 32;
static unsigned callbacks;
static int32_t dynamic_read(uint32_t addr) { callbacks++; return points[addr & 31]; }
eng_point_ext_fn g_eng_pointram = dynamic_read;
static void output(const geo_quad *q, void *unused) {
    (void)unused;
    if (fwrite(q, sizeof *q, 1, stdout) != 1) exit(1);
}
static uint32_t rng = 0x12345678;
static uint32_t next(void) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
int main(void) {
    for (unsigned test = 0; test < 10000; test++) {
        geo_view view = {0};
        for (int a = 0; a < 3; a++) for (int b = 0; b < 3; b++)
            view.m[a][b] = test < 32 ? (a == b ? 32768 : 0) : (int)(next()%65537)-32768;
        view.zoom_mant = test < 32 ? 1920 : (int)(next()%65536)-32768;
        view.zoom_shift = next()%47;
        view.vx = (int)(next()%129)-64; view.vy = (int)(next()%129)-64;
        view.have_clip = test&1; view.cl = -320; view.cr = -320; view.cu = -240; view.cd = -240;
        view.cullflip = test&1; view.objectflags = test&3; view.absolute_priority = test&7;
        view.t[0] = (int)(next()%4097)-2048;
        view.t[1] = (int)(next()%4097)-2048;
        view.t[2] = test < 32 ? 0 : (int)(next()%100001)-50000;
        geo_hw_set_view(&view);
        for (int i = 0; i < 8; i++) points[i] = next() & 0xffffff;
        for (int i = 0; i < 4; i++) {
            points[8+3*i] = (int)(next()%100001)-50000;
            points[9+3*i] = (int)(next()%100001)-50000;
            points[10+3*i] = test < 32 ? ((test & (1<<i)) ? 1 : -(int)(test&1)) : (int)(next()%100001)-50000;
        }
        /* Lighting consumption is stateful; compare that as well. */
        g_lit_n = test%9; g_lit_idx = 0;
        for (int i = 0; i < g_lit_n; i++) g_lit[i] = next()%256;
        quad_fixed(0x7f1234, (test&1) ? 32 : 0, test%4==0 ? 0x200001 : 0,
                   ((test&3)<<8) | ((test&4) ? 0x20 : 0), test&0x40, output, NULL);
        fwrite(&g_lit_idx, sizeof g_lit_idx, 1, stdout);
    }
    fwrite(&callbacks, sizeof callbacks, 1, stdout);
    fwrite(&g_geo_stats, sizeof g_geo_stats, 1, stdout);
    return ferror(stdout) ? 1 : 0;
}
