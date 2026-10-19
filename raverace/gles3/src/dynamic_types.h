#define G1B_SIDE 3072
#define G1B_CELL 8
#define G1B_ENTRIES 4096
#define G1B_MATERIALS 512
#define G1B_BYTES (G1B_SIDE*G1B_SIDE)
typedef struct {int key[6];unsigned x,y,w,h;uint64_t generation,last;bool occupied;} G1BEntry;
typedef struct {unsigned entry,x,y,w,h,offset;uint64_t generation;int key[6];} G1BUpload;
typedef struct {float uv[4];unsigned group,upload;int recipe[7];} G1BMaterial;
typedef struct {unsigned tick,uploads,materials,bytes,hits,misses,evictions;G1BUpload upload[G1B_MATERIALS];G1BMaterial material[G1B_MATERIALS];uint8_t pens[G1B_BYTES];unsigned prior_uploads;bool reuse_prefix;uint64_t copied_bytes,metadata_bytes;} G1BSlot;
