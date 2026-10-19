#define G1C_POOL 65535u
#define G1C_PROBES 64u
typedef struct {int32_t recipe[7];int material;} G1CKey;
static G1CKey g1c_pool[G1C_POOL];static uint32_t*g1c_cells;
static unsigned g1c_capacity,g1c_occupancy,g1c_maxprobe,g1c_clears,g1c_generation=1;
static uint64_t g1c_lookups,g1c_probes,g1c_full;
static void g1c_init(void){uint64_t combinations=G1C_POOL;g1c_capacity=1;while(g1c_capacity<combinations*2){CHECK3(g1c_capacity<(1u<<30));g1c_capacity*=2;}g1c_cells=calloc(g1c_capacity,4);CHECK3(g1c_cells);fprintf(stderr,"[G1C-INIT] catalogue=%u sizing=bounded-pool combinations=%llu capacity=%u cell_bytes=%llu key_bytes=%llu load_limit=0.5 pool_limit=%u probe_limit=%u\n",live_nt,(unsigned long long)combinations,g1c_capacity,(unsigned long long)g1c_capacity*4,(unsigned long long)sizeof g1c_pool,G1C_POOL,G1C_PROBES);}
static void g1c_next(void){g1c_clears++;g1c_occupancy=0;if(++g1c_generation==65536){memset(g1c_cells,0,(size_t)g1c_capacity*4);g1c_generation=1;}}
static int live_material(const int32_t*recipe){

 uint32_t hash=2166136261u;for(unsigned i=0;i<7;i++)hash=(hash^(uint32_t)recipe[i])*16777619u;
 g1c_lookups++;uint32_t*cell=NULL;unsigned limit=g1c_capacity/2;if(limit>G1C_POOL)limit=G1C_POOL;
 if(g1c_occupancy==limit)g1c_next();
 for(unsigned probe=0;probe<G1C_PROBES;probe++){g1c_probes++;if(probe+1>g1c_maxprobe)g1c_maxprobe=probe+1;uint32_t*c=g1c_cells+((hash+probe)&(g1c_capacity-1));if((*c>>16)!=g1c_generation){cell=c;break;}G1CKey*k=g1c_pool+((*c&65535)-1);if(!memcmp(k->recipe,recipe,28))return k->material;}
 if(!cell){g1c_full++;g1c_next();cell=g1c_cells+(hash&(g1c_capacity-1));}
 int material=g1_uncached(recipe);G1CKey*k=g1c_pool+g1c_occupancy;memcpy(k->recipe,recipe,28);k->material=material;g1c_occupancy++;*cell=(g1c_generation<<16)|g1c_occupancy;return material;
}
