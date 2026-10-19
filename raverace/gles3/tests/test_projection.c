#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <inttypes.h>
#include "projection.h"
static int32_t reference(int64_t b, int64_t v, int32_t m, int s, int64_t z, int n)
{
    __int128 x = (__int128)((unsigned __int128)((__int128)v * m) << 4);
    x >>= s;
    __int128 q = z ? x / z : 0, r = (__int128)b + (n ? -q : q);
    return r > INT32_MAX ? INT32_MAX : r < INT32_MIN ? INT32_MIN : (int32_t)r;
}
static uint64_t rng = UINT64_C(0xc397a498dbf30297);
static uint64_t random64(void)
{
    rng ^= rng << 13;
    rng ^= rng >> 7;
    rng ^= rng << 17;
    return rng;
}
static uint64_t cases;
static void check(int64_t b, int64_t v, int32_t m, int s, int64_t z, int n)
{
    int32_t x = reference(b, v, m, s, z, n), y = project_saturated_exact(b, v, m, s, z, n);
    cases++;
    if (x != y) {
        fprintf(stderr,
                "Mismatch b=%" PRId64 " v=%" PRId64 " m=%d s=%d z=%" PRId64 " n=%d got=%d expected=%d\n",
                b,
                v,
                m,
                s,
                z,
                n,
                y,
                x);
        exit(1);
    }
}
int main(void)
{
    const int64_t edge[] = {INT64_MIN,
                            INT64_MIN + 1,
                            INT64_MIN / 16 - 1,
                            INT64_MIN / 16,
                            INT64_MIN / 16 + 1,
                            INT32_MIN - 1LL,
                            INT32_MIN,
                            -32769,
                            -32768,
                            -1,
                            0,
                            1,
                            32767,
                            32768,
                            INT32_MAX,
                            INT32_MAX + 1LL,
                            INT64_MAX / 16 - 1,
                            INT64_MAX / 16,
                            INT64_MAX / 16 + 1,
                            INT64_MAX - 1,
                            INT64_MAX};
    const int32_t mant[] = {INT32_MIN, INT32_MIN + 1, -65536, -32768, -16, -1, 0, 1, 16, 32767, 65536, INT32_MAX};
    /* Cartesian boundary classes x ALL128 defined shifts x both negate values. */
    for (unsigned b = 0; b < sizeof edge / sizeof *edge; b++)
        for (unsigned v = 0; v < sizeof edge / sizeof *edge; v++)
            for (unsigned z = 0; z < sizeof edge / sizeof *edge; z++)
                for (unsigned m = 0; m < sizeof mant / sizeof *mant; m++)
                    for (int s = 0; s < 128; s++)
                        for (int n = 0; n < 2; n++)
                            check(edge[b], edge[v], mant[m], s, edge[z], n);
    uint64_t boundary = cases;
    for (uint64_t i = 0; i < 100000000; i++) {
        int64_t b = (int64_t)random64(), v = (int64_t)random64(), z = (int64_t)random64();
        int32_t m = (int32_t)random64();
        int shift = random64() % 128, n = random64() & 1;
        /* Half stress normal geometry-size operands / fast paths; half full-width. */
        if (i & 1) {
            b = (int32_t)b;
            v = (int32_t)v;
            m = (int16_t)m;
            z = (int32_t)z;
            shift %= 32;
        }
        if (!(i % 127))
            z = 0;
        check(b, v, m, shift, z, n);
    }
    printf("PASS boundary=%" PRIu64 " random=100000000 total=%" PRIu64 " seed=c397a498dbf30297 shifts=0..127\n",
           boundary,
           cases);
}
