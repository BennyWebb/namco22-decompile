#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <assert.h>
#include "dynamic_types.h"
#include "staging.h"
int main(void)
{
    DynamicCacheSlot *s[2] = {calloc(1, sizeof(DynamicCacheSlot)), calloc(1, sizeof(DynamicCacheSlot))};
    uint8_t *src = calloc(1, DYNAMIC_CACHE_BYTES),
            *expected[2] = {calloc(1, DYNAMIC_CACHE_BYTES), calloc(1, DYNAMIC_CACHE_BYTES)};
    unsigned lastbytes[2] = {0};
    uint64_t copied = 0, pending = 0;
    for (unsigned frame = 0; frame < 200; frame++) {
        unsigned at = frame % 2;
        assert(!memcmp(s[at]->pens, expected[at], lastbytes[at]));
        preparation_stage_begin(s[at]);
        for (unsigned u = 0; u < 3; u++) {
            unsigned id = (u + (frame / 20) % 3) % 3;
            unsigned gen = frame / 40 + 1, w = 16 + (frame / 20) % 2 * 7, h = 19;
            DynamicCacheUpload p = {
                .entry = id, .x = id * 100, .y = id * 128, .w = w, .h = h, .offset = s[at]->bytes, .generation = gen};
            for (unsigned y = 0; y < h; y++)
                for (unsigned x = 0; x < w; x++)
                    src[(p.y + y) * DYNAMIC_CACHE_SIDE + p.x + x] = (id + gen + x + y) & 255;
            preparation_stage(s[at], u, &p, src);
            for (unsigned y = 0; y < h; y++)
                memcpy(expected[at] + p.offset + y * w, src + (p.y + y) * DYNAMIC_CACHE_SIDE + p.x, w);
            s[at]->bytes += w * h;
            s[at]->uploads++;
        }
        assert(!memcmp(s[at]->pens, expected[at], s[at]->bytes));
        if (frame == 2)
            assert(s[at]->copied_bytes == 0);
        copied += s[at]->copied_bytes;
        pending += s[at]->bytes;
        lastbytes[at] = s[at]->bytes;
    }
    assert(copied < pending / 5);
    printf("PASS 200 interleaved pictures: layout/generation changes, immutable snapshots; copied=%llu pending=%llu\n",
           (unsigned long long)copied,
           (unsigned long long)pending);
    return 0;
}
