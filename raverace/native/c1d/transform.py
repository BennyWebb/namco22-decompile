import pathlib

def apply(name,s,root,a):
 if name=='async_io.c':
  pos=s.index('static void finish(');s=s[:pos]+'''static unsigned c1d_n;static unsigned c1d_tick[18];static uint32_t c1d_meta[18][640*480*4];static uint8_t c1d_colour[18][640*480*4],c1d_raw[18][640*480*4];
void rr_c1d_frame(unsigned t,const void*m,const void*c,const void*r){if(c1d_n>=18)abort();unsigned i=c1d_n++;c1d_tick[i]=t;memcpy(c1d_meta[i],m,sizeof c1d_meta[i]);memcpy(c1d_colour[i],c,sizeof c1d_colour[i]);memcpy(c1d_raw[i],r,sizeof c1d_raw[i]);}
'''+s[pos:]
  s=s.replace('static void finish(void){','''static void finish(void){
 const char*d=getenv("ENG_C1_PROVENANCE_DIR");if(d){for(unsigned i=0;i<c1d_n;i++){const void*data[]={c1d_meta[i],c1d_colour[i],c1d_raw[i]};size_t size[]={sizeof c1d_meta[i],sizeof c1d_colour[i],sizeof c1d_raw[i]};const char*ext[]={"provenance.u32","proof.rgba","texel.rgba"};for(int k=0;k<3;k++){char path[4096];snprintf(path,sizeof path,"%s/tick-%u.%s",d,c1d_tick[i],ext[k]);FILE*f=__real_fopen(path,"wbx");if(!f||fwrite(data[k],1,size[k],f)!=size[k]||fclose(f))abort();}}}
''');return s
 if name!='live.c':return s
 s=s.replace('if(!prec)prec="highp"','if(!prec)prec="lowp"')
 pos=s.index('static void load3(');s=s[:pos]+(pathlib.Path(__file__).with_name('provenance.inc').read_text())+s[pos:]
 s=s.replace(' const char*mode=getenv("ENG_C1_TEXTURES");',' const char*debug=getenv("ENG_C1_DEBUG");CHECK3(!debug||!strcmp(debug,"provenance"));c1d_debug=debug!=NULL;\n const char*mode=getenv("ENG_C1_TEXTURES");')
 s=s.replace('shader3(c1_index?c1dir:shader_dir,names[i],GL_VERTEX_SHADER)', 'shader3(c1d_debug?c1dir:c1_index?c1dir:shader_dir,c1d_debug?(i?"provenance_race_v3_near.vert":"provenance_race_v3.vert"):names[i],GL_VERTEX_SHADER)')
 s=s.replace('shader3(c1_index?c1dir:shader_dir,c1_index?c1_fragment:"race.frag",GL_FRAGMENT_SHADER)', 'shader3(c1d_debug?c1dir:c1_index?c1dir:shader_dir,c1d_debug?"provenance.frag":c1_index?c1_fragment:"race.frag",GL_FRAGMENT_SHADER)')
 s=s.replace('if(c1_index)glUniform1i(glGetUniformLocation(programs3[i],"palette"),5);','if(c1_index)glUniform1i(glGetUniformLocation(programs3[i],"palette"),5);if(c1d_debug){glUniform1i(glGetUniformLocation(programs3[i],"coordinate_image"),6);glUniform1i(glGetUniformLocation(programs3[i],"source_mapping"),7);}')
 s=s.replace(' glFinish();\n}', ' c1d_init(c1dir);fprintf(stderr,"[C1D] debug=%s precision=%s host_only=1\\n",c1d_debug?"provenance":"off",prec);\n glFinish();\n}',1)
 s=s.replace('glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glBindVertexArray', 'glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);c1d_begin(h);glBindVertexArray')
 s=s.replace('glBindTexture(GL_TEXTURE_2D,pages3[g->page]);glScissor', 'glBindTexture(GL_TEXTURE_2D,pages3[g->page]);if(c1d_debug){glActiveTexture(GL_TEXTURE6);glBindTexture(GL_TEXTURE_2D,c1d_coordinates[g->page]);glActiveTexture(GL_TEXTURE7);glBindTexture(GL_TEXTURE_2D,c1d_mapping);glActiveTexture(GL_TEXTURE0); }glScissor')
 s=s.replace('if(capture){save3(out,h->tick,"world");}', 'if(capture){if(c1d_debug)c1d_capture(h->tick);else save3(out,h->tick,"world");}')
 s=s.replace('if(capture)save3(out,h->tick,"composed");','if(capture&&!c1d_debug)save3(out,h->tick,"composed");')
 return s
