#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define RENDER_CHECK(x) assert(x)
static unsigned live_nt = 21783;
static int state_uncached(const int32_t *r)
{
    return r[0] ^ r[1] ^ r[2] ^ r[3] ^ r[4] ^ r[5] ^ r[6];
}
#include "material_cache.h"
int main(void)
{
    material_cache_init();
    for (int n = 0; n < 200000; n++) {
        int32_t k[] = {n % 16, n % 128, (n / 16) % 16, n % 4096, (n * 13) % 4096, n % 67 + 1, n % 146 + 1};
        assert(live_material(k) == state_uncached(k));
        assert(live_material(k) == state_uncached(k));
        assert(material_cache_occupancy <= material_cache_capacity / 2 &&
               material_cache_occupancy <= MATERIAL_CACHE_POOL);
    }
    int32_t k[7] = {0};
    uint32_t hash = 2166136261u;
    for (int j = 0; j < 7; j++)
        hash *= 16777619u;
    material_cache_next();
    for (unsigned j = 0; j < 64; j++) {
        material_cache_pool[j].recipe[0] = 1;
        material_cache_cells[(hash + j) & (material_cache_capacity - 1)] = (material_cache_generation << 16) | (j + 1);
    }
    material_cache_occupancy = 64;
    unsigned g = material_cache_generation;
    assert(live_material(k) == 0);
    assert(material_cache_generation == g + 1 && material_cache_full == 1);
    material_cache_generation = 65535;
    material_cache_occupancy = MATERIAL_CACHE_POOL;
    assert(live_material(k) == 0);
    assert(material_cache_generation == 1);
    printf("PASS lookups=%llu full=%llu max_probe=%u generations=%u exact-key collisions, pool reset and generation "
           "wrap\n",
           (unsigned long long)material_cache_lookups,
           (unsigned long long)material_cache_full,
           material_cache_maxprobe,
           material_cache_generation);
    return 0;
}
