"""G1 private ROM-only pack, exact-key lookup, immutable RAM activation receipts."""
import re

def replace(s,a,b):
 assert a in s,a[:150]
 return s.replace(a,b)
def apply(name,s,root,a):
 if name=='rr_presentation.h':return replace(s,'    uint64_t c1_revision[128];','    uint16_t g1_main,g1_sub,g1_screen,g1_course;unsigned g1_reason;\n    uint64_t c1_revision[128];')
 if name=='presentation.c':
  return replace(s,'snapshot->native_tag=0; snapshot->frame', '''snapshot->g1_main=(g_rr.wram[0x8da0]<<8)|g_rr.wram[0x8da1];snapshot->g1_sub=(g_rr.wram[0xa32e]<<8)|g_rr.wram[0xa32f];snapshot->g1_screen=(g_rr.wram[0xa332]<<8)|g_rr.wram[0xa333];snapshot->g1_course=(g_rr.wram[0xa046]<<8)|g_rr.wram[0xa047];
        snapshot->native_tag=0; snapshot->frame''')
 if name=='async_io.c':
  s=s.replace('c1_sem[4207]','c1_sem[8192]').replace('c1_nt>=4207','c1_nt>=8192');return s
 if name!='live.c':return s
 s=s.replace('#define MAXM3 8192','#define MAXM3 65535').replace('c1_group[8192]','c1_group[MAXM3]').replace('h[2]<=32768&&h[3]<=8192','h[2]<=131072&&h[3]<=MAXM3')
 # Same exact-key hash table as P2, revision-independent in index mode.
 start=s.index('static int live_material(');end=s.index('static int live_mesh(',start)
 orig=s[start:end];index=orig[orig.index('if(c1_index){')+len('if(c1_index){'):orig.index('}uint32_t hash=')]
 s=s[:start]+'static int g1_uncached(const int32_t*recipe){'+index+'}\n'+orig[0:orig.index('if(c1_index){')]+orig[orig.index('uint32_t hash='):].replace('if(e->revision==p2_revision[recipe[1]])','if(c1_index||e->revision==p2_revision[recipe[1]])').replace('int material=p2_material_uncached(recipe);','int material=c1_index?g1_uncached(recipe):p2_material_uncached(recipe);')+s[end:]
 # Loaded only in validation, never an input to ROM asset generation.
 s=replace(s,'static LiveMesh *live_meshes;', 'static int32_t*g1_ref_mesh,*g1_ref_material;static unsigned g1_ref_mesh_n;\nstatic LiveMesh *live_meshes;')
 if a.validation:
  s=replace(s,'for(unsigned k=0;k<n;k++)c1_copy[k].page=0;', '''for(unsigned k=0;k<n;k++){c1_copy[k].page=0;if(live_frame>=1800&&g1_ref_material){int material=c1_copy[k].q.material;P1_VALID(g1_ref_material[material]>=0);c1_copy[k].q.material=g1_ref_material[material];int mesh=c1_copy[k].q.mesh;if(mesh>=0){P1_VALID((unsigned)mesh<g1_ref_mesh_n&&g1_ref_mesh[mesh]>=0);c1_copy[k].q.mesh=g1_ref_mesh[mesh];}}}''')
 s=replace(s,'p1_diagnostic=getenv("ENG_P1_FENCE")!=NULL;', '''const char*identity=getenv("ENG_G1_REFERENCE_IDENTITY");if(identity){FILE*rf=fopen(identity,"rb");CHECK3(rf);uint32_t rh[2];read3(rf,rh,8);CHECK3(rh[1]==live_nt);g1_ref_mesh_n=rh[0];g1_ref_mesh=malloc(rh[0]*4);g1_ref_material=malloc(rh[1]*4);CHECK3(g1_ref_mesh&&g1_ref_material);read3(rf,g1_ref_mesh,rh[0]*4);read3(rf,g1_ref_material,rh[1]*4);CHECK3(fgetc(rf)==EOF&&!fclose(rf));}p1_diagnostic=getenv("ENG_P1_FENCE")!=NULL;''')
 # Warm only from currently compiled, slot-owned native commands; no recording read.
 start=s.index('const char*wp=getenv("ENG_P1B_WARM_PACKET")');end=s.index('glGenTextures(1,&p1b_color)',start);s=s[:start]+s[end:]
 s=replace(s,'static double p1_ring_wait;', '''static double p1_ring_wait;static unsigned g1_warm_candidates;static bool g1_audit;static unsigned g1_native,g1_fallback;
static void g1_receipt(rr_presentation*s,const char*why){fprintf(stderr,"[G1-COVERAGE] tick=%u main=%u sub=%u screen=%u course=%u walk=%u direct=%u native=%u reason=%s\\n",s->frame,s->g1_main,s->g1_sub,s->g1_screen,s->g1_course,s->walk,s->direct_count,s->native_tag,why);}
''')
 s=replace(s,'if(!live_loaded||s->frame<live_from)return true;', '''g1_audit=getenv("ENG_G1_STATE_AUDIT")!=NULL;
 if(!live_loaded||(!g1_audit&&(s->g1_main==0||s->g1_main>5))||(g1_audit&&s->frame<live_from)){g1_receipt(s,"boot-state");return true;}''')
 s=replace(s,'p3_native_projection=false;p1_fallbacks++;return true;', 'p3_native_projection=false;p1_fallbacks++;g1_fallback++;g1_receipt(s,p1_reason);return true;')
 s=replace(s,'rr_p1b_packet(s->frame,((Slot3*)s->native_payload)->bytes,((Slot3*)s->native_payload)->size);s->native_tag=1;', '''if(s->frame>=1800)rr_p1b_packet(s->frame,((Slot3*)s->native_payload)->bytes,((Slot3*)s->native_payload)->size);s->native_tag=1;
 if(!g1_audit&&g1_warm_candidates<3){s->native_tag=0;s->p1b_warm=1;g1_warm_candidates++;g1_receipt(s,"prewarm");}else{g1_native++;g1_receipt(s,"covered");}''')
 s=replace(s,'rr_c1b_semantic(live_frame,c1_copy,n*sizeof *live_cmd);','if(live_frame>=1800)rr_c1b_semantic(live_frame,c1_copy,n*sizeof *live_cmd);') if a.validation else s
 s=replace(s,'if(!s||s->frame+60<live_from||s->frame>=live_from||p1b_warmed>=3)return;', 'if(!s||!s->p1b_warm||p1b_warmed>=3)return;')
 s=replace(s,'draw3(&p1b_warm_slot,-1,"-",t,0);','draw3(s->native_payload,-1,"-",t,0);')
 return s
