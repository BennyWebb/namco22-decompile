from pathlib import Path
import importlib.util,re
spec=importlib.util.spec_from_file_location('g1b_base',Path(__file__).parents[1]/'g1b/transform.py');base=importlib.util.module_from_spec(spec);spec.loader.exec_module(base)
def apply(name,s,root,a):
 s=base.apply(name,s,root,a)
 if name!='live.c':return s
 start=s.index('static int live_material(const int32_t *recipe)');end=s.index('static int live_mesh(',start)
 s=s[:start]+'''static unsigned g1c_occupancy,g1c_maxprobe;static uint64_t g1c_lookups,g1c_probes,g1c_full;
static int live_material(const int32_t *recipe){uint32_t hash=2166136261u;for(unsigned i=0;i<7;i++)hash=(hash^(uint32_t)recipe[i])*16777619u;P2Material*entry=NULL;g1c_lookups++;
for(unsigned probe=0;probe<32768;probe++){g1c_probes++;if(probe+1>g1c_maxprobe)g1c_maxprobe=probe+1;P2Material*e=p2_cache+((hash+probe)&32767);if(!e->occupied){entry=e;g1c_occupancy++;break;}if(!memcmp(e->recipe,recipe,28)){if(c1_index||e->revision==p2_revision[recipe[1]])return e->material;entry=e;break;}}
if(!entry)g1c_full++;int material=c1_index?g1_uncached(recipe):p2_material_uncached(recipe);if(entry){memcpy(entry->recipe,recipe,28);entry->revision=p2_revision[recipe[1]];entry->material=material;entry->occupied=1;}return material;}
'''+s[end:]
 s=s.replace('static void g1_receipt(rr_presentation*s,const char*why){','static void g1_receipt(rr_presentation*s,const char*why){fprintf(stderr,"[G1C-CACHE] tick=%u occupancy=%u lookups=%llu probes=%llu max_probe=%u full=%llu\\n",s->frame,g1c_occupancy,(unsigned long long)g1c_lookups,(unsigned long long)g1c_probes,g1c_maxprobe,(unsigned long long)g1c_full);g1c_lookups=g1c_probes=g1c_full=g1c_maxprobe=0;')
 s=s.replace('int key[]={r[0],r[2],r[3],r[4],r[5],r[6]};','int key[]={r[0],r[2],r[3],r[4],r[5],r[6]};if(getenv("ENG_G1C_KEYS"))fprintf(stderr,"[G1C-KEY] tick=%u key=%d,%d,%d,%d,%d,%d\\n",live_frame,key[0],key[1],key[2],key[3],key[4],key[5]);')
 return s
