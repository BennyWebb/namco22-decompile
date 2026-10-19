#define DYNAMIC_CACHE_SIDE 3072
#define DYNAMIC_CACHE_CELL 8
#define DYNAMIC_CACHE_ENTRIES 4096
#define DYNAMIC_CACHE_MATERIALS 512
#define DYNAMIC_CACHE_BYTES (DYNAMIC_CACHE_SIDE * DYNAMIC_CACHE_SIDE)
typedef struct {
    int key[6];
    unsigned x, y, w, h;
    uint64_t generation, last;
    bool occupied;
} DynamicCacheEntry;
typedef struct {
    unsigned entry, x, y, w, h, offset;
    uint64_t generation;
    int key[6];
} DynamicCacheUpload;
typedef struct {
    float uv[4];
    unsigned group, upload;
    int recipe[7];
} DynamicCacheMaterial;
typedef struct {
    unsigned tick, uploads, materials, bytes, hits, misses, evictions;
    DynamicCacheUpload upload[DYNAMIC_CACHE_MATERIALS];
    DynamicCacheMaterial material[DYNAMIC_CACHE_MATERIALS];
    uint8_t pens[DYNAMIC_CACHE_BYTES];
    unsigned prior_uploads;
    bool reuse_prefix;
    uint64_t copied_bytes, metadata_bytes;
} DynamicCacheSlot;
