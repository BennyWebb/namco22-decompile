#include "eng_audit.h"
#include <string.h>
#include <time.h>
#include <linux/perf_event.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <errno.h>
bool eng_audit_enabled, eng_audit_no_draw;
bool eng_audit_flat_fragment;
bool eng_audit_detailed;
int eng_audit_divisor=1;
double eng_audit_wall[AUD_COUNT], eng_audit_cpu[AUD_COUNT];
double eng_audit_inclusive_wall[AUD_COUNT], eng_audit_inclusive_cpu[AUD_COUNT];
static int current=-1;
static double last_wall,last_cpu;
static double wall_now(void)
{ struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9; }
double eng_audit_cpu_now(void)
{ struct timespec t;clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return t.tv_sec+t.tv_nsec*1e-9; }
bool eng_audit_available(void)
{
#ifdef RR_PIPELINE_AUDIT
 return true;
#else
 return false;
#endif
}
eng_audit_scope eng_audit_begin_impl(eng_audit_stage stage)
{
 double cpu=eng_audit_cpu_now(),wall=wall_now();
 eng_audit_scope s={current,stage,wall,cpu};
 if(current>=0) {eng_audit_wall[current]+=wall-last_wall;eng_audit_cpu[current]+=cpu-last_cpu;}
 current=stage;last_wall=wall;last_cpu=cpu;return s;
}
void eng_audit_end_impl(eng_audit_scope s)
{
 double cpu=eng_audit_cpu_now(),wall=wall_now();
 eng_audit_wall[current]+=wall-last_wall;eng_audit_cpu[current]+=cpu-last_cpu;
 eng_audit_inclusive_wall[s.stage]+=wall-s.wall;eng_audit_inclusive_cpu[s.stage]+=cpu-s.cpu;
 current=s.parent;last_wall=wall;last_cpu=cpu;
}
void eng_audit_reset(void)
{
 memset(eng_audit_wall,0,sizeof eng_audit_wall);memset(eng_audit_cpu,0,sizeof eng_audit_cpu);
 memset(eng_audit_inclusive_wall,0,sizeof eng_audit_inclusive_wall);memset(eng_audit_inclusive_cpu,0,sizeof eng_audit_inclusive_cpu);
}
static int pmu_fd[4]={-1,-1,-1,-1};
static const char *pmu_names[]={"cycles","instructions","cache_misses","branch_misses"};
void eng_audit_pmu_start(void)
{
    const unsigned configs[]={PERF_COUNT_HW_CPU_CYCLES,PERF_COUNT_HW_INSTRUCTIONS,PERF_COUNT_HW_CACHE_MISSES,PERF_COUNT_HW_BRANCH_MISSES};
    for(int i=0;i<4;i++) {
        struct perf_event_attr a={0};a.type=PERF_TYPE_HARDWARE;a.size=PERF_ATTR_SIZE_VER3;a.config=configs[i];
        a.disabled=1;a.exclude_kernel=1;a.exclude_hv=1;
        a.read_format=PERF_FORMAT_TOTAL_TIME_ENABLED|PERF_FORMAT_TOTAL_TIME_RUNNING;
        pmu_fd[i]=(int)syscall(__NR_perf_event_open,&a,0,-1,-1,0);
        if(pmu_fd[i]<0) fprintf(stderr,"[PMU] name=%s unavailable=%d\n",pmu_names[i],errno);
        else {ioctl(pmu_fd[i],PERF_EVENT_IOC_RESET,0);ioctl(pmu_fd[i],PERF_EVENT_IOC_ENABLE,0);}
    }
}
void eng_audit_pmu_report(void)
{
    for(int i=0;i<4;i++) if(pmu_fd[i]>=0) {
        uint64_t values[3]={0};ioctl(pmu_fd[i],PERF_EVENT_IOC_DISABLE,0);
        if(read(pmu_fd[i],values,sizeof values)==sizeof values)
            fprintf(stderr,"[PMU] name=%s raw=%llu enabled_ns=%llu running_ns=%llu scope=main_thread_user_only\n",pmu_names[i],
                (unsigned long long)values[0],(unsigned long long)values[1],(unsigned long long)values[2]);
        close(pmu_fd[i]);pmu_fd[i]=-1;
    }
}
