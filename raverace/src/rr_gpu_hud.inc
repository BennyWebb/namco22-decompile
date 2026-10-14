/* Optional GLES2 packed character/tile compositor. State is selected by the
 * caller; caches compare against the last actual upload, never a skipped slot. */
#ifdef RR_GPU_HUD
#include "eng.h"
static GLuint gpu_program, raw_tex, palette_tex;
static GLint gpu_samplers[4], gpu_scroll, gpu_shadow, gpu_mix[3];
static uint8_t uploaded_raw[0x20000], uploaded_palette[256][4];
static bool raw_valid, palette_valid;
static uint64_t gpu_bytes, gpu_calls, gpu_pictures, gpu_verified;
static uint64_t gpu_profile_bytes,gpu_profile_calls;
static uint64_t gpu_profile_glyph_bytes,gpu_profile_vertex_bytes,gpu_profile_palette_bytes,gpu_profile_lut_bytes;
static uint8_t *reference_pixels,*gpu_pixels;
#include "rr_gpu_hud_geometry.inc"
static bool gpu_open(void)
{
 const char *vs="#version 100\nattribute vec2 a_position;varying highp vec2 uv;void main(){gl_Position=vec4(a_position,0.,1.);uv=a_position*.5+.5;}\n";
 const char *fs="#version 100\nprecision highp float;varying highp vec2 uv;"
 "uniform sampler2D scene_tex,raw_tex,palette_tex,lut_tex;uniform vec2 scroll;uniform bool shadow_on;uniform vec3 mix0,mix1,mix2;"
 "vec4 word4(float off){float t=floor(off/4.);return floor(texture2D(raw_tex,vec2((mod(t,256.)+.5)/256.,(floor(t/256.)+.5)/128.))*255.+.5);}"
 "float byteAt(float off){vec4 b=word4(off);float k=mod(off,4.);if(k<.5)return b.r;if(k<1.5)return b.g;if(k<2.5)return b.b;return b.a;}"
 "void main(){vec4 base=texture2D(scene_tex,uv);vec3 c=floor(base.rgb*255.+.5);"
 "if(base.a<.5){vec2 p=mod(floor(vec2(uv.x,1.-uv.y)*vec2(640.,480.))+scroll,1024.);"
 "float off=122880.+2.*(floor(p.y/16.)*64.+floor(p.x/16.));vec4 b=word4(off);vec2 h=mod(off,4.)<2.?b.rg:b.ba;float w=h.x*256.+h.y;"
 "float code=mod(w,1024.);vec2 q=mod(p,16.);if(mod(floor(w/1024.),2.)>.5)q.x=15.-q.x;if(mod(floor(w/2048.),2.)>.5)q.y=15.-q.y;"
 "float v=byteAt(code*128.+q.y*8.+floor(q.x/2.));float pen=mod(q.x,2.)<1.?floor(v/16.):mod(v,16.);"
 "if(pen<14.5){float idx=floor(w/4096.)*16.+pen;"
 "if(shadow_on&&idx>=252.&&idx<=254.){vec3 m=idx<252.5?mix0:(idx<253.5?mix1:mix2);c=floor(c*m/256.);}"
 "else c=floor(texture2D(palette_tex,vec2((idx+.5)/256.,.5)).rgb*255.+.5);}}"
 "vec3 o;o.r=texture2D(lut_tex,vec2((c.r+.5)/256.,.5)).r;o.g=texture2D(lut_tex,vec2((c.g+.5)/256.,.5)).g;o.b=texture2D(lut_tex,vec2((c.b+.5)/256.,.5)).b;gl_FragColor=vec4(o,1.);}\n";
 if(gpu_geometry())fs=glyph_final_fs;
 GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs);
 if(!v||!f){if(v)glDeleteShader(v);if(f)glDeleteShader(f);return false;}
 gpu_program=glCreateProgram();glAttachShader(gpu_program,v);glAttachShader(gpu_program,f);glBindAttribLocation(gpu_program,0,"a_position");glLinkProgram(gpu_program);glDeleteShader(v);glDeleteShader(f);
 GLint ok;glGetProgramiv(gpu_program,GL_LINK_STATUS,&ok);if(!ok){fprintf(stderr,"[GPU-HUD] shader link failed\n");return false;}
 const char *names[]={"scene_tex","raw_tex","palette_tex","lut_tex"};for(int i=0;i<4;i++)gpu_samplers[i]=glGetUniformLocation(gpu_program,names[i]);
 gpu_scroll=glGetUniformLocation(gpu_program,"scroll");gpu_shadow=glGetUniformLocation(gpu_program,"shadow_on");
 const char *mixes[]={"mix0","mix1","mix2"};for(int i=0;i<3;i++)gpu_mix[i]=glGetUniformLocation(gpu_program,mixes[i]);
 raw_tex=gpu_geometry()?texture(W,H):texture(256,128);palette_tex=texture(256,1);
 if(gpu_geometry()&&!geometry_open())return false;
 fprintf(stderr,"[GPU-HUD] mode=%s GLES2; verify=%d\n",gpu_geometry()?"glyph-quads":"packed-tilemap",rr_video_layers_verify());
 return error("initialise packed HUD");
}
static void gpu_upload_rows(const uint8_t *source,int first,int rows)
{
 int end=first+rows;
 for(int row=first;row<end;){
  if(raw_valid&&!memcmp(uploaded_raw+row*1024,source+(row-first)*1024,1024)){row++;continue;}
  int begin=row++;
  while(row<end&&(!raw_valid||memcmp(uploaded_raw+row*1024,source+(row-first)*1024,1024)))row++;
  size_t bytes=(size_t)(row-begin)*1024;
  glTexSubImage2D(GL_TEXTURE_2D,0,0,begin,256,row-begin,GL_RGBA,GL_UNSIGNED_BYTE,source+(begin-first)*1024);
  memcpy(uploaded_raw+begin*1024,source+(begin-first)*1024,bytes);
  gpu_bytes+=bytes;gpu_calls++;if(rr_video_layers_profile_enabled()){gpu_profile_bytes+=bytes;gpu_profile_calls++;}
 }
}
static bool write_case(const char *dir,const char *name,const void *p,size_t n)
{
 char path[1024];snprintf(path,sizeof path,"%s/hud-%u.%s",dir,g_eng_frame,name);FILE *f=fopen(path,"wb");if(!f)return false;bool ok=fwrite(p,1,n,f)==n;if(fclose(f))ok=false;return ok;
}
bool rr_video_layers_compose_gpu(const rr_video_cpu_frame *frame)
{
 if(!enabled)return true;
 bool verify=rr_video_layers_verify();
 if(verify){
  if(!reference_pixels)reference_pixels=malloc(W*H*4);
  if(!gpu_pixels)gpu_pixels=malloc(W*H*4);
  if(!reference_pixels||!gpu_pixels)return false;
  if(!rr_video_layers_compose(frame->text,frame->shadow,true,frame->any_text,frame->any_shadow,frame->lut))return false;
  glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,reference_pixels);
 }
 double cpu=rr_video_layers_cpu_time(),wall=rr_video_layers_profile_enabled()?stamp(CLOCK_MONOTONIC):0;
 glPixelStorei(GL_UNPACK_ALIGNMENT,1);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,raw_tex);
 if(gpu_geometry()){if(!geometry_prepare(frame))return false;}
 else {gpu_upload_rows(frame->characters,0,120);gpu_upload_rows(frame->tilemap,120,8);raw_valid=true;}
 if(!palette_valid||memcmp(uploaded_palette,frame->palette,sizeof uploaded_palette)){
  glBindTexture(GL_TEXTURE_2D,palette_tex);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,256,1,GL_RGBA,GL_UNSIGNED_BYTE,frame->palette);
  memcpy(uploaded_palette,frame->palette,sizeof uploaded_palette);palette_valid=true;gpu_bytes+=1024;gpu_calls++;if(rr_video_layers_profile_enabled()){gpu_profile_bytes+=1024;gpu_profile_calls++;gpu_profile_palette_bytes+=1024;}
 }
 if(!lut_valid||memcmp(last_lut,frame->lut,sizeof last_lut)){
  glBindTexture(GL_TEXTURE_2D,lut_tex);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,256,1,GL_RGBA,GL_UNSIGNED_BYTE,frame->lut);memcpy(last_lut,frame->lut,sizeof last_lut);lut_valid=true;gpu_bytes+=1024;gpu_calls++;if(rr_video_layers_profile_enabled()){gpu_profile_bytes+=1024;gpu_profile_calls++;gpu_profile_lut_bytes+=1024;}
 }
 if(rr_video_layers_profile_enabled()){upload_cpu+=rr_video_layers_cpu_time()-cpu;upload_wall+=stamp(CLOCK_MONOTONIC)-wall;cpu=rr_video_layers_cpu_time();wall=stamp(CLOCK_MONOTONIC);}
 glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,W,H);glDisable(GL_DEPTH_TEST);glDisable(GL_SCISSOR_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glUseProgram(gpu_program);
 const GLuint ts[]={scene,raw_tex,palette_tex,lut_tex};for(int i=0;i<4;i++){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,ts[i]);glUniform1i(gpu_samplers[i],i);}
 glUniform2f(gpu_scroll,frame->scroll_x,frame->scroll_y);glUniform1i(gpu_shadow,frame->shadow_enabled);
 for(int i=0;i<3;i++)glUniform3f(gpu_mix[i],frame->multipliers[i][0],frame->multipliers[i][1],frame->multipliers[i][2]);
 glBindBuffer(GL_ARRAY_BUFFER,vbo);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,0);glEnableVertexAttribArray(0);glDisableVertexAttribArray(1);glDisableVertexAttribArray(2);glDrawArrays(GL_TRIANGLES,0,6);glActiveTexture(GL_TEXTURE0);
 if(rr_video_layers_profile_enabled()){compose_cpu+=rr_video_layers_cpu_time()-cpu;compose_wall+=stamp(CLOCK_MONOTONIC)-wall;profile_frames++;}
 frames++;gpu_pictures++;
 if(verify){
  glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,gpu_pixels);size_t differences=0;unsigned max_difference=0;for(size_t i=0;i<W*H*4;i++){unsigned d=abs((int)gpu_pixels[i]-reference_pixels[i]);differences+=d!=0;if(d>max_difference)max_difference=d;}
  const char *dir=getenv("ENG_GPU_HUD_CAPTURE");
  const char *ticks=getenv("ENG_GPU_HUD_CAPTURE_TICKS");bool selected=!ticks;
  if(ticks)for(const char *p=ticks;*p;){char *end;unsigned long tick=strtoul(p,&end,10);if(end==p)break;if(tick==g_eng_frame)selected=true;p=*end==','?end+1:end;}
  if(dir&&(selected||differences)){
   uint32_t header[]={0x48554431,frame->scroll_x,frame->scroll_y,frame->shadow_enabled};
   uint8_t state[16+0x20000+1024+9+1024];size_t at=0;
#define SAVE_PART(p,n) do{memcpy(state+at,p,n);at+=n;}while(0)
   SAVE_PART(header,sizeof header);SAVE_PART(frame->characters,0x1e000);SAVE_PART(frame->tilemap,0x2000);SAVE_PART(frame->palette,1024);SAVE_PART(frame->multipliers,9);SAVE_PART(frame->lut,1024);
#undef SAVE_PART
   if(!write_case(dir,"state",state,at)||!write_case(dir,"reference.rgba",reference_pixels,W*H*4)||!write_case(dir,"gpu.rgba",gpu_pixels,W*H*4))return false;
   if(gpu_geometry()){
    glBindFramebuffer(GL_FRAMEBUFFER,pen_target);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,gpu_pixels);glBindFramebuffer(GL_FRAMEBUFFER,0);
    if(!write_case(dir,"pen.rgba",gpu_pixels,W*H*4)||!write_case(dir,"uploaded-source",uploaded_raw,sizeof uploaded_raw))return false;
   }
   glBindFramebuffer(GL_FRAMEBUFFER,target);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,gpu_pixels);glBindFramebuffer(GL_FRAMEBUFFER,0);if(!write_case(dir,"world.rgba",gpu_pixels,W*H*4))return false;
  }
  if(differences){fprintf(stderr,"[GPU-HUD] mismatch tick=%u channels=%zu max=%u\n",g_eng_frame,differences,max_difference);return false;}gpu_verified++;
 }
 return error("packed HUD compose");
}
static void gpu_close(void)
{
 if(!gpu_program)return;
 fprintf(stderr,"[GPU-HUD] pictures=%llu verified=%llu bytes=%llu calls=%llu profile_bytes=%llu profile_calls=%llu\n",(unsigned long long)gpu_pictures,(unsigned long long)gpu_verified,(unsigned long long)gpu_bytes,(unsigned long long)gpu_calls,(unsigned long long)gpu_profile_bytes,(unsigned long long)gpu_profile_calls);
 fprintf(stderr,"[GPU-HUD-TRANSFER] scope=profile glyph_bytes=%llu vertex_bytes=%llu palette_bytes=%llu lut_bytes=%llu\n",(unsigned long long)gpu_profile_glyph_bytes,(unsigned long long)gpu_profile_vertex_bytes,(unsigned long long)gpu_profile_palette_bytes,(unsigned long long)gpu_profile_lut_bytes);
 geometry_close();GLuint ts[]={raw_tex,palette_tex};glDeleteTextures(2,ts);glDeleteProgram(gpu_program);gpu_program=0;free(reference_pixels);free(gpu_pixels);reference_pixels=gpu_pixels=NULL;
}
#endif
