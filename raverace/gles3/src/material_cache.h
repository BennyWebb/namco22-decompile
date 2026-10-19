#define MATERIAL_CACHE_POOL 65535u
#define MATERIAL_CACHE_PROBES 64u
typedef struct {
    int32_t recipe[7];
    int material;
} MaterialCacheKey;
static MaterialCacheKey material_cache_pool[MATERIAL_CACHE_POOL];
static uint32_t *material_cache_cells;
static unsigned material_cache_capacity, material_cache_occupancy, material_cache_maxprobe, material_cache_clears,
    material_cache_generation = 1;
static uint64_t material_cache_lookups, material_cache_probes, material_cache_full;
static void material_cache_init(void)
{
    uint64_t combinations = MATERIAL_CACHE_POOL;
    material_cache_capacity = 1;
    while (material_cache_capacity < combinations * 2) {
        RENDER_CHECK(material_cache_capacity < (1u << 30));
        material_cache_capacity *= 2;
    }
    material_cache_cells = calloc(material_cache_capacity, 4);
    RENDER_CHECK(material_cache_cells);
    fprintf(stderr,
            "[MATERIAL_CACHE-INIT] catalogue=%u sizing=bounded-pool combinations=%llu capacity=%u cell_bytes=%llu "
            "key_bytes=%llu "
            "load_limit=0.5 pool_limit=%u probe_limit=%u\n",
            live_nt,
            (unsigned long long)combinations,
            material_cache_capacity,
            (unsigned long long)material_cache_capacity * 4,
            (unsigned long long)sizeof material_cache_pool,
            MATERIAL_CACHE_POOL,
            MATERIAL_CACHE_PROBES);
}
static void material_cache_next(void)
{
    material_cache_clears++;
    material_cache_occupancy = 0;
    if (++material_cache_generation == 65536) {
        memset(material_cache_cells, 0, (size_t)material_cache_capacity * 4);
        material_cache_generation = 1;
    }
}
static int live_material(const int32_t *recipe)
{

    uint32_t hash = 2166136261u;
    for (unsigned i = 0; i < 7; i++)
        hash = (hash ^ (uint32_t)recipe[i]) * 16777619u;
    material_cache_lookups++;
    uint32_t *cell = NULL;
    unsigned limit = material_cache_capacity / 2;
    if (limit > MATERIAL_CACHE_POOL)
        limit = MATERIAL_CACHE_POOL;
    if (material_cache_occupancy == limit)
        material_cache_next();
    for (unsigned probe = 0; probe < MATERIAL_CACHE_PROBES; probe++) {
        material_cache_probes++;
        if (probe + 1 > material_cache_maxprobe)
            material_cache_maxprobe = probe + 1;
        uint32_t *c = material_cache_cells + ((hash + probe) & (material_cache_capacity - 1));
        if ((*c >> 16) != material_cache_generation) {
            cell = c;
            break;
        }
        MaterialCacheKey *k = material_cache_pool + ((*c & 65535) - 1);
        if (!memcmp(k->recipe, recipe, 28))
            return k->material;
    }
    if (!cell) {
        material_cache_full++;
        material_cache_next();
        cell = material_cache_cells + (hash & (material_cache_capacity - 1));
    }
    int material = state_uncached(recipe);
    MaterialCacheKey *k = material_cache_pool + material_cache_occupancy;
    memcpy(k->recipe, recipe, 28);
    k->material = material;
    material_cache_occupancy++;
    *cell = (material_cache_generation << 16) | material_cache_occupancy;
    return material;
}
