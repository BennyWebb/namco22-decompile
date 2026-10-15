/* Standalone bounded race playback. No simulator/DSP/ROM decoder linked. MIT. */
#include <SDL2/SDL.h>
#include <GLES3/gl3.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>
#define RR_GLES2 1
#define RR_GPU_HUD 1
#define RR_GPU_HUD_DEFAULT 1
#include "../../src/rr_video_layers.c"
unsigned g_eng_frame;
void eng_gles2_priority(bool on){(void)on;}
#define REQUIRE(x) do{if(!(x)){fprintf(stderr,"race failure line=%d: %s GL=%x SDL=%s\n",__LINE__,#x,glGetError(),SDL_GetError());exit(2);}}while(0)
#define MAXQ 4096
#define MAXP 16
#define STATE_SIZE (16+0x20000+1024+9+1024)
typedef struct{uint32_t magic,version,tick,n,groups,dynamic,hud,palette;float bg[3];} Header;
typedef struct{int32_t first,count,page,box[4],protect;} Group;
_Static_assert(sizeof(Header)==44,"frame ABI");
static GLuint rpages[MAXP],rmesh,rcmd,rdynamic,rprogram,rvao;
static unsigned np,mw,mh,mq;static int pw[MAXP],ph[MAXP];
static float command_data[MAXQ*36],dynamic_data[4096*4];static Group group_data[MAXQ];
static uint8_t hud_data[STATE_SIZE],capture[640*480*4];
static size_t static_bytes;static GLint first_location;static FILE*rstream;static uint64_t roffsets[3841];
static double rnow(clockid_t clock){struct timespec t;clock_gettime(clock,&t);return t.tv_sec+t.tv_nsec/1e9;}
static FILE*ropen(const char*dir,const char*name){char p[4096];REQUIRE(snprintf(p,sizeof p,"%s/%s",dir,name)>0);FILE*f=fopen(p,"rb");REQUIRE(f);return f;}
static void rread(FILE*f,void*p,size_t n){REQUIRE(fread(p,1,n,f)==n);}
static void reof(FILE*f){REQUIRE(fgetc(f)==EOF&&!ferror(f));fclose(f);}
static GLuint rtexture(unsigned w,unsigned h,GLenum format,const void*data){GLuint tex;glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glTexImage2D(GL_TEXTURE_2D,0,format,w,h,0,GL_RGBA,format==GL_RGBA32F?GL_FLOAT:GL_UNSIGNED_BYTE,data);REQUIRE(glGetError()==GL_NO_ERROR);return tex;}
static GLuint rshader(const char*dir,const char*name,GLenum kind){FILE*f=ropen(dir,name);REQUIRE(!fseek(f,0,SEEK_END));long n=ftell(f);REQUIRE(n>0&&n<65536);rewind(f);char*s=calloc(n+1,1);REQUIRE(s);rread(f,s,n);reof(f);GLuint shader=glCreateShader(kind);glShaderSource(shader,1,(const char**)&s,NULL);glCompileShader(shader);free(s);GLint ok;glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);if(!ok){char log[8192];glGetShaderInfoLog(shader,sizeof log,NULL,log);fprintf(stderr,"%s\n",log);}REQUIRE(ok);return shader;}
static void rload(const char*dir,const char*shaders){FILE*f=ropen(dir,"race.meta");char magic[32];REQUIRE(fscanf(f,"%31s %u %u %u %u",magic,&np,&mw,&mh,&mq)==5);REQUIRE(!strcmp(magic,"RRRACE2")&&np>0&&np<=MAXP&&mw==1024&&mh<=8192&&mq<=MAXQ);for(unsigned i=0;i<np;i++)REQUIRE(fscanf(f,"%d %d",pw+i,ph+i)==2);fclose(f);
 for(unsigned i=0;i<np;i++){REQUIRE(pw[i]>0&&pw[i]<=4096&&ph[i]>0&&ph[i]<=4096);size_t n=(size_t)pw[i]*ph[i]*4;static_bytes+=n;REQUIRE(static_bytes<=512u*1024*1024);uint8_t*b=malloc(n);REQUIRE(b);char name[80];snprintf(name,sizeof name,"page-%u.rgba",i);f=ropen(dir,name);rread(f,b,n);reof(f);rpages[i]=rtexture(pw[i],ph[i],GL_RGBA8,b);free(b);}
 size_t n=(size_t)mw*mh*16;float*b=malloc(n);REQUIRE(b);f=ropen(dir,"meshes.f32");rread(f,b,n);reof(f);rmesh=rtexture(mw,mh,GL_RGBA32F,b);free(b);static_bytes+=n;
 rcmd=rtexture(9,MAXQ,GL_RGBA32F,NULL);rdynamic=rtexture(4096,1,GL_RGBA32F,NULL);
 rstream=ropen(dir,"frames.rsc");uint32_t stream_header[4];rread(rstream,stream_header,sizeof stream_header);REQUIRE(stream_header[0]==0x33525352&&stream_header[1]==2&&stream_header[2]==3840);rread(rstream,roffsets,sizeof roffsets);REQUIRE(roffsets[0]==16+sizeof roffsets);
 rprogram=glCreateProgram();GLuint vs=rshader(shaders,"race.vert",GL_VERTEX_SHADER),fs=rshader(shaders,"race.frag",GL_FRAGMENT_SHADER);glAttachShader(rprogram,vs);glAttachShader(rprogram,fs);glLinkProgram(rprogram);GLint ok;glGetProgramiv(rprogram,GL_LINK_STATUS,&ok);REQUIRE(ok);glDeleteShader(vs);glDeleteShader(fs);glUseProgram(rprogram);const char*names[]={"image","meshes","commands","dynamic_mesh"};for(int i=0;i<4;i++)glUniform1i(glGetUniformLocation(rprogram,names[i]),i);first_location=glGetUniformLocation(rprogram,"first_command");glGenVertexArrays(1,&rvao);glFinish();
}
static rr_video_cpu_frame rhud(void){rr_video_cpu_frame h={0};uint32_t header[4];memcpy(header,hud_data,16);REQUIRE(header[0]==0x48554431);h.scroll_x=header[1];h.scroll_y=header[2];h.shadow_enabled=header[3];h.characters=hud_data+16;h.tilemap=hud_data+16+0x1e000;size_t off=16+0x20000;memcpy(h.palette,hud_data+off,1024);off+=1024;memcpy(h.multipliers,hud_data+off,9);off+=9;memcpy(h.lut,hud_data+off,1024);return h;}
int main(int argc,char**argv){
 REQUIRE(argc==7);unsigned from=atoi(argv[3]),through=atoi(argv[4]),passes=atoi(argv[5]);const char*out=argv[6];REQUIRE(from>=1800&&through<=5639&&through>=from&&passes>0&&passes<=5);
 REQUIRE(!SDL_Init(SDL_INIT_VIDEO));SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_Window*w=SDL_CreateWindow("Native race playback",0,0,640,480,SDL_WINDOW_OPENGL);REQUIRE(w&&SDL_GL_CreateContext(w));SDL_GL_SetSwapInterval(0);GLint limit,units;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&limit);glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS,&units);REQUIRE(limit>=4096&&units>=3);printf("GPU renderer=%s version=%s max_texture=%d vertex_units=%d\n",glGetString(GL_RENDERER),glGetString(GL_VERSION),limit,units);
 double begin=rnow(CLOCK_MONOTONIC);REQUIRE(rr_video_layers_open());rload(argv[1],argv[2]);printf("LOAD ms=%.6f static_gpu_bytes=%zu dynamic_gpu_bytes=%u bounded_frame_bytes=%zu\n",(rnow(CLOCK_MONOTONIC)-begin)*1000,static_bytes,9*MAXQ*16+4096*16,sizeof command_data+sizeof dynamic_data+sizeof group_data+sizeof hud_data);
 double last_completion=0,playback_start=0;
 for(int pass=-5;pass<(int)passes;pass++)for(unsigned tick=from;tick<=(pass<0?from:through);tick++){
  double start=rnow(CLOCK_MONOTONIC),cpu=rnow(CLOCK_THREAD_CPUTIME_ID);if(pass>=0&&!playback_start)playback_start=start;REQUIRE(!fseek(rstream,roffsets[tick-1800],SEEK_SET));FILE*f=rstream;Header h;rread(f,&h,sizeof h);REQUIRE(h.magic==0x33504e52&&h.version==2&&h.tick==tick&&h.n<=MAXQ&&h.groups<=MAXQ&&h.dynamic<=4096&&h.hud==STATE_SIZE);rread(f,command_data,h.n*144);rread(f,group_data,h.groups*32);rread(f,dynamic_data,h.dynamic*16);rread(f,hud_data,STATE_SIZE);REQUIRE((uint64_t)ftell(f)==roffsets[tick-1799]);double loaded=rnow(CLOCK_MONOTONIC);
  glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,rcmd);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,9,h.n,GL_RGBA,GL_FLOAT,command_data);glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,rdynamic);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,h.dynamic,1,GL_RGBA,GL_FLOAT,dynamic_data);double uploaded=rnow(CLOCK_MONOTONIC);
  rr_video_layers_begin();glViewport(0,0,640,480);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDepthFunc(GL_LEQUAL);glClearColor(h.bg[0],h.bg[1],h.bg[2],0);glClearDepthf(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glUseProgram(rprogram);glBindVertexArray(rvao);glActiveTexture(GL_TEXTURE1);glBindTexture(GL_TEXTURE_2D,rmesh);glActiveTexture(GL_TEXTURE0);glEnable(GL_SCISSOR_TEST);
  unsigned accounted=0;for(unsigned i=0;i<h.groups;i++){Group*g=group_data+i;REQUIRE(g->first==(int)accounted&&g->count>0&&g->first+g->count<=(int)h.n&&g->page>=0&&g->page<(int)np&&g->box[2]>=0&&g->box[3]>=0);accounted+=g->count;glBindTexture(GL_TEXTURE_2D,rpages[g->page]);glScissor(g->box[0],g->box[1],g->box[2],g->box[3]);glUniform1i(first_location,g->first);glDrawArraysInstanced(GL_TRIANGLES,0,12,g->count);}REQUIRE(accounted==h.n);double submitted=rnow(CLOCK_MONOTONIC);glFinish();double world=rnow(CLOCK_MONOTONIC);g_eng_frame=tick;rr_video_cpu_frame hud=rhud();REQUIRE(rr_video_layers_compose_gpu(&hud));glFinish();double end=rnow(CLOCK_MONOTONIC),thread=rnow(CLOCK_THREAD_CPUTIME_ID)-cpu;REQUIRE(glGetError()==GL_NO_ERROR);double interval=last_completion?end-last_completion:end-start;if(pass>=0)last_completion=end;
  printf(pass<0?"WARM pass=%d tick=%u primitives=%u draws=%u dynamic_bytes=%u load_ms=%.6f upload_ms=%.6f submit_ms=%.6f world_ms=%.6f hud_ms=%.6f total_ms=%.6f cpu_ms=%.6f interval_ms=%.6f\n":"FRAME pass=%d tick=%u primitives=%u draws=%u dynamic_bytes=%u load_ms=%.6f upload_ms=%.6f submit_ms=%.6f world_ms=%.6f hud_ms=%.6f total_ms=%.6f cpu_ms=%.6f interval_ms=%.6f\n",pass,tick,h.n,h.groups,h.n*144+h.dynamic*16,(loaded-start)*1000,(uploaded-loaded)*1000,(submitted-uploaded)*1000,(world-start)*1000,(end-world)*1000,(end-start)*1000,thread*1000,interval*1000);
  if(strcmp(out,"-")&&pass==0&&(getenv("ENG_NATIVE_SEQUENCE")||tick==1800||tick==1965||tick==2500||tick==3000||tick==3600||tick==4200||tick==4800||tick==5400||tick==5639)){glReadPixels(0,0,640,480,GL_RGBA,GL_UNSIGNED_BYTE,capture);char path[4096];snprintf(path,sizeof path,"%s/tick-%u.rgba",out,tick);FILE*image=fopen(path,"wbx");REQUIRE(image&&fwrite(capture,4,640*480,image)==640*480);fclose(image);}
 }
 printf("PLAYBACK wall_ms=%.6f frames=%u\n",(rnow(CLOCK_MONOTONIC)-playback_start)*1000,(through-from+1)*passes);
 struct rusage usage;getrusage(RUSAGE_SELF,&usage);printf("RSS max_kib=%ld\n",usage.ru_maxrss);fflush(stdout);rr_video_layers_close();SDL_Quit();return 0;
}
