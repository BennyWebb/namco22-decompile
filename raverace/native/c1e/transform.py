"""Device-safe diagnostic storage plus measured upload/load receipts; normal math unchanged."""
import pathlib

def apply(name,s,root,a):
 if name=='async_io.c':
  start=s.index('static unsigned c1d_n;');end=s.index('static void finish(',start)
  if a.validation:
   # Replace both original large frame caches with a single bounded writer ring.
   s=s.replace('static unsigned char c1_images[64][640*480*4];','')
   start=s.index('static unsigned c1d_n;');end=s.index('static void finish(',start)
   s=s[:start]+(pathlib.Path(__file__).with_name('capture.inc').read_text())+s[end:]
   p=s.index('void rr_p1b_pixel(');q=s.index('static unsigned c1_nt;',p);pixel=s[p:q];begin=pixel.index('if(getenv("ENG_C1_IMAGES"))');pixel=pixel[:begin]+'if(getenv("ENG_C1_IMAGES"))c1e_colour(tick,suffix,bytes,n);}\n';s=s[:p]+'static void c1e_colour(unsigned,const char*,const void*,size_t);\n'+pixel+s[q:]
   begin=s.index(' const char*d=getenv("ENG_C1_PROVENANCE_DIR");');end=s.index(' const char*path=getenv("ENG_P1B_RECEIPTS");',begin);s=s[:begin]+' c1e_capture_finish();\n'+s[end:]
   begin=s.index(' const char*c1dir=getenv("ENG_C1_IMAGES");');end=s.index(' const char*sem=getenv("ENG_C1_SEMANTIC");',begin);s=s[:begin]+s[end:]
  else:
   s=s[:start]+'void rr_c1d_frame(unsigned t,const void*m,const void*c,const void*r){(void)t;(void)m;(void)c;(void)r;}\n'+s[end:]
   begin=s.index(' const char*d=getenv("ENG_C1_PROVENANCE_DIR");');end=s.index(' const char*path=getenv("ENG_P1B_RECEIPTS");',begin);s=s[:begin]+s[end:]
  return s
 if name!='live.c':return s
 if not a.validation:s=s.replace('c1d_debug=debug!=NULL;','CHECK3(!debug);c1d_debug=false;')
 s=s.replace('host_only=1','isolated_c1e=1')
 start=s.index(' for(unsigned p=0;p<page_count3;p++){',s.index('static void c1d_init'));end=s.index(' unsigned mh=',start)
 s=s[:start]+' for(unsigned p=0;p<page_count3;p++){c1d_coordinates[p]=c1e_axis(widths3[p],false);c1e_ygrids[p]=c1e_axis(heights3[p],true);}\n'+s[end:]
 at=s.index('static void c1d_init');s=s[:at]+pathlib.Path(__file__).with_name('coordinate.inc').read_text()+s[at:]
 s=s.replace('glUniform1i(glGetUniformLocation(programs3[i],"coordinate_image"),6);','glUniform1i(glGetUniformLocation(programs3[i],"coordinate_image"),6);glUniform1i(glGetUniformLocation(programs3[i],"coordinate_y"),8);')
 s=s.replace('glBindTexture(GL_TEXTURE_2D,c1d_coordinates[g->page]);','glBindTexture(GL_TEXTURE_2D,c1d_coordinates[g->page]);glActiveTexture(GL_TEXTURE8);glBindTexture(GL_TEXTURE_2D,c1e_ygrids[g->page]);')
 s=s.replace('static char c1_fragment[80];', 'static double c1e_upload_wall,c1e_upload_cpu,c1e_page_read,c1e_page_upload;static unsigned c1e_groups;static size_t c1e_page_bytes;\nstatic char c1_fragment[80];')
 s=s.replace(' const char*assets=getenv("ENG_NATIVE_ASSET_DIR");',' double c1e_load_start=clock3(CLOCK_MONOTONIC);\n const char*assets=getenv("ENG_NATIVE_ASSET_DIR");')
 s=s.replace('void*p=file3(assets,name,n);','double c1e_t=clock3(CLOCK_MONOTONIC);void*p=file3(assets,name,n);c1e_page_read+=(clock3(CLOCK_MONOTONIC)-c1e_t)*1000;c1e_page_bytes+=n;c1e_t=clock3(CLOCK_MONOTONIC);')
 s=s.replace('else pages3[i]=texture3(widths3[i],heights3[i],GL_RGBA8,p);free(p);','else pages3[i]=texture3(widths3[i],heights3[i],GL_RGBA8,p);c1e_page_upload+=(clock3(CLOCK_MONOTONIC)-c1e_t)*1000;free(p);')
 s=s.replace(' glFinish();\n}', ' glFinish();\n fprintf(stderr,"[C1E-LOAD] mode=%s pages=%u page_bytes=%zu cache_read_ms=%.6f upload_submit_ms=%.6f GL_ready_ms=%.6f debug=%d\\n",c1_index?"index":"rgba",page_count3,c1e_page_bytes,c1e_page_read,c1e_page_upload,(clock3(CLOCK_MONOTONIC)-c1e_load_start)*1000,c1d_debug);\n}',1)
 marker='if(c1_index){glActiveTexture(GL_TEXTURE5);glBindTexture(GL_TEXTURE_2D,c1_palettes[i]);uint8_t row[256*4];'
 s=s.replace(marker,'double c1e_up=clock3(CLOCK_MONOTONIC),c1e_ucpu=clock3(CLOCK_THREAD_CPUTIME_ID);c1e_groups=0;'+marker)
 s=s.replace('for(unsigned p=0;p<256;p++){for(unsigned c=0;c<3;c++)row', 'c1e_groups++;for(unsigned p=0;p<256;p++){for(unsigned c=0;c<3;c++)row')
 s=s.replace('instances3=p1_instances[i];dynamic3=p1_dynamic[i];attributes3=p1_attributes[i];p1_hud=&s->video;', 'c1e_upload_wall=(clock3(CLOCK_MONOTONIC)-c1e_up)*1000;c1e_upload_cpu=(clock3(CLOCK_THREAD_CPUTIME_ID)-c1e_ucpu)*1000;instances3=p1_instances[i];dynamic3=p1_dynamic[i];attributes3=p1_attributes[i];p1_hud=&s->video;')
 s=s.replace(' if(capture&&!c1d_debug)save3(out,h->tick,"composed");', ' if(capture&&!c1d_debug)save3(out,h->tick,"composed");\n if(pass>=0&&!p1b_warming)printf("C1E_FRAME tick=%u upload_ms=%.6f upload_cpu_ms=%.6f groups=%u bytes=%u render_ms=%.6f fence=%d\\n",h->tick,c1e_upload_wall,c1e_upload_cpu,c1e_groups,c1e_groups*1024,(end-start)*1000+c1e_upload_wall,p1_diagnostic);')
 if globals().get('COMPACT_FENCE'):
  lines=s.splitlines(True);old=[x for x in lines if x.startswith(' if(p1_diagnostic){ printf("%s pass=')];assert len(old)==1;s=s.replace(old[0],'')
  lines=s.splitlines(True);old=[x for x in lines if 'printf("C1E_FRAME' in x];assert len(old)==1
  new=old[0].replace('fence=%d\\n','fence=%d world_ms=%.6f hud_ms=%.6f wait_ms=%.6f\\n').replace('p1_diagnostic);','p1_diagnostic,(world_submit-uploaded)*1000,(hud_submit-world_done)*1000,(end-hud_submit)*1000);');assert new!=old[0];s=s.replace(old[0],new)
 return s
