from pathlib import Path
import importlib.util
spec=importlib.util.spec_from_file_location('g1c_base',Path(__file__).parents[1]/'g1c/transform.py');base=importlib.util.module_from_spec(spec);spec.loader.exec_module(base)
def change(s,a,b):
 assert a in s,a[:160]
 return s.replace(a,b)
def apply(name,s,root,a):
 s=base.apply(name,s,root,a)
 if name=='gles.c':
  code=(root/'raverace/src/rr_gles2_input.inc').read_text().replace('void rr_gles2_apply_input(void)\n{','void rr_gles2_apply_input(void)\n{g2_save_tick();')
  s=change(s,'#include "rr_gles2_input.inc"','#include "rr_hw.h"\n'+Path(__file__).with_name('eeprom.inc').read_text()+'\n'+code)
  s=change(s,'bool rr_gles2_open(void)\n{','bool rr_gles2_open(void)\n{if(!g2_save_open(getenv("ENG_G2_SESSION")&&interactive))return false;')
  s=change(s,'static bool input_exit(void)\n{','static bool input_exit(void)\n{g2_save_emit();')
  return s
 if name!='live.c':return s
 s=change(s,'uint64_t combinations=(uint64_t)live_nt*128;','uint64_t combinations=G1C_POOL;').replace('catalogue=%u groups=128 combinations=','catalogue=%u sizing=bounded-pool combinations=')
 s=change(s,'uint8_t pens[G1B_BYTES];} G1BSlot;','uint8_t pens[G1B_BYTES];unsigned prior_uploads;bool reuse_prefix;uint64_t copied_bytes,metadata_bytes;} G1BSlot;')
 s=change(s,'static G1BSlot*g1b_slot;',Path(__file__).with_name('staging.inc').read_text()+'\nstatic G1BSlot*g1b_slot;')
 s=change(s,'memset(g1b_slot,0,offsetof(G1BSlot,pens));','g2_stage_begin(g1b_slot);')
 begin=s.index('G1BUpload*p=g1b_slot->upload+u;*p=');end=s.index('g1b_slot->bytes+=',begin)
 s=s[:begin]+'''G1BUpload next={.entry=id,.x=e->x,.y=e->y,.w=e->w,.h=e->h,.offset=g1b_slot->bytes,.generation=e->generation};memcpy(next.key,key,24);g2_stage(g1b_slot,u,&next,g1b_page);G1BUpload*p=g1b_slot->upload+u;'''+s[end:]
 begin=s.index('G1BMaterial*v=g1b_slot->material+m;');end=s.index('return material_count3+m;',begin)
 s=s[:begin]+'''G1BMaterial*v=g1b_slot->material+m,value={.uv={1.f/G1B_SIDE,1.f/G1B_SIDE,(e->x+2.5-e->key[2])/(double)G1B_SIDE,(e->y+2.5-e->key[3])/(double)G1B_SIDE},.group=r[1],.upload=u};memcpy(value.recipe,r,28);if(memcmp(v,&value,sizeof value)){*v=value;g1b_slot->metadata_bytes+=sizeof value;}'''+s[end:]
 s=change(s,'static void g1_receipt(rr_presentation*s,const char*why){','static void g1_receipt(rr_presentation*s,const char*why){if(s->g1b_dynamic&&((G1BSlot*)s->g1b_dynamic)->tick==s->frame){const G1BSlot*d=s->g1b_dynamic;fprintf(stderr,"[G2-STAGING] tick=%u copied_bytes=%llu metadata_bytes=%llu\\n",s->frame,(unsigned long long)d->copied_bytes,(unsigned long long)d->metadata_bytes);}')
 # UV resources follow the same existing per-copy fence; group attributes stay per picture.
 s=change(s,'static void g1b_upload(const rr_presentation*s,unsigned copy){','static float g2_uv[3][G1B_MATERIALS][4];static bool g2_uv_valid[3][G1B_MATERIALS];\nstatic void g1b_upload(const rr_presentation*s,unsigned copy){unsigned uv_bytes=0;')
 old='glBindTexture(GL_TEXTURE_2D,g1b_uv[copy]);glTexSubImage2D(GL_TEXTURE_2D,0,id%1024,id/1024,1,1,GL_RGBA,GL_FLOAT,m->uv);'
 s=change(s,old,'if(!g2_uv_valid[copy][j]||memcmp(g2_uv[copy][j],m->uv,16)){'+old+'memcpy(g2_uv[copy][j],m->uv,16);g2_uv_valid[copy][j]=true;uv_bytes+=16;}')
 s=change(s,'CHECK3(glGetError()==GL_NO_ERROR);glActiveTexture(GL_TEXTURE0);\n fprintf(stderr,"[G1B-UPLOAD]', 'fprintf(stderr,"[G2-UV] tick=%u bytes=%u\\n",s->frame,uv_bytes);CHECK3(glGetError()==GL_NO_ERROR);glActiveTexture(GL_TEXTURE0);\n fprintf(stderr,"[G1B-UPLOAD]')
 return s
