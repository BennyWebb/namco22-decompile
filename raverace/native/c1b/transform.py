"""C1b-only index texture and immutable slot palette derivation."""
import pathlib

def change(s,old,new):
 assert old in s,old[:160];return s.replace(old,new)
def apply(name,s,root,a):
 if name=='rr_presentation.h':return change(s,'    void *native_payload;', '    uint64_t c1_revision[128];\n    void *native_payload;')
 if name=='async_io.c' and a.validation:
  s=s.replace('static unsigned packet_count,pixel_count;', 'static unsigned char c1_images[64][640*480*4];static unsigned packet_count,pixel_count;')
  s=s.replace('static void finish(', 'static unsigned c1_nt;static struct{unsigned tick;size_t n;unsigned char hash[32];}c1_sem[4207];\nvoid rr_c1b_semantic(unsigned t,const void*p,size_t n){if(c1_nt>=4207)abort();unsigned i=c1_nt++;c1_sem[i].tick=t;c1_sem[i].n=n;SHA256(p,n,c1_sem[i].hash); }\nstatic void finish(',1)
  s=s.replace('SHA256(bytes,n,pixels[i].digest);','SHA256(bytes,n,pixels[i].digest);if(getenv("ENG_C1_IMAGES")){if(n!=sizeof c1_images[i])abort();memcpy(c1_images[i],bytes,n);}')
  s=s.replace(' const char*image=getenv("ENG_P1B_IMAGE_HASH");', '\n const char*c1dir=getenv("ENG_C1_IMAGES");if(c1dir){for(unsigned i=0;i<pixel_count;i++){char path[4096];snprintf(path,sizeof path,"%s/tick-%u.%s.rgba",c1dir,pixels[i].tick,pixels[i].suffix);FILE*f=__real_fopen(path,"wbx");if(!f||fwrite(c1_images[i],1,sizeof c1_images[i],f)!=sizeof c1_images[i]||fclose(f))abort();}}\n const char*sem=getenv("ENG_C1_SEMANTIC");if(sem){FILE*f=__wrap_fopen(sem,"wx");if(!f)abort();fprintf(f,"tick,bytes,sha256\\n");for(unsigned i=0;i<c1_nt;i++){fprintf(f,"%u,%zu,",c1_sem[i].tick,c1_sem[i].n);for(int j=0;j<32;j++)fprintf(f,"%02x",c1_sem[i].hash[j]);fputc(10,f);}fclose(f); }\n const char*image=getenv("ENG_P1B_IMAGE_HASH");')
  return s
 if name!='live.c':return s
 s=change(s,'static GLuint pages3[16]', 'static bool c1_index;static unsigned c1_group[8192];static GLuint c1_palettes[3];static uint64_t c1_uploaded[3][128];static bool c1_valid[3][128];\nstatic GLuint pages3[16]')
 s=change(s,'static GLuint shader3(', 'static GLuint shader3(') # shader files selected at load only
 # Configure before reading atlas metadata. Index uses ROM-derived pages and fresh affine.
 s=change(s,' FILE*f=open3(dir,"race.meta");', ''' const char*mode=getenv("ENG_C1_TEXTURES");CHECK3(!mode||!strcmp(mode,"rgba")||!strcmp(mode,"index"));c1_index=mode&&!strcmp(mode,"index");
 const char*c1dir=getenv("ENG_C1_ATLAS");if(c1_index){CHECK3(c1dir);assets=c1dir;}
 FILE*f=open3(dir,"race.meta");''')
 s=change(s,' fclose(f);\n for(unsigned i=0;i<page_count3;i++)', ''' fclose(f);
 if(c1_index){f=open3(c1dir,"atlas.meta");CHECK3(fscanf(f,"%u",&page_count3)==1&&page_count3<=16);for(unsigned i=0;i<page_count3;i++)CHECK3(fscanf(f,"%d %d",widths3+i,heights3+i)==2);CHECK3(!fclose(f));}
 for(unsigned i=0;i<page_count3;i++)''')
 old='snprintf(name,sizeof name,"page-%u.rgba",i);void*p=file3(assets,name,n);pages3[i]=texture3(widths3[i],heights3[i],GL_RGBA8,p);free(p);'
 new='''snprintf(name,sizeof name,c1_index?"page-%u.r8ui":"page-%u.rgba",i);if(c1_index){static_bytes3-=n;n/=4;static_bytes3+=n;}void*p=file3(assets,name,n);
 if(c1_index){glGenTextures(1,pages3+i);glBindTexture(GL_TEXTURE_2D,pages3[i]);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glTexImage2D(GL_TEXTURE_2D,0,GL_R8UI,widths3[i],heights3[i],0,GL_RED_INTEGER,GL_UNSIGNED_BYTE,p);CHECK3(glGetError()==GL_NO_ERROR);}else pages3[i]=texture3(widths3[i],heights3[i],GL_RGBA8,p);free(p);'''
 s=change(s,old,new)
 # Meshes still reside in original pack, not index atlas.
 s=change(s,'file3(assets,"meshes.f32",n)','file3(c1_index?dir:assets,"meshes.f32",n)')
 s=change(s,'memcpy(material_data3,p,material_count3*sizeof(Material3));free(p);','''memcpy(material_data3,p,material_count3*sizeof(Material3));free(p);
 if(c1_index){p=file3(c1dir,"materials.bin",material_count3*sizeof(Material3));memcpy(material_data3,p,material_count3*sizeof(Material3));free(p);p=file3(c1dir,"groups.bin",material_count3*4);memcpy(c1_group,p,material_count3*4);free(p);for(unsigned i=0;i<3;i++)c1_palettes[i]=texture3(256,128,GL_RGBA8,NULL);}
 fprintf(stderr,"[C1-TEXTURES] mode=%s pages=%u bytes=%zu palette_copies=%u snapshot_only=1\\n",c1_index?"index":"rgba",page_count3,static_bytes3,c1_index?3:0);''')
 s=change(s,'shader3(shader_dir,names[i],GL_VERTEX_SHADER),fs=shader3(shader_dir,"race.frag",GL_FRAGMENT_SHADER)', 'shader3(c1_index?c1dir:shader_dir,names[i],GL_VERTEX_SHADER),fs=shader3(c1_index?c1dir:shader_dir,c1_index?"index.frag":"race.frag",GL_FRAGMENT_SHADER)')
 s=change(s,'for(int k=0;k<5;k++)glUniform1i(glGetUniformLocation(programs3[i],samplers[k]),k);', 'for(int k=0;k<5;k++)glUniform1i(glGetUniformLocation(programs3[i],samplers[k]),k);if(c1_index)glUniform1i(glGetUniformLocation(programs3[i],"palette"),5);')
 s=change(s,'(r->material+1)*(r->flags&1?-1.f:1.f)', '(c1_index?(r->material+1)*128+c1_group[r->material]:r->material+1)*(r->flags&1?-1.f:1.f)')
 if a.validation:
  s=change(s,' qsort(live_cmd,n,sizeof *live_cmd,live_painter);',' extern void rr_c1b_semantic(unsigned,const void*,size_t);static LiveCommand c1_copy[MAXQ3];memcpy(c1_copy,live_cmd,n*sizeof *live_cmd);for(unsigned k=0;k<n;k++)c1_copy[k].page=0;rr_c1b_semantic(live_frame,c1_copy,n*sizeof *live_cmd);\n qsort(live_cmd,n,sizeof *live_cmd,live_painter);')
 # Compiler lookup bypasses all palette-content matching only on indexed path.
 s=change(s,'static int live_material(const int32_t *recipe){uint32_t hash=', '''static int live_material(const int32_t *recipe){if(c1_index){unsigned lo=0,hi=live_nt;while(lo<hi){unsigned mid=lo+(hi-lo)/2;if(live_recipe_compare(live_materials[mid].recipe,recipe)<0)lo=mid+1;else hi=mid;}return lo<live_nt&&!live_recipe_compare(live_materials[lo].recipe,recipe)?live_materials[lo].material:-1;}uint32_t hash=''')
 s=change(s,'live_compile();s->native_compile_ms=live_compile_ms;', 'live_compile();if(c1_index)memcpy(s->c1_revision,p2_revision,sizeof s->c1_revision);s->native_compile_ms=live_compile_ms;')
 # Presentation reads only selected snapshot. Each GPU ring has its own revisions.
 upload='''if(c1_index){glActiveTexture(GL_TEXTURE5);glBindTexture(GL_TEXTURE_2D,c1_palettes[i]);uint8_t row[256*4];for(unsigned g=0;g<128;g++)if(!c1_valid[i][g]||c1_uploaded[i][g]!=s->c1_revision[g]){for(unsigned p=0;p<256;p++){for(unsigned c=0;c<3;c++)row[p*4+c]=s->pal[g*256+p+c*32768];row[p*4+3]=255;}glTexSubImage2D(GL_TEXTURE_2D,0,0,g,256,1,GL_RGBA,GL_UNSIGNED_BYTE,row);c1_valid[i][g]=true;c1_uploaded[i][g]=s->c1_revision[g];}glActiveTexture(GL_TEXTURE0);}
 '''
 s=change(s,'instances3=p1_instances[i];dynamic3=p1_dynamic[i];attributes3=p1_attributes[i];p1_hud=&s->video;',upload+'instances3=p1_instances[i];dynamic3=p1_dynamic[i];attributes3=p1_attributes[i];p1_hud=&s->video;')
 # Warm packet is old page grouping; rebuild runs without modifying any geometry.
 hook=pathlib.Path(__file__).with_name('warm.inc').read_text()
 pos=s.index('bool rr_p1_open(void)');s=s[:pos]+hook+s[pos:]
 s=change(s,'p1b_warm_slot.tick=((Header3*)p1b_warm_slot.bytes)->tick;', 'p1b_warm_slot.tick=((Header3*)p1b_warm_slot.bytes)->tick;if(c1_index)c1_repack_warm();')
 return s
