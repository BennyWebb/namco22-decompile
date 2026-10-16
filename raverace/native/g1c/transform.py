from pathlib import Path
import importlib.util,re
spec=importlib.util.spec_from_file_location('g1b_base',Path(__file__).parents[1]/'g1b/transform.py');base=importlib.util.module_from_spec(spec);spec.loader.exec_module(base)
def apply(name,s,root,a):
 here=Path(__file__).parent
 s=base.apply(name,s,root,a)
 if name!='live.c':return s
 start=s.index('static int live_material(const int32_t *recipe)');end=s.index('static int live_mesh(',start)
 s=s[:start]+s[start:end].replace('static int live_material(', 'static int g1c_rgba_material(')+(here/'cache.inc').read_text()+s[end:]
 s=s.replace('live_nt=h[3];','live_nt=h[3];g1c_init();')
 s=s.replace('static void g1_receipt(rr_presentation*s,const char*why){','static void g1_receipt(rr_presentation*s,const char*why){fprintf(stderr,"[G1C-CACHE] tick=%u occupancy=%u lookups=%llu probes=%llu max_probe=%u full=%llu clears=%u generation=%u capacity=%u pool_limit=65535\\n",s->frame,g1c_occupancy,(unsigned long long)g1c_lookups,(unsigned long long)g1c_probes,g1c_maxprobe,(unsigned long long)g1c_full,g1c_clears,g1c_generation,g1c_capacity);g1c_clears=0;g1c_lookups=g1c_probes=g1c_full=g1c_maxprobe=0;')
 s=s.replace('int key[]={r[0],r[2],r[3],r[4],r[5],r[6]};','int key[]={r[0],r[2],r[3],r[4],r[5],r[6]};if(getenv("ENG_G1C_KEYS"))fprintf(stderr,"[G1C-KEY] tick=%u key=%d,%d,%d,%d,%d,%d\\n",live_frame,key[0],key[1],key[2],key[3],key[4],key[5]);')
 # Larger source-coalesced page: exact logical recipes, shared ROM-aligned allocation.
 lo=s.index('/* CPU allocator owns');hi=s.index('static G1BSlot*g1b_slot;',lo)
 h=s[lo:hi].replace('#define G1B_SIDE 1024','#define G1B_SIDE 3072').replace('#define G1B_BYTES (1024*1024)','#define G1B_BYTES (G1B_SIDE*G1B_SIDE)').replace('128*128','384*384').replace('*128','*384').replace('<=128','<=384').replace('*1024','*G1B_SIDE').replace('>1020','>G1B_SIDE-4')
 s=s[:lo]+h+s[hi:]
 lo=s.index('static int g1b_material(const int*r)');hi=s.index('static void g1b_upload(',lo)
 body=s[lo:hi].replace('int key[]={r[0],r[2],r[3],r[4],r[5],r[6]};','if(r[3]<0||r[3]>4095||r[4]<0||r[4]>4095||r[5]<1||r[6]<1||r[5]>1020||r[6]>1020)live_fail("dynamic footprint dimensions/capacity",-1,-1);int key[]={r[0],r[2],(r[3]/256)*256,(r[4]/256)*256,0,0};key[4]=((r[3]+r[5]+255)/256)*256-key[2];key[5]=((r[4]+r[6]+255)/256)*256-key[3];').replace('1024.','(double)G1B_SIDE').replace('1024','G1B_SIDE').replace('e->x+2.5-r[3]','e->x+2.5-e->key[2]').replace('e->y+2.5-r[4]','e->y+2.5-e->key[3]')
 s=s[:lo]+body+s[hi:]
 s=s.replace('heights3[g1b_page_index]=1024','heights3[g1b_page_index]=G1B_SIDE').replace('GL_R8UI,1024,1024,0','GL_R8UI,G1B_SIDE,G1B_SIDE,0').replace('dynamic_page=1024','dynamic_page=3072 source_blocks=256')
 s=s.replace('p->x+2,p->y+2,m->recipe[5]','p->x+2+m->recipe[3]-p->key[2],p->y+2+m->recipe[4]-p->key[3],m->recipe[5]')
 return s
