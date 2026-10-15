/* Revised M3: resident original meshes, compact commands and bounded prefetch.
 * No simulator, ROM decoder or audio linked. Repository MIT licence. */
#include <SDL2/SDL.h>
#include <GLES3/gl3.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>
#include <pthread.h>
#include <zlib.h>
#define RR_GLES2 1
#define RR_GPU_HUD 1
#define RR_GPU_HUD_DEFAULT 1
#include "../../src/rr_video_layers.c"
unsigned g_eng_frame;
void eng_gles2_priority(bool enabled_){(void)enabled_;}
#define CHECK3(x) do {if(!(x)){fprintf(stderr,"v3 failure line=%d %s SDL=%s\n",__LINE__,#x,SDL_GetError());exit(2);}}while(0)
#define MAXQ3 4096
#define MAXI3 512
#define MAXM3 8192
#define RAW3 524288
#define SLOTS3 3
#define FONT3 122880
#define STYLE3 2073
typedef struct {uint32_t magic,version,tick,n,visible,groups,instances,dynamic,font,style,palette,reserved;float bg[3];} Header3;
typedef struct {float data[16];int32_t box[4];} Instance3;
typedef struct {int32_t mesh;uint16_t instance,material;float depth;uint16_t brightness[4],source_instance,order,flags,reserved;uint32_t zsort;} Quad3;
typedef struct {uint32_t first,count,page,mode;int32_t box[4];} Group3;
typedef struct {float uv[4];uint32_t page,pad[3];} Material3;
typedef struct {float state[4],brightness[4];} Attribute3;
_Static_assert(sizeof(Header3)==60&&sizeof(Instance3)==80&&sizeof(Quad3)==32&&sizeof(Group3)==32&&sizeof(Material3)==32,"v3 ABI");
typedef struct {uint8_t bytes[RAW3];size_t size;int state;unsigned tick;double read_ms,inflate_ms,worker_cpu_ms;} Slot3;
static Slot3 slots3[SLOTS3];
static pthread_mutex_t lock3=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t ready3=PTHREAD_COND_INITIALIZER,free3=PTHREAD_COND_INITIALIZER;
static FILE *stream3;
static uint64_t offsets3[3841];
static uint8_t zip3[RAW3];static z_stream inflate3;
static unsigned first3,last3,passes3;
static GLuint pages3[16],mesh3,instances3,dynamic3,materials3,programs3[2],vao3,attributes3;
static unsigned page_count3,mesh_width3,mesh_height3,material_count3,font_count3,style_count3;
static int widths3[16],heights3[16];
static Material3 material_data3[MAXM3];
static uint8_t *fonts3,*styles3;
static float instance_data3[MAXI3*16];
static Attribute3 attribute_data3[MAXQ3];
static uint8_t pixels3[640*480*4];
static size_t static_bytes3;static bool stage_sync3;
static double clock3(clockid_t kind){struct timespec t;clock_gettime(kind,&t);return t.tv_sec+t.tv_nsec/1e9;}
static FILE *open3(const char *dir,const char *name){char path[4096];CHECK3(snprintf(path,sizeof path,"%s/%s",dir,name)>0);FILE*f=fopen(path,"rb");CHECK3(f);return f;}
static void read3(FILE*f,void *data,size_t size){CHECK3(fread(data,1,size,f)==size);}
static void *file3(const char *dir,const char *name,size_t size){FILE*f=open3(dir,name);void *p=malloc(size?size:1);CHECK3(p);read3(f,p,size);CHECK3(fgetc(f)==EOF&&!ferror(f));fclose(f);return p;}
static GLuint texture3(unsigned w,unsigned h,GLenum format,const void *data){GLuint t;glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glTexImage2D(GL_TEXTURE_2D,0,format,w,h,0,GL_RGBA,format==GL_RGBA32F?GL_FLOAT:GL_UNSIGNED_BYTE,data);CHECK3(glGetError()==GL_NO_ERROR);return t;}
static GLuint shader3(const char *dir,const char *name,GLenum kind){FILE*f=open3(dir,name);CHECK3(!fseek(f,0,SEEK_END));long n=ftell(f);CHECK3(n>0&&n<65536);rewind(f);char*s=calloc(n+1,1);CHECK3(s);read3(f,s,n);fclose(f);GLuint shader_=glCreateShader(kind);glShaderSource(shader_,1,(const char**)&s,NULL);glCompileShader(shader_);free(s);GLint ok;glGetShaderiv(shader_,GL_COMPILE_STATUS,&ok);if(!ok){char log[8192];glGetShaderInfoLog(shader_,sizeof log,NULL,log);fprintf(stderr,"%s\n",log);}CHECK3(ok);return shader_;}
static void load3(const char *dir,const char *shader_dir){
 const char*assets=getenv("ENG_NATIVE_ASSET_DIR");if(!assets)assets=dir;
 FILE*f=open3(dir,"race.meta");char magic[32];CHECK3(fscanf(f,"%31s %u %u %u %u %u %u",magic,&page_count3,&mesh_width3,&mesh_height3,&material_count3,&font_count3,&style_count3)==7);
 CHECK3(!strcmp(magic,"RRRACE3")&&page_count3>0&&page_count3<=16&&mesh_width3==1024&&mesh_height3>0&&mesh_height3<=8192&&material_count3>0&&material_count3<=MAXM3&&font_count3>0&&font_count3<=16&&style_count3>0&&style_count3<=4096);
 for(unsigned i=0;i<page_count3;i++){CHECK3(fscanf(f,"%d %d",widths3+i,heights3+i)==2);}
 fclose(f);
 for(unsigned i=0;i<page_count3;i++){CHECK3(widths3[i]>0&&widths3[i]<=4096&&heights3[i]>0&&heights3[i]<=4096);size_t n=(size_t)widths3[i]*heights3[i]*4;static_bytes3+=n;CHECK3(static_bytes3<=512u*1024*1024);char name[80];snprintf(name,sizeof name,"page-%u.rgba",i);void*p=file3(assets,name,n);pages3[i]=texture3(widths3[i],heights3[i],GL_RGBA8,p);free(p);}
 size_t n=(size_t)mesh_width3*mesh_height3*16;void*p=file3(assets,"meshes.f32",n);mesh3=texture3(mesh_width3,mesh_height3,GL_RGBA32F,p);free(p);static_bytes3+=n;
 p=file3(dir,"materials.bin",material_count3*sizeof(Material3));memcpy(material_data3,p,material_count3*sizeof(Material3));free(p);unsigned mh=(material_count3+1023)/1024;float *uv=calloc(1024*mh*4,sizeof(float));CHECK3(uv);for(unsigned i=0;i<material_count3;i++){CHECK3(material_data3[i].page<page_count3);memcpy(uv+i*4,material_data3[i].uv,16);}materials3=texture3(1024,mh,GL_RGBA32F,uv);free(uv);static_bytes3+=1024*mh*16;CHECK3(static_bytes3<=512u*1024*1024);
 fonts3=file3(dir,"hud-fonts.bin",font_count3*FONT3);styles3=file3(dir,"hud-styles.bin",style_count3*STYLE3);
 instances3=texture3(4,MAXI3,GL_RGBA32F,NULL);dynamic3=texture3(4096,1,GL_RGBA32F,NULL);
 const char*names[]={"race_v3.vert","race_v3_near.vert"};for(unsigned i=0;i<2;i++){GLuint v=shader3(shader_dir,names[i],GL_VERTEX_SHADER),fs=shader3(shader_dir,"race.frag",GL_FRAGMENT_SHADER);programs3[i]=glCreateProgram();glAttachShader(programs3[i],v);glAttachShader(programs3[i],fs);glLinkProgram(programs3[i]);GLint ok;glGetProgramiv(programs3[i],GL_LINK_STATUS,&ok);CHECK3(ok);glDeleteShader(v);glDeleteShader(fs);glUseProgram(programs3[i]);const char*samplers[]={"image","meshes","instances","dynamic_mesh","materials"};for(int k=0;k<5;k++)glUniform1i(glGetUniformLocation(programs3[i],samplers[k]),k);}
 glGenVertexArrays(1,&vao3);glGenBuffers(1,&attributes3);glBindBuffer(GL_ARRAY_BUFFER,attributes3);glBufferData(GL_ARRAY_BUFFER,sizeof attribute_data3,NULL,GL_STREAM_DRAW);glBindVertexArray(vao3);for(unsigned i=0;i<2;i++){glEnableVertexAttribArray(i);glVertexAttribDivisor(i,1);}glBindVertexArray(0);
 stream3=open3(dir,"frames.rsc");CHECK3(!setvbuf(stream3,NULL,_IOFBF,65536));uint32_t h[4];read3(stream3,h,16);CHECK3(h[0]==0x33525352&&h[1]==3&&h[2]==3840&&h[3]==1);read3(stream3,offsets3,sizeof offsets3);CHECK3(offsets3[0]==16+sizeof offsets3);for(unsigned i=0;i<3840;i++)CHECK3(offsets3[i+1]>offsets3[i]&&offsets3[i+1]-offsets3[i]<=RAW3);CHECK3(!fseek(stream3,0,SEEK_END)&&(uint64_t)ftell(stream3)==offsets3[3840]);memset(&inflate3,0,sizeof inflate3);CHECK3(inflateInit(&inflate3)==Z_OK);glFinish();
}
static void decode3(Slot3*s,unsigned tick){double c=clock3(CLOCK_THREAD_CPUTIME_ID),t=clock3(CLOCK_MONOTONIC);unsigned i=tick-1800;CHECK3(!fseek(stream3,offsets3[i],SEEK_SET));size_t n=offsets3[i+1]-offsets3[i];read3(stream3,zip3,n);double loaded=clock3(CLOCK_MONOTONIC);CHECK3(inflateReset(&inflate3)==Z_OK);inflate3.next_in=zip3;inflate3.avail_in=n;inflate3.next_out=s->bytes;inflate3.avail_out=RAW3;CHECK3(inflate(&inflate3,Z_FINISH)==Z_STREAM_END&&inflate3.avail_in==0);s->size=inflate3.total_out;s->tick=tick;s->read_ms=(loaded-t)*1000;s->inflate_ms=(clock3(CLOCK_MONOTONIC)-loaded)*1000;s->worker_cpu_ms=(clock3(CLOCK_THREAD_CPUTIME_ID)-c)*1000;}
static void *producer3(void *unused){(void)unused;unsigned ordinal=0;for(unsigned pass=0;pass<passes3;pass++)for(unsigned tick=first3;tick<=last3;tick++,ordinal++){Slot3*s=slots3+ordinal%SLOTS3;pthread_mutex_lock(&lock3);while(s->state!=0)pthread_cond_wait(&free3,&lock3);s->state=1;pthread_mutex_unlock(&lock3);decode3(s,tick);pthread_mutex_lock(&lock3);s->state=2;pthread_cond_broadcast(&ready3);pthread_mutex_unlock(&lock3);}return NULL;}
static void save3(const char*out,unsigned tick,const char*suffix){char path[4096];snprintf(path,sizeof path,"%s/tick-%u.%s.rgba",out,tick,suffix);glReadPixels(0,0,640,480,GL_RGBA,GL_UNSIGNED_BYTE,pixels3);FILE*f=fopen(path,"wbx");CHECK3(f&&fwrite(pixels3,4,640*480,f)==640*480);CHECK3(!fclose(f));}
static double last_end3;
static void draw3(const Slot3*s,int pass,const char*out,double start,double waited){
 double cpu=clock3(CLOCK_THREAD_CPUTIME_ID);CHECK3(s->size>=sizeof(Header3));const Header3*h=(const Header3*)s->bytes;CHECK3(h->magic==0x33504e52&&h->version==3&&h->tick==s->tick&&h->n<=MAXQ3&&h->visible<=h->n&&h->groups<=MAXQ3&&h->instances<=MAXI3&&h->dynamic<=4096&&h->font<font_count3&&h->style<style_count3);
 size_t offset=sizeof *h;const Instance3*ins=(const Instance3*)(s->bytes+offset);offset+=h->instances*sizeof *ins;const Quad3*q=(const Quad3*)(s->bytes+offset);offset+=h->n*sizeof *q;const uint16_t*visible=(const uint16_t*)(s->bytes+offset);offset+=(h->visible*2+3)&~3u;const Group3*groups=(const Group3*)(s->bytes+offset);offset+=h->groups*sizeof *groups;const float*dynamic=(const float*)(s->bytes+offset);offset+=h->dynamic*16;const uint8_t*text=s->bytes+offset;offset+=8192;CHECK3(offset==s->size);
 unsigned culled=0;for(unsigned i=0;i<h->n;i++){CHECK3(q[i].instance<h->instances&&q[i].material<material_count3);culled+=(q[i].flags>>8)!=0;}CHECK3(culled+h->visible==h->n);
 for(unsigned i=0;i<h->instances;i++)memcpy(instance_data3+i*16,ins[i].data,64);
 for(unsigned i=0;i<h->visible;i++){CHECK3(visible[i]<h->n);const Quad3*r=q+visible[i];CHECK3(!(r->flags>>8));Attribute3*a=attribute_data3+i;a->state[0]=r->mesh;a->state[1]=r->instance;a->state[2]=r->depth;a->state[3]=(r->material+1)*(r->flags&1?-1.f:1.f);for(unsigned j=0;j<4;j++)a->brightness[j]=r->brightness[j]/64.f;}
 double prepared=clock3(CLOCK_MONOTONIC);glActiveTexture(GL_TEXTURE2);glBindTexture(GL_TEXTURE_2D,instances3);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,4,h->instances,GL_RGBA,GL_FLOAT,instance_data3);glActiveTexture(GL_TEXTURE3);glBindTexture(GL_TEXTURE_2D,dynamic3);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,h->dynamic,1,GL_RGBA,GL_FLOAT,dynamic);glBindBuffer(GL_ARRAY_BUFFER,attributes3);glBufferSubData(GL_ARRAY_BUFFER,0,h->visible*sizeof(Attribute3),attribute_data3);double uploaded=clock3(CLOCK_MONOTONIC);
 rr_video_layers_begin();glViewport(0,0,640,480);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glEnable(GL_DEPTH_TEST);glDepthMask(GL_TRUE);glDepthFunc(GL_LEQUAL);glClearColor(h->bg[0],h->bg[1],h->bg[2],0);glClearDepthf(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glBindVertexArray(vao3);GLuint textures[]={0,mesh3,instances3,dynamic3,materials3};for(unsigned i=1;i<5;i++){glActiveTexture(GL_TEXTURE0+i);glBindTexture(GL_TEXTURE_2D,textures[i]);}glActiveTexture(GL_TEXTURE0);glEnable(GL_SCISSOR_TEST);
 unsigned accounted=0,vertices=0;for(unsigned i=0;i<h->groups;i++){const Group3*g=groups+i;CHECK3(g->first==accounted&&g->count>0&&g->first+g->count<=h->visible&&g->page<page_count3&&g->mode<=3&&g->box[2]>=0&&g->box[3]>=0);accounted+=g->count;unsigned near=g->mode&1;glUseProgram(programs3[near]);glBindTexture(GL_TEXTURE_2D,pages3[g->page]);glScissor(g->box[0],g->box[1],g->box[2],g->box[3]);glBindBuffer(GL_ARRAY_BUFFER,attributes3);for(unsigned k=0;k<2;k++)glVertexAttribPointer(k,4,GL_FLOAT,GL_FALSE,sizeof(Attribute3),(void*)(uintptr_t)(g->first*sizeof(Attribute3)+k*16));glDrawArraysInstanced(GL_TRIANGLES,0,near?12:6,g->count);vertices+=(near?12:6)*g->count;}CHECK3(accounted==h->visible);double world_submit=clock3(CLOCK_MONOTONIC);if(stage_sync3)glFinish();double world_done=clock3(CLOCK_MONOTONIC);
 bool capture=strcmp(out,"-")&&pass==0&&(getenv("ENG_NATIVE_SEQUENCE")||h->tick==1800||h->tick==1965||h->tick==2500||h->tick==2643||h->tick==3000||h->tick==3546||h->tick==3600||h->tick==4200||h->tick==4800||h->tick==5400||h->tick==5639);
 if(capture){save3(out,h->tick,"world");}glBindVertexArray(0);const uint8_t*style=styles3+h->style*STYLE3;uint32_t hs[4];memcpy(hs,style,16);CHECK3(hs[0]==0x48554431);rr_video_cpu_frame hud={0};hud.scroll_x=hs[1];hud.scroll_y=hs[2];hud.shadow_enabled=hs[3];hud.characters=fonts3+h->font*FONT3;hud.tilemap=text;memcpy(hud.palette,style+16,1024);memcpy(hud.multipliers,style+1040,9);memcpy(hud.lut,style+1049,1024);g_eng_frame=h->tick;CHECK3(rr_video_layers_compose_gpu(&hud));double hud_submit=clock3(CLOCK_MONOTONIC);glFinish();double end=clock3(CLOCK_MONOTONIC);double interval=last_end3?end-last_end3:end-start;if(pass>=0)last_end3=end;CHECK3(glGetError()==GL_NO_ERROR);
 printf("%s pass=%d tick=%u primitives=%u visible=%u culled=%u draws=%u vertices=%u instances=%u dynamic_bytes=%zu wait_ms=%.6f read_ms=%.6f inflate_ms=%.6f worker_cpu_ms=%.6f prepare_ms=%.6f upload_ms=%.6f world_submit_ms=%.6f world_sync_ms=%.6f hud_submit_ms=%.6f finish_ms=%.6f total_ms=%.6f cpu_ms=%.6f interval_ms=%.6f\n",pass<0?"WARM":"FRAME",pass,h->tick,h->n,h->visible,culled,h->groups,vertices,h->instances,h->visible*sizeof(Attribute3)+h->instances*64+h->dynamic*16,waited,s->read_ms,s->inflate_ms,s->worker_cpu_ms,(prepared-start)*1000-waited,(uploaded-prepared)*1000,(world_submit-uploaded)*1000,(world_done-world_submit)*1000,(hud_submit-world_done)*1000,(end-hud_submit)*1000,(end-start)*1000,(clock3(CLOCK_THREAD_CPUTIME_ID)-cpu)*1000,interval*1000);
 if(capture)save3(out,h->tick,"composed");
}
int main(int argc,char**argv){
 CHECK3(argc==7);first3=atoi(argv[3]);last3=atoi(argv[4]);passes3=atoi(argv[5]);CHECK3(first3>=1800&&last3<=5639&&last3>=first3&&passes3>0&&passes3<=5);stage_sync3=getenv("ENG_NATIVE_STAGE_SYNC")!=NULL;CHECK3(!SDL_Init(SDL_INIT_VIDEO));SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_Window*w=SDL_CreateWindow("Native race v3",0,0,640,480,SDL_WINDOW_OPENGL);CHECK3(w&&SDL_GL_CreateContext(w));SDL_GL_SetSwapInterval(0);GLint limit,units,attrs;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&limit);glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS,&units);glGetIntegerv(GL_MAX_VERTEX_ATTRIBS,&attrs);CHECK3(limit>=4096&&units>=4&&attrs>=2);printf("GPU renderer=%s version=%s max_texture=%d vertex_units=%d attributes=%d stage_sync=%d\n",glGetString(GL_RENDERER),glGetString(GL_VERSION),limit,units,attrs,stage_sync3);
 double load_start=clock3(CLOCK_MONOTONIC);CHECK3(rr_video_layers_open());load3(argv[1],argv[2]);decode3(slots3,first3);for(int i=-5;i<0;i++)draw3(slots3,i,"-",clock3(CLOCK_MONOTONIC),0);printf("LOAD including_warmup_ms=%.6f static_gpu_bytes=%zu ring_slots=%d ring_payload_bytes=%d dynamic_gpu_bytes=%zu font_bytes=%u style_bytes=%u\n",(clock3(CLOCK_MONOTONIC)-load_start)*1000,static_bytes3,SLOTS3,SLOTS3*RAW3,sizeof attribute_data3+MAXI3*64+4096*16,font_count3*FONT3,style_count3*STYLE3);
 pthread_t producer;CHECK3(!pthread_create(&producer,NULL,producer3,NULL));double prefill=clock3(CLOCK_MONOTONIC);pthread_mutex_lock(&lock3);unsigned initial=(last3-first3+1)*passes3;if(initial>SLOTS3)initial=SLOTS3;for(unsigned i=0;i<initial;i++)while(slots3[i].state!=2)pthread_cond_wait(&ready3,&lock3);pthread_mutex_unlock(&lock3);printf("PREFILL ms=%.6f packets=%u\n",(clock3(CLOCK_MONOTONIC)-prefill)*1000,initial);
 double start_all=clock3(CLOCK_MONOTONIC);unsigned ordinal=0;for(unsigned pass=0;pass<passes3;pass++)for(unsigned tick=first3;tick<=last3;tick++,ordinal++){double start=clock3(CLOCK_MONOTONIC);Slot3*s=slots3+ordinal%SLOTS3;pthread_mutex_lock(&lock3);while(s->state!=2)pthread_cond_wait(&ready3,&lock3);CHECK3(s->tick==tick);s->state=3;pthread_mutex_unlock(&lock3);double waited=(clock3(CLOCK_MONOTONIC)-start)*1000;draw3(s,pass,argv[6],start,waited);pthread_mutex_lock(&lock3);s->state=0;pthread_cond_broadcast(&free3);pthread_mutex_unlock(&lock3);}CHECK3(!pthread_join(producer,NULL));printf("PLAYBACK wall_ms=%.6f frames=%u\n",(clock3(CLOCK_MONOTONIC)-start_all)*1000,ordinal);struct rusage usage;getrusage(RUSAGE_SELF,&usage);printf("RSS max_kib=%ld\n",usage.ru_maxrss);fflush(stdout);rr_video_layers_close();inflateEnd(&inflate3);fclose(stream3);SDL_Quit();return 0;
}
