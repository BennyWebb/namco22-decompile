#include "trace.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
extern unsigned g_eng_frame;
#define N 32768
/* Identity is GL name + allocation serial; names can be recycled. */
typedef struct {GLuint id;unsigned serial,w,h,layers,updates[2],refs[2];double last[2];} resource;
static resource rs[N];static unsigned serial;static GLuint bound,sync_object;static FILE *logfile;static int initialized;static double start;
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
static resource *get(GLuint id){unsigned k=id%N;for(unsigned n=0;n<N;n++,k=(k+1)%N){if(rs[k].id==id)return rs+k;if(!rs[k].id){rs[k].id=id;return rs+k;}}abort();}
static void init(void){if(initialized)return;initialized=1;const char *p=getenv("ENG_MALI_CALL_TRACE");if(!p)return;logfile=fopen(p,"wx");if(!logfile){perror(p);exit(2);}setvbuf(logfile,NULL,_IOFBF,1<<20);start=now();fputs("tick,op,id,serial,w,h,layers,layer,bytes,wall_us,time_us,updates,refs,layer_age_us,other_age_us\n",logfile);}
typedef struct {unsigned tick;const char *op;resource r;int layer;size_t bytes;double wall,time,age,other;} event;
static event events[16384];static unsigned event_n;
static const char *ops[]={"gen","delete","image2","sub2","image3","sub3","poll_pending","poll_signalled","fence","delete_sync","reap_scope","search_scope","bind_sample","invalidate"};
static struct {double wall;size_t bytes;unsigned n;} totals[6100][14];
static void record(const char *op,GLuint id,int layer,size_t bytes,double t){init();if(!logfile)return;resource *r=get(id);double end=now();int l=layer<0?0:layer;
 if(g_eng_frame>=1960 && g_eng_frame<=1970 && event_n<16384){event *e=events+event_n++;e->tick=g_eng_frame;e->op=op;e->r=*r;e->layer=layer;e->bytes=bytes;e->wall=end-t;e->time=t-start;e->age=r->last[l]?t-r->last[l]:-1;e->other=r->last[1-l]?t-r->last[1-l]:-1;}
 else if(g_eng_frame<6100){for(unsigned k=0;k<14;k++)if(!strcmp(op,ops[k])){totals[g_eng_frame][k].wall+=end-t;totals[g_eng_frame][k].bytes+=bytes;totals[g_eng_frame][k].n++;break;}}
}
void mt_scope(const char *op,double t){record(op,bound,-1,0,t);}
void mt_reference(GLuint id,int layer){init();if(!logfile)return;resource*r=get(id);if(layer<0||layer>1)layer=0;r->refs[layer]++;r->last[layer]=now();}
static unsigned bind_count;
void mt_bind(GLenum target,GLuint id){bound=id;if((bind_count++&63)==0){double t=now();glBindTexture(target,id);record("bind_sample",id,-1,1,t);}else glBindTexture(target,id);}
void mt_gen(GLsizei n,GLuint *ids){double t=now();glGenTextures(n,ids);record("gen",ids[0],-1,n,t);}
void mt_delete(GLsizei n,const GLuint *ids){double t=now();glDeleteTextures(n,ids);resource *first=get(ids[0]);record("delete",ids[0],-1,(size_t)first->w*first->h*first->layers*4,t);for(int i=0;i<n;i++){resource*r=get(ids[i]);memset(r->updates,0,sizeof r->updates);memset(r->refs,0,sizeof r->refs);memset(r->last,0,sizeof r->last);}}
void mt_image2(GLenum a,GLint b,GLint c,GLsizei w,GLsizei h,GLint d,GLenum e,GLenum f,const void*p){resource*r=get(bound);r->w=w;r->h=h;r->layers=1;r->serial=++serial;double t=now();glTexImage2D(a,b,c,w,h,d,e,f,p);record("image2",bound,0,(size_t)w*h*4,t);}
void mt_sub2(GLenum a,GLint b,GLint x,GLint y,GLsizei w,GLsizei h,GLenum e,GLenum f,const void*p){double t=now();glTexSubImage2D(a,b,x,y,w,h,e,f,p);record("sub2",bound,0,(size_t)w*h*4,t);get(bound)->updates[0]++;}
void mt_image3(GLenum a,GLint b,GLint c,GLsizei w,GLsizei h,GLsizei z,GLint d,GLenum e,GLenum f,const void*p){resource*r=get(bound);r->w=w;r->h=h;r->layers=z;r->serial=++serial;double t=now();glTexImage3D(a,b,c,w,h,z,d,e,f,p);record("image3",bound,-1,(size_t)w*h*z*4,t);}
void mt_sub3(GLenum a,GLint b,GLint x,GLint y,GLint z,GLsizei w,GLsizei h,GLsizei n,GLenum e,GLenum f,const void*p){double t=now();glTexSubImage3D(a,b,x,y,z,w,h,n,e,f,p);record("sub3",bound,z,(size_t)w*h*n*4,t);get(bound)->updates[z]++;}
__attribute__((destructor))static void close_log(void){if(!logfile)return;
for(unsigned i=0;i<event_n;i++){event *e=events+i;int l=e->layer<0?0:e->layer;fprintf(logfile,"%u,%s,%u,%u,%u,%u,%u,%d,%zu,%.3f,%.3f,%u,%u,%.3f,%.3f\n",e->tick,e->op,e->r.id,e->r.serial,e->r.w,e->r.h,e->r.layers,e->layer,e->bytes,e->wall*1e6,e->time*1e6,e->r.updates[l],e->r.refs[l],e->age<0?-1:e->age*1e6,e->other<0?-1:e->other*1e6);}
for(unsigned t=0;t<6100;t++)for(unsigned k=0;k<14;k++)if(totals[t][k].n)fprintf(logfile,"%u,%s,0,0,0,0,0,-1,%zu,%.3f,0,%u,0,-1,-1\n",t,ops[k],totals[t][k].bytes,totals[t][k].wall*1e6,totals[t][k].n);
fclose(logfile);}


GLenum mt_wait(GLsync s,GLbitfield f,GLuint64 ns){double t=now();GLenum result=glClientWaitSync(s,f,ns);record(result==GL_TIMEOUT_EXPIRED?"poll_pending":"poll_signalled",sync_object,-1,ns,t);return result;}
GLsync mt_fence(GLenum c,GLbitfield f){double t=now();GLsync s=glFenceSync(c,f);record("fence",sync_object,-1,0,t);return s;}
void mt_delete_sync(GLsync s){double t=now();glDeleteSync(s);record("delete_sync",sync_object,-1,0,t);}

void mt_sync_object(GLuint id){sync_object=id;}
void mt_invalidate(GLuint id,int layer,unsigned mask){double t=now();record("invalidate",id,layer,mask,t);}
