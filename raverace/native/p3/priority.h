/* Native-only opt-in. Startup/audio/driver creation precedes promotion.
 * Each call affects the calling TID only. No global scheduler/RT knobs change. */
#include <sys/resource.h>
#include <sched.h>
#include <errno.h>
static void p3_priority(unsigned tick,int role){
 static _Thread_local int applied;
 if(applied||tick<1800)return;applied=1;
 const char*mode=getenv("ENG_P3_PRIORITY");if(!mode||!*mode||!strcmp(mode,"off"))return;
 int nice_value=role==1?-5:-10,error=0,policy=SCHED_OTHER;struct sched_param param={0};
 if(!strcmp(mode,"rr")&&role!=1){policy=SCHED_RR;param.sched_priority=5;if(pthread_setschedparam(pthread_self(),policy,&param))error=1;}
 else if(!strcmp(mode,"nice")||(!strcmp(mode,"rr")&&role==1)){if(setpriority(PRIO_PROCESS,(id_t)syscall(SYS_gettid),nice_value))error=errno;}
 else error=EINVAL;
 struct sched_param actual={0};int got=-1;pthread_getschedparam(pthread_self(),&got,&actual);
 fprintf(stderr,"[P3-PRIORITY] tick=%u role=%d tid=%ld mode=%s policy=%d rt=%d nice=%d error=%d\n",tick,role,(long)syscall(SYS_gettid),mode,got,actual.sched_priority,getpriority(PRIO_PROCESS,(id_t)syscall(SYS_gettid)),error);
 if(error)abort();
}
