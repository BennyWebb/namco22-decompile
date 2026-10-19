#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define CHECK3(x) assert(x)
static unsigned live_nt=21783;static bool c1_index=true;
static int g1_uncached(const int32_t*r){return r[0]^r[1]^r[2]^r[3]^r[4]^r[5]^r[6];}
static int g1c_rgba_material(const int32_t*r){return g1_uncached(r);}
#include "material_cache.h"
int main(void){g1c_init();for(int n=0;n<200000;n++){int32_t k[]={n%16,n%128,(n/16)%16,n%4096,(n*13)%4096,n%67+1,n%146+1};assert(live_material(k)==g1_uncached(k));assert(live_material(k)==g1_uncached(k));assert(g1c_occupancy<=g1c_capacity/2&&g1c_occupancy<=G1C_POOL);}
 int32_t k[7]={0};uint32_t hash=2166136261u;for(int j=0;j<7;j++)hash*=16777619u;g1c_next();for(unsigned j=0;j<64;j++){g1c_pool[j].recipe[0]=1;g1c_cells[(hash+j)&(g1c_capacity-1)]=(g1c_generation<<16)|(j+1);}g1c_occupancy=64;unsigned g=g1c_generation;assert(live_material(k)==0);assert(g1c_generation==g+1&&g1c_full==1);g1c_generation=65535;g1c_occupancy=G1C_POOL;assert(live_material(k)==0);assert(g1c_generation==1);printf("PASS lookups=%llu full=%llu max_probe=%u generations=%u exact-key collisions, pool reset and generation wrap\n",(unsigned long long)g1c_lookups,(unsigned long long)g1c_full,g1c_maxprobe,g1c_generation);return 0;}
