/* System 22 composition on an RGBA8 target: framebuffer alpha is prioverchar.
 * One final pass applies original text/shadow priority and fade/gamma LUTs.
 * No emulation pointers or mutable hardware state enter this module. */
#include "rr_video_layers.h"
#include "eng_gl.h"
#include "quad_gles2.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#define W 640
#define H 480
static bool enabled=true, profiling, profile_gate=true;
static unsigned profile_frames;
static double decode_cpu, upload_cpu, upload_wall, compose_cpu, compose_wall;
static double stamp(clockid_t clock) {struct timespec t;clock_gettime(clock,&t);return t.tv_sec+t.tv_nsec*1e-9;}
void rr_video_layers_profile(bool on){profiling=on;}
void rr_video_layers_profile_gate(bool on){profile_gate=on;}
bool rr_video_layers_profile_enabled(void){return profiling&&profile_gate;}
double rr_video_layers_cpu_time(void){return rr_video_layers_profile_enabled()?stamp(CLOCK_THREAD_CPUTIME_ID):0;}
void rr_video_layers_decode_time(double elapsed){decode_cpu+=elapsed;}
static GLuint target,depth,scene,text_tex,shadow_tex,lut_tex,program,vbo;
static uint8_t last_lut[256][4];
static bool lut_valid;
static GLint sampler_uniform[4], text_uniform, shadow_uniform;
static unsigned frames,uploads;
void rr_video_layers_enable(bool on){enabled=on;}
bool rr_video_layers_enabled(void){return enabled;}
static bool error(const char *where)
{
 GLenum e=glGetError();if(e==GL_NO_ERROR)return true;
 fprintf(stderr,"[VIDEO] %s: GLES error 0x%x\n",where,e);return false;
}
static GLuint shader(GLenum type,const char *source)
{
 GLuint s=glCreateShader(type);glShaderSource(s,1,&source,NULL);glCompileShader(s);
 GLint ok,n;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);glGetShaderiv(s,GL_INFO_LOG_LENGTH,&n);
 if(n>1){char *log=malloc((size_t)n);if(log){glGetShaderInfoLog(s,n,NULL,log);fprintf(stderr,"[VIDEO] shader: %s\n",log);free(log);}}
 if(!ok){glDeleteShader(s);return 0;}return s;
}
static GLuint texture(int w,int h)
{
 GLuint t;glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
 glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);return t;
}
bool rr_video_layers_open(void)
{
 if(!enabled)return true;
 const char *vs="#version 100\nattribute vec2 a_position;varying highp vec2 uv;void main(){gl_Position=vec4(a_position,0.,1.);uv=a_position*.5+.5;}\n";
 const char *fs="#version 100\nprecision highp float;varying highp vec2 uv;"
 "uniform sampler2D scene_tex,text_tex,shadow_tex,lut_tex;uniform bool has_text,has_shadow;"
 "void main(){vec4 base=texture2D(scene_tex,uv);vec3 c=floor(base.rgb*255.+.5);"
 "if(base.a<.5){vec2 p=vec2(uv.x,1.-uv.y);"
 "if(has_shadow){vec4 s=texture2D(shadow_tex,p);if(s.a>.5)c=floor(c*floor(s.rgb*255.+.5)/256.);}"
 "if(has_text){vec4 t=texture2D(text_tex,p);if(t.a>.5)c=floor(t.rgb*255.+.5);}}"
 "vec3 outc;outc.r=texture2D(lut_tex,vec2((c.r+.5)/256.,.5)).r;"
 "outc.g=texture2D(lut_tex,vec2((c.g+.5)/256.,.5)).g;"
 "outc.b=texture2D(lut_tex,vec2((c.b+.5)/256.,.5)).b;gl_FragColor=vec4(outc,1.);}\n";
 GLuint v=shader(GL_VERTEX_SHADER,vs),f=shader(GL_FRAGMENT_SHADER,fs);
 if(!v||!f){if(v)glDeleteShader(v);if(f)glDeleteShader(f);return false;}
 program=glCreateProgram();glAttachShader(program,v);glAttachShader(program,f);
 glBindAttribLocation(program,0,"a_position");glLinkProgram(program);glDeleteShader(v);glDeleteShader(f);
 GLint ok,n;glGetProgramiv(program,GL_LINK_STATUS,&ok);glGetProgramiv(program,GL_INFO_LOG_LENGTH,&n);
 if(n>1){char *log=malloc((size_t)n);if(log){glGetProgramInfoLog(program,n,NULL,log);fprintf(stderr,"[VIDEO] link: %s\n",log);free(log);}}
 if(!ok)return false;
 const char *names[]={"scene_tex","text_tex","shadow_tex","lut_tex"};
 for(int i=0;i<4;i++)sampler_uniform[i]=glGetUniformLocation(program,names[i]);
 text_uniform=glGetUniformLocation(program,"has_text");
 shadow_uniform=glGetUniformLocation(program,"has_shadow");
 scene=texture(W,H);text_tex=texture(W,H);shadow_tex=texture(W,H);lut_tex=texture(256,1);
 glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);
 glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT16,W,H);
 glGenFramebuffers(1,&target);glBindFramebuffer(GL_FRAMEBUFFER,target);
 glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,scene,0);
 glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
 GLenum status=glCheckFramebufferStatus(GL_FRAMEBUFFER);glBindFramebuffer(GL_FRAMEBUFFER,0);
 if(status!=GL_FRAMEBUFFER_COMPLETE){fprintf(stderr,"[VIDEO] RGBA/depth framebuffer incomplete 0x%x\n",status);return false;}
 const GLfloat vertices[]={-1,-1,1,-1,1,1,-1,-1,1,1,-1,1};
 glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,sizeof vertices,vertices,GL_STATIC_DRAW);
 fprintf(stderr,"[VIDEO] original CG/text + priority + shadow + fade/gamma; GPU_fixed_bytes=%u\n",W*H*4*3+W*H*2+256*4);
 return error("initialise compositor");
}
void rr_video_layers_begin(void)
{
 glBindFramebuffer(GL_FRAMEBUFFER,enabled?target:0);
 eng_gles2_priority(enabled);
}
bool rr_video_layers_compose(const uint8_t *text,const uint8_t *shadow,bool dirty,
                             bool any_text,bool any_shadow,const uint8_t lut[256][4])
{
 if(!enabled)return true;
 double cpu=rr_video_layers_cpu_time(),wall=rr_video_layers_profile_enabled()?stamp(CLOCK_MONOTONIC):0;
 glPixelStorei(GL_UNPACK_ALIGNMENT,1);
 if(dirty){glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,text_tex);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,text);
  glBindTexture(GL_TEXTURE_2D,shadow_tex);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,shadow);uploads+=2;}
 if(!lut_valid||memcmp(last_lut,lut,sizeof last_lut)){
  glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,lut_tex);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,256,1,GL_RGBA,GL_UNSIGNED_BYTE,lut);
  memcpy(last_lut,lut,sizeof last_lut);lut_valid=true;
 }
 if(rr_video_layers_profile_enabled()){upload_cpu+=rr_video_layers_cpu_time()-cpu;upload_wall+=stamp(CLOCK_MONOTONIC)-wall;cpu=rr_video_layers_cpu_time();wall=stamp(CLOCK_MONOTONIC);}
 glBindFramebuffer(GL_FRAMEBUFFER,0);glViewport(0,0,W,H);
 glDisable(GL_DEPTH_TEST);glDisable(GL_SCISSOR_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);
 glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glUseProgram(program);
 const GLuint textures[]={scene,text_tex,shadow_tex,lut_tex};
 for(int i=0;i<4;i++){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,textures[i]);glUniform1i(sampler_uniform[i],i);}
 glUniform1i(text_uniform,any_text);glUniform1i(shadow_uniform,any_shadow);
 glBindBuffer(GL_ARRAY_BUFFER,vbo);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,0,0);glEnableVertexAttribArray(0);
 glDisableVertexAttribArray(1);glDisableVertexAttribArray(2);glDrawArrays(GL_TRIANGLES,0,6);glActiveTexture(GL_TEXTURE0);
 bool ok=error("compose frame");
 if(rr_video_layers_profile_enabled()){compose_cpu+=rr_video_layers_cpu_time()-cpu;compose_wall+=stamp(CLOCK_MONOTONIC)-wall;}
 if(rr_video_layers_profile_enabled())profile_frames++;
 frames++;return ok;
}
void rr_video_layers_close(void)
{
 if(!program)return;
 fprintf(stderr,"[VIDEO] presentations=%u full_layer_uploads=%u\n",frames,uploads);
 if(profiling&&profile_frames)fprintf(stderr,"[VIDEO-PROFILE] samples=%u avg_ms decode_cpu=%.6f upload_cpu=%.6f upload_wall=%.6f composition_cpu=%.6f composition_wall=%.6f\n",profile_frames,decode_cpu*1000/profile_frames,upload_cpu*1000/profile_frames,upload_wall*1000/profile_frames,compose_cpu*1000/profile_frames,compose_wall*1000/profile_frames);
 glDeleteFramebuffers(1,&target);glDeleteRenderbuffers(1,&depth);
 GLuint tex[]={scene,text_tex,shadow_tex,lut_tex};glDeleteTextures(4,tex);
 glDeleteBuffers(1,&vbo);glDeleteProgram(program);program=0;
}
