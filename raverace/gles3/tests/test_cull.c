#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct {
    int32_t meta[36], local[24], oracle[28];
} ObjectQuad;
static unsigned original(const ObjectQuad *r)
{
    double low[4][3], high[4][3], zoom = ldexp((double)r->meta[24], -r->meta[25]);
    for (int v = 0; v < 4; v++)
        for (int c = 0; c < 3; c++) {
            int64_t dot = 0;
            double absolute = 0;
            for (int k = 0; k < 3; k++) {
                int64_t term = (int64_t)r->local[v * 6 + k] * r->meta[12 + k * 3 + c];
                dot += term;
                absolute += fabs((double)term);
            }
            double tr = r->meta[21 + c], error = absolute / (32768. * 1048576.) + fabs(tr) / 1048576. + 2;
            double xyz = (double)(dot >> 15) + tr;
            low[v][c] = xyz - error;
            high[v][c] = xyz + error;
        }
    for (int v = 0; v < 4; v++)
        if (low[v][2] <= 1)
            return 0;
    bool outside[4] = {true, true, true, true};
    for (int v = 0; v < 4; v++) {
        double xl = 1e100, xh = -1e100, yl = 1e100, yh = -1e100;
        for (int a = 0; a < 2; a++)
            for (int b = 0; b < 2; b++) {
                double z = b ? high[v][2] : low[v][2], x = (a ? high[v][0] : low[v][0]) / z * zoom,
                       y = (a ? high[v][1] : low[v][1]) / z * zoom;
                xl = fmin(xl, x);
                xh = fmax(xh, x);
                yl = fmin(yl, y);
                yh = fmax(yh, y);
            }
        outside[0] &= r->meta[26] + xh < r->meta[28] - 4;
        outside[1] &= r->meta[26] + xl > r->meta[29] + 4;
        outside[2] &= r->meta[27] - yl < r->meta[30] - 4;
        outside[3] &= r->meta[27] - yh > r->meta[31] + 4;
    }
    for (unsigned i = 0; i < 4; i++)
        if (outside[i])
            return i + 1;
    return 0;
}
#include "culling.h"
static unsigned live_cull(const ObjectQuad *r)
{
    return cull_projected_bounds(r);
}
static uint64_t seed = 0x9852dbab147c6a31ULL;
static uint64_t next(void)
{
    seed ^= seed << 13;
    seed ^= seed >> 7;
    seed ^= seed << 17;
    return seed;
}
int main(void)
{
    unsigned counts[5] = {0};
    for (unsigned i = 0; i < 1000000; i++) {
        ObjectQuad r = {0};
        for (int k = 0; k < 36; k++)
            r.meta[k] = (int32_t)next();
        for (int k = 0; k < 24; k++)
            r.local[k] = (int32_t)(next() & 0xffffff) - 0x800000;
        r.meta[24] = (i % 7 == 0) ? 0 : (i % 7 == 1) ? -32768 : 32768;
        r.meta[25] = (i % 127 == 0) ? -2048 : (int)(next() % 256) - 128;
        for (int c = 0; c < 3; c++)
            r.meta[21 + c] = (int32_t)next();
        if (i % 3 == 0)
            r.meta[23] = (int32_t)(next() % 30000000) + 1;
        r.meta[26] = 320;
        r.meta[27] = 240;
        r.meta[28] = 0;
        r.meta[29] = 639;
        r.meta[30] = 0;
        r.meta[31] = 479;
        unsigned a = original(&r), b = live_cull(&r);
        if (a != b) {
            printf("FAIL %u %u %u\n", i, a, b);
            return 1;
        }
        counts[a]++;
    }
    printf("PASS random=1000000 seed=9852dbab147c6a31 reason_counts=%u,%u,%u,%u,%u\n",
           counts[0],
           counts[1],
           counts[2],
           counts[3],
           counts[4]);
}
