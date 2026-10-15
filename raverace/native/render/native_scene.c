/* Native local-space frozen scene. MIT licence from repository root.
 * Source meshes + Q15 instances are prepared once at load; no simulation links. */
#include <SDL2/SDL.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"failure line %d: %s SDL=%s GL=%x\n",__LINE__,#x,SDL_GetError(),glGetError());exit(2);}} while(0)
#define LIMIT 32768
#define PAGE_LIMIT 32
#define MAT_LIMIT 8192
/* Binary ABI, all little endian; compiler/host/H700 use IEEE float32. */
typedef struct {int32_t x,y,z,u,v,bri;} Source;
typedef struct {int32_t m[9],t[3];float projection[4];} Instance;
typedef struct {uint32_t first,instance,page,flags;int32_t box[4];float uv[4],source_uv[4],depth,alpha;uint32_t code,packet,material;} Quad;
typedef struct {uint32_t page,x,y,w,h;} Material;
typedef struct {float p[3],uv[2],source_uv[2],brightness,depth,alpha,projection[4];} Vertex;
typedef struct {unsigned first,count,page,material,ordinal;int box[4],protected;} Draw;
_Static_assert(sizeof(Source)==24,"source ABI");
_Static_assert(sizeof(Instance)==64,"instance ABI");
_Static_assert(sizeof(Quad)==84,"quad ABI");
_Static_assert(sizeof(Vertex)==56,"GPU ABI");
static Source*source;static Instance*instances;static Quad*quads;static Material*materials;
static Vertex*vertices;static unsigned nv,nq,ns,ni,nm,np,nd,ng;static Draw draws[LIMIT],groups[LIMIT];static unsigned *indices;static GLuint pages[PAGE_LIMIT],source_textures[MAT_LIMIT],vbo,ebo,program,fb;static int widths[PAGE_LIMIT],heights[PAGE_LIMIT];static float bg[3];static size_t page_bytes,source_texture_bytes;static unsigned near_clipped,fully_clipped;static int selection;
static double now(clockid_t id){struct timespec t;clock_gettime(id,&t);return t.tv_sec+t.tv_nsec/1e9;}
static FILE*openfile(const char*dir,const char*name){char path[4096];int n=snprintf(path,sizeof path,"%s/%s",dir,name);CHECK(n>0&&(size_t)n<sizeof path);FILE*f=fopen(path,"rb");CHECK(f);return f;}
static void*readfile(const char*dir,const char*name,size_t n){FILE*f=openfile(dir,name);void*b=malloc(n?n:1);CHECK(b&&fread(b,1,n,f)==n&&fgetc(f)==EOF);fclose(f);return b;}
static GLuint texture(int w,int h,const void*data){GLuint t;glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,data);CHECK(glGetError()==GL_NO_ERROR);return t;}
static GLuint shader_program(void){
 const char*vs="attribute vec3 position;attribute vec2 uv;attribute vec3 shade_depth_alpha;attribute vec4 camera;varying highp vec2 texcoord;varying highp vec2 brightness_z;varying mediump float alpha;void main(){float z=position.z;gl_Position=vec4(camera.x*z+camera.z*position.x,camera.y*z+camera.w*position.y,shade_depth_alpha.y*z,z);texcoord=uv;brightness_z=vec2(shade_depth_alpha.x*z,z);alpha=shade_depth_alpha.z;}";
 const char*fs="precision highp float;uniform sampler2D image;varying highp vec2 texcoord;varying highp vec2 brightness_z;varying mediump float alpha;void main(){vec4 t=texture2D(image,texcoord);if(t.a==0.0)discard;float b=brightness_z.x/brightness_z.y;gl_FragColor=vec4(t.rgb*b,alpha);}";
 GLuint p=glCreateProgram();const char*src[2]={vs,fs};for(int i=0;i<2;i++){GLuint s=glCreateShader(i?GL_FRAGMENT_SHADER:GL_VERTEX_SHADER);glShaderSource(s,1,src+i,NULL);glCompileShader(s);GLint ok;glGetShaderiv(s,GL_COMPILE_STATUS,&ok);if(!ok){char log[4096];glGetShaderInfoLog(s,sizeof log,NULL,log);fprintf(stderr,"%s\n",log);}CHECK(ok);glAttachShader(p,s);glDeleteShader(s);}const char*names[]={"position","uv","shade_depth_alpha","camera"};for(int i=0;i<4;i++)glBindAttribLocation(p,i,names[i]);glLinkProgram(p);GLint ok;glGetProgramiv(p,GL_LINK_STATUS,&ok);CHECK(ok);return p;
}
static Vertex lerp(Vertex a,Vertex b,float t){Vertex o;float*x=(float*)&o,*aa=(float*)&a,*bb=(float*)&b;for(int i=0;i<14;i++)x[i]=aa[i]+(bb[i]-aa[i])*t;o.p[2]=1;return o;}
static void append(Vertex v){CHECK(nv<2000000);vertices=realloc(vertices,(nv+1)*sizeof *vertices);CHECK(vertices);vertices[nv++]=v;}
static int compare(const void*aa,const void*bb){const Draw*a=aa,*b=bb;if(a->protected!=b->protected)return a->protected-b->protected;if(a->page!=b->page)return a->page<b->page?-1:1;int k=memcmp(a->box,b->box,sizeof a->box);if(k)return k;return a->ordinal<b->ordinal?-1:a->ordinal!=b->ordinal;}
static void prepare(void){
 for(unsigned qi=0;qi<nq;qi++){
  const Quad*q=quads+qi;if(selection&&q->code!=1280&&q->code!=1945)continue;
  CHECK(q->first+4<=ns&&q->instance<ni&&q->page<np&&q->material<nm);const Instance*in=instances+q->instance;Vertex p[8],clipped[8];
  for(int i=0;i<4;i++){const Source*s=source+q->first+i;Vertex v={0};if(q->flags&1){v.p[0]=(s->x-320)*(float)s->z;v.p[1]=(240-s->y)*(float)s->z;v.p[2]=s->z;}else{int32_t local[3]={s->x,s->y,s->z};for(int c=0;c<3;c++){int64_t sum=0;for(int r=0;r<3;r++)sum+=(int64_t)local[r]*in->m[r*3+c];v.p[c]=(float)((sum>>15)+in->t[c]);}}v.uv[0]=s->u*q->uv[0]+q->uv[2];v.uv[1]=s->v*q->uv[1]+q->uv[3];v.source_uv[0]=s->u*q->source_uv[0]+q->source_uv[2];v.source_uv[1]=s->v*q->source_uv[1]+q->source_uv[3];v.brightness=s->bri/64.f;v.depth=q->depth;v.alpha=q->alpha;memcpy(v.projection,in->projection,sizeof v.projection);p[i]=v;}
  int count=0;for(int i=0;i<4;i++){Vertex a=p[i],b=p[(i+1)%4];int av=a.p[2]>=1,bv=b.p[2]>=1;if(av)clipped[count++]=a;if(av!=bv){CHECK(count<8);clipped[count++]=lerp(a,b,(1-a.p[2])/(b.p[2]-a.p[2]));}}
  if(count<3){fully_clipped++;continue;}if(count!=4)near_clipped++;CHECK(count<=6);
  CHECK(nd<LIMIT);Draw*d=draws+nd++;d->first=nv;d->page=q->page;d->material=q->material;d->ordinal=qi;d->protected=!!(q->flags&2);memcpy(d->box,q->box,sizeof d->box);
  for(int t=1;t+1<count;t++){append(clipped[0]);append(clipped[t]);append(clipped[t+1]);}d->count=nv-d->first;
 }
 Draw*sorted=malloc(nd*sizeof(Draw));CHECK(sorted);memcpy(sorted,draws,nd*sizeof(Draw));qsort(sorted,nd,sizeof(Draw),compare);indices=malloc(nv*4);CHECK(indices);unsigned cursor=0;
 for(unsigned i=0;i<nd;i++){Draw*d=sorted+i;int merge=ng&&!d->protected&&!groups[ng-1].protected&&groups[ng-1].page==d->page&&!memcmp(groups[ng-1].box,d->box,sizeof d->box);if(merge)groups[ng-1].count+=d->count;else{groups[ng]=*d;groups[ng++].first=cursor;}for(unsigned k=0;k<d->count;k++)indices[cursor++]=d->first+k;}CHECK(cursor==nv);free(sorted);
 glGenBuffers(1,&vbo);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBufferData(GL_ARRAY_BUFFER,nv*sizeof(Vertex),vertices,GL_STATIC_DRAW);glGenBuffers(1,&ebo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo);glBufferData(GL_ELEMENT_ARRAY_BUFFER,nv*4,indices,GL_STATIC_DRAW);
 printf("GEOMETRY source_vertices=%u instances=%u source_quads=%u native_quads=%u native_vertices=%u grouped_draws=%u near_clipped=%u fully_clipped=%u gpu_vertex_bytes=%zu gpu_index_bytes=%u steady_upload_bytes=0\n",ns,ni,nq,nd,nv,ng,near_clipped,fully_clipped,nv*sizeof(Vertex),nv*4);
}
static void load(const char*dir){FILE*f=openfile(dir,"scene.meta");char magic[32];CHECK(fscanf(f,"%31s %u %u %u %u %u %f %f %f",magic,&np,&ns,&ni,&nq,&nm,bg,bg+1,bg+2)==9);CHECK(!strcmp(magic,"RRNATIVE1")&&np>0&&np<PAGE_LIMIT&&nq<LIMIT&&ns<=1000000&&ni<LIMIT&&nm<MAT_LIMIT);for(unsigned i=0;i<np;i++)CHECK(fscanf(f,"%d %d",widths+i,heights+i)==2);fclose(f);source=readfile(dir,"models.bin",ns*sizeof *source);instances=readfile(dir,"instances.bin",ni*sizeof *instances);quads=readfile(dir,"quads.bin",nq*sizeof *quads);materials=readfile(dir,"textures.bin",nm*sizeof *materials);
 for(unsigned page=0;page<np;page++){CHECK(widths[page]>0&&widths[page]<=8192&&heights[page]>0&&heights[page]<=8192);char name[80];snprintf(name,sizeof name,"page-%u.rgba",page);size_t bytes=(size_t)widths[page]*heights[page]*4;unsigned char*image=readfile(dir,name,bytes);page_bytes+=bytes;pages[page]=texture(widths[page],heights[page],image);
  for(unsigned i=0;i<nm;i++){Material*m=materials+i;if(m->page!=page)continue;CHECK(m->x+m->w<=(unsigned)widths[page]&&m->y+m->h<=(unsigned)heights[page]&&m->w&&m->h);int needed=0;for(unsigned j=0;j<nq;j++)if(quads[j].material==i&&(!selection||quads[j].code==1280||quads[j].code==1945)){needed=1;break;}if(!needed)continue;int same=-1;for(unsigned j=0;j<i;j++)if(source_textures[j]&&!memcmp(materials+j,m,sizeof *m)){same=j;break;}if(same>=0){source_textures[i]=source_textures[same];continue;}size_t size=(size_t)m->w*m->h*4;unsigned char*cut=malloc(size);CHECK(cut);for(unsigned y=0;y<m->h;y++)memcpy(cut+y*m->w*4,image+((m->y+y)*widths[page]+m->x)*4,m->w*4);source_textures[i]=texture(m->w,m->h,cut);source_texture_bytes+=size;free(cut);
  }free(image);
 }prepare();printf("MATERIALS native_pages=%u native_page_bytes=%zu comparison_texture_bytes=%zu\n",np,page_bytes,source_texture_bytes);
}
static void render(int mode,double*cpu,double*submission,double*completion){double begin=now(CLOCK_MONOTONIC),c=now(CLOCK_THREAD_CPUTIME_ID);glBindFramebuffer(GL_FRAMEBUFFER,fb);glViewport(0,0,640,480);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glDisable(GL_SCISSOR_TEST);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_TRUE);glClearDepthf(1);glClearColor(bg[0],bg[1],bg[2],0);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glEnable(GL_SCISSOR_TEST);glUseProgram(program);glUniform1i(glGetUniformLocation(program,"image"),0);glActiveTexture(GL_TEXTURE0);glBindBuffer(GL_ARRAY_BUFFER,vbo);glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ebo);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,56,(void*)0);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,56,(void*)(uintptr_t)(mode?12:20));glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,56,(void*)28);glVertexAttribPointer(3,4,GL_FLOAT,GL_FALSE,56,(void*)40);for(int i=0;i<4;i++)glEnableVertexAttribArray(i);Draw*list=mode?groups:draws;unsigned count=mode?ng:nd;for(unsigned i=0;i<count;i++){Draw*d=list+i;glBindTexture(GL_TEXTURE_2D,mode?pages[d->page]:source_textures[d->material]);glScissor(d->box[0],d->box[1],d->box[2],d->box[3]);if(mode)glDrawElements(GL_TRIANGLES,d->count,GL_UNSIGNED_INT,(void*)((size_t)d->first*4));else glDrawArrays(GL_TRIANGLES,d->first,d->count);}*cpu=now(CLOCK_THREAD_CPUTIME_ID)-c;*submission=now(CLOCK_MONOTONIC)-begin;glFinish();*completion=now(CLOCK_MONOTONIC)-begin;CHECK(glGetError()==GL_NO_ERROR);}
int main(int argc,char**argv){CHECK(argc==4);int samples=atoi(argv[2]);selection=atoi(argv[3]);CHECK(samples>=0&&samples<=200&&(selection==0||selection==1));double start=now(CLOCK_MONOTONIC);CHECK(!SDL_Init(SDL_INIT_VIDEO));SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_Window*w=SDL_CreateWindow("Native local meshes",0,0,640,480,SDL_WINDOW_OPENGL);CHECK(w&&SDL_GL_CreateContext(w));SDL_GL_SetSwapInterval(0);const char*extensions=(char*)glGetString(GL_EXTENSIONS);CHECK(strstr((char*)glGetString(GL_VERSION),"OpenGL ES 3")||strstr(extensions,"GL_OES_element_index_uint"));GLint limit,attrs;glGetIntegerv(GL_MAX_TEXTURE_SIZE,&limit);glGetIntegerv(GL_MAX_VERTEX_ATTRIBS,&attrs);CHECK(limit>=4096&&attrs>=4);printf("GPU %s VERSION %s max_texture=%d attrs=%d\n",glGetString(GL_RENDERER),glGetString(GL_VERSION),limit,attrs);load(argv[1]);program=shader_program();GLuint target=texture(640,480,NULL),depth;glGenFramebuffers(1,&fb);glBindFramebuffer(GL_FRAMEBUFFER,fb);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,target,0);glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24_OES,640,480);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);glFinish();printf("LOAD wall_ms=%.6f selection=%d\n",(now(CLOCK_MONOTONIC)-start)*1000,selection);unsigned char*base=malloc(640*480*4),*pixels=malloc(640*480*4);CHECK(base&&pixels);double cpu,sub,complete;unsigned changed=0,maxerr=0;
 for(int mode=0;mode<2;mode++){render(mode,&cpu,&sub,&complete);glReadPixels(0,0,640,480,GL_RGBA,GL_UNSIGNED_BYTE,pixels);if(!mode)memcpy(base,pixels,640*480*4);else{for(unsigned i=0;i<640*480;i++)changed+=memcmp(base+i*4,pixels+i*4,4)!=0;for(unsigned i=0;i<640*480*4;i++){unsigned err=abs((int)base[i]-(int)pixels[i]);if(err>maxerr)maxerr=err;}}char name[80];snprintf(name,sizeof name,"%s.rgba",mode?"native":"source");FILE*f=fopen(name,"wbx");CHECK(f&&fwrite(pixels,4,640*480,f)==640*480);fclose(f);}
 printf("COMPARE changed_pixels=%u maximum_channel_error=%u\n",changed,maxerr);
 for(int warm=0;warm<5;warm++)for(int mode=0;mode<2;mode++)render(mode,&cpu,&sub,&complete);
 for(int sample=0;sample<samples;sample++)for(int step=0;step<2;step++){int mode=(sample+step)%2;render(mode,&cpu,&sub,&complete);printf("TIMING sample=%d mode=%d cpu_ms=%.6f submission_ms=%.6f completion_ms=%.6f\n",sample,mode,cpu*1000,sub*1000,complete*1000);}
 struct rusage usage;getrusage(RUSAGE_SELF,&usage);printf("RSS max_kib=%ld\n",usage.ru_maxrss);SDL_Quit();return 0;}
