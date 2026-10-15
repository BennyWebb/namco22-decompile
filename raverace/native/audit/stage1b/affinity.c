/* Diagnostic preload only. No thread creation/scheduling-policy changes.
 * Unset RR_AFFINITY: no pthread_setaffinity_np/sched_setaffinity calls.
 * Example: RR_AFFINITY=main:0,prep:1,present:2
 * Existing rr-prepare/rr-present name calls identify only those workers. */
#define _GNU_SOURCE
#include <pthread.h>
#include <sched.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int cores[3]={-1,-1,-1};
static int enabled;static cpu_set_t original_mask;
static void pin(int role){if(cores[role]<0)return;cpu_set_t set;CPU_ZERO(&set);CPU_SET(cores[role],&set);int r=pthread_setaffinity_np(pthread_self(),sizeof set,&set);fprintf(stderr,"[STAGE1B-AFFINITY] role=%d cpu=%d result=%d\n",role,cores[role],r);if(r)_exit(96);}
__attribute__((constructor))static void init(void){const char*s=getenv("RR_AFFINITY");if(!s||!*s)return;enabled=1;if(pthread_getaffinity_np(pthread_self(),sizeof original_mask,&original_mask))_exit(96);char*copy=strdup(s),*save=NULL;if(!copy)_exit(96);for(char*t=strtok_r(copy,",",&save);t;t=strtok_r(NULL,",",&save)){char name[32],extra;int core;if(sscanf(t,"%31[^:]:%d%c",name,&core,&extra)!=2||core<0||core>=4)_exit(96);int role=!strcmp(name,"main")?0:!strcmp(name,"prep")?1:!strcmp(name,"present")?2:-1;if(role<0||cores[role]>=0)_exit(96);cores[role]=core;}free(copy);pin(0);}
int pthread_setname_np(pthread_t thread,const char*name){int(*original)(pthread_t,const char*)=dlsym(RTLD_NEXT,"pthread_setname_np");if(!original)_exit(96);int r=original(thread,name);if(pthread_equal(thread,pthread_self())){if(!strcmp(name,"rr-prepare"))pin(1);else if(!strcmp(name,"rr-present"))pin(2);}return r;}

/* pthread children inherit their creator's mask. Restore the ORIGINAL allowed
 * mask at entry so audio/driver helpers do not accidentally inherit main:0 or
 * present:2. Named game workers subsequently apply their explicitly requested
 * masks. No new threads; unset forwards the identical original start routine. */
typedef struct{void*(*start)(void*);void*arg;} launch;
static void*entry(void*p){launch value=*(launch*)p;free(p);int r=pthread_setaffinity_np(pthread_self(),sizeof original_mask,&original_mask);if(r)_exit(96);return value.start(value.arg);}
int pthread_create(pthread_t*t,const pthread_attr_t*a,void*(*start)(void*),void*arg){int(*original)(pthread_t*,const pthread_attr_t*,void*(*)(void*),void*)=dlsym(RTLD_NEXT,"pthread_create");if(!original)_exit(96);if(!enabled)return original(t,a,start,arg);launch*p=malloc(sizeof *p);if(!p)_exit(96);*p=(launch){start,arg};int r=original(t,a,entry,p);if(r)free(p);return r;}
