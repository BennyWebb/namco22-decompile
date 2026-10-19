/* Optional exclusive stage accounting and bounded SIGPROF gles3-PC sampling.
 * No instruction hooks, allocations or output in hot loops/signal handler. */
#define _GNU_SOURCE
#include "rr_sim_profile.h"
#include "rr_mem.h"
#include "eng_audit.h"
#include "rr_gles3.h"
static eng_audit_scope audit_sim;
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <time.h>
#include <ucontext.h>
#include <unistd.h>
#define MAX_TICKS 8192
#define MAX_SAMPLES 65536
static bool enabled, active, started, reported, rate_requested;
static double rate_start, rate_end;
static unsigned rate_steps,rate_renders;
static uint32_t from=1400, current_frame=1;
static const char *sample_path;
static const char *names[SIM_COUNT]={"translated68k_inclusive_memory","scheduler","dsp_inclusive_pdp","sound_inclusive_bus","c352_mix","serial","board_io","irq_inclusive_handlers","video","presentation"};
static double last, tick_start, cpu_start, sums[SIM_COUNT];
static rr_sim_stage stage=SIM_CORE;
typedef struct { double time[SIM_COUNT], wall, cpu; } tick_record;
static tick_record *records;
static unsigned count, overflow;
static volatile sig_atomic_t sample_count, sample_overflow, signal_stage, signal_frame;
static pid_t main_tid;
typedef struct { uintptr_t pc; uint32_t frame, stage; } pc_sample;
static pc_sample *samples;
static struct sigaction previous_action;
static struct itimerval previous_timer;
static double now(clockid_t clock) { struct timespec t; clock_gettime(clock,&t); return t.tv_sec+t.tv_nsec*1e-9; }
static void sample(int sig, siginfo_t *info, void *context)
{
    int saved_errno=errno;
    (void)sig; (void)info;
    if (syscall(SYS_gettid)!=main_tid || signal_frame<(sig_atomic_t)from) { errno=saved_errno; return; }
    sig_atomic_t n=sample_count;
    if(n>=MAX_SAMPLES) { sample_overflow++; errno=saved_errno; return; }
    ucontext_t *u=context;
#if defined(__aarch64__)
    uintptr_t pc=u->uc_mcontext.pc;
#elif defined(__x86_64__)
    uintptr_t pc=u->uc_mcontext.gregs[REG_RIP];
#else
#error Unsupported gles3 sampling architecture
#endif
    samples[n]=(pc_sample){pc,(uint32_t)signal_frame,(uint32_t)signal_stage};
    sample_count=n+1; errno=saved_errno;
}
bool rr_sim_options(int *argc,char **argv)
{
    int output=1;
    for(int i=1;i<*argc;i++) {
        if(!strcmp(argv[i],"--profile-simulation")) enabled=true;
        else if(!strcmp(argv[i],"--sample-simulation") && i+1<*argc) sample_path=argv[++i];
        else if(!strcmp(argv[i],"--simulation-profile-from") && i+1<*argc) {
            char *end; const char *n=argv[++i]; errno=0; unsigned long v=strtoul(n,&end,10);
            if(errno || n==end || *n<'0' || *n>'9' || *end || !v || v>INT32_MAX) return false;
            from=(uint32_t)v;rate_requested=true;
        } else argv[output++]=argv[i];
    }
    *argc=output;argv[output]=NULL;
    if(sample_path) enabled=true;
    if(enabled) rate_requested=true;
    return true;
}
bool rr_sim_start(void)
{
    if(!enabled) { rr_sim_resume(1); return true; }
    records=calloc(MAX_TICKS,sizeof *records);
    if(!records) { perror("simulation profile records"); return false; }
    if(sample_path) {
        samples=calloc(MAX_SAMPLES,sizeof *samples);
        if(!samples) { perror("simulation profile samples"); return false; }
        char maps_path[1024];
        if(snprintf(maps_path,sizeof maps_path,"%s.maps",sample_path)>=(int)sizeof maps_path) return false;
        FILE *in=fopen("/proc/self/maps","r"),*out=fopen(maps_path,"w");
        if(!in || !out) { if(in)fclose(in);if(out)fclose(out);perror("sample maps");return false; }
        char line[2048];while(fgets(line,sizeof line,in))fputs(line,out);fclose(in);fclose(out);
        struct sigaction action={0}; action.sa_sigaction=sample;action.sa_flags=SA_SIGINFO|SA_RESTART;
        sigemptyset(&action.sa_mask);main_tid=(pid_t)syscall(SYS_gettid);
        getitimer(ITIMER_PROF,&previous_timer);
        if(previous_timer.it_value.tv_sec || previous_timer.it_value.tv_usec) {
            fprintf(stderr,"[SIM] existing SIGPROF timer; refusing sampling\n");return false;
        }
        if(sigaction(SIGPROF,&action,&previous_action)) { perror("SIGPROF");return false; }
        struct itimerval timer={{0,5000},{0,5000}};
        if(setitimer(ITIMER_PROF,&timer,NULL)) { perror("ITIMER_PROF");return false; }
    }
    started=true;rr_sim_resume(1);rr_sim_enter(SIM_CORE);return true;
}
rr_sim_stage rr_sim_enter(rr_sim_stage next)
{
    rr_sim_stage old=stage;
    if(enabled && started) {
        double t=now(CLOCK_MONOTONIC);
        if(active) sums[stage]+=t-last;
        last=t;stage=next;signal_stage=next;
    }
    return old;
}
void rr_sim_leave(rr_sim_stage old) { (void)rr_sim_enter(old); }
void rr_sim_pause(uint32_t frame)
{
    eng_audit_end(audit_sim);
    if(!enabled || !started) return;
    double cpu=now(CLOCK_THREAD_CPUTIME_ID),t=now(CLOCK_MONOTONIC);
    sums[stage]+=t-last;
    if(frame>=from) {
        if(count<MAX_TICKS) {
            tick_record *r=&records[count++]; memcpy(r->time,sums,sizeof sums);
            r->wall=t-tick_start;r->cpu=cpu-cpu_start;
        } else overflow++;
    }
    active=false;stage=SIM_VIDEO;signal_stage=stage;signal_frame=(sig_atomic_t)frame;
}
void rr_sim_resume(uint32_t next_frame)
{
#ifdef RR_PIPELINE_AUDIT
    if(eng_audit_enabled && next_frame==rr_gles3_profile_from()) eng_audit_pmu_start();
#endif
    audit_sim=eng_audit_begin(AUD_SIM);
    rr_memory_profile_frame(next_frame);
    if(rate_requested) {
        double t=now(CLOCK_MONOTONIC);
        if(next_frame==from) rate_start=t;
        if(next_frame>from) rate_end=t;
    }
    if(!enabled || !started) return;
    memset(sums,0,sizeof sums);current_frame=next_frame;signal_frame=(sig_atomic_t)current_frame;
    stage=SIM_SCHEDULER;signal_stage=stage;
    last=tick_start=now(CLOCK_MONOTONIC);cpu_start=now(CLOCK_THREAD_CPUTIME_ID);active=true;
}
static int compare(const void *a,const void *b) { double x=*(const double *)a,y=*(const double *)b;return (x>y)-(x<y); }
void rr_sim_present_tick(uint32_t frame, bool rendered)
{
    if(rate_requested && frame>=from) { rate_steps++;rate_renders+=rendered; }
}
void rr_sim_report(void)
{
    if(reported) return;
    reported=true;
    if(rate_requested && rate_steps && rate_end>rate_start)
        fprintf(stderr,"[SIM-RATE] from=%u steps=%u rendered=%u skipped=%u wall_s=%.6f sim_hz=%.6f render_fps=%.6f\n",
            from,rate_steps,rate_renders,rate_steps-rate_renders,rate_end-rate_start,
            rate_steps/(rate_end-rate_start),rate_renders/(rate_end-rate_start));
    if(!enabled || !started) return;
    if(sample_path) {
        sigset_t set,old;sigemptyset(&set);sigaddset(&set,SIGPROF);sigprocmask(SIG_BLOCK,&set,&old);
        struct itimerval zero={0};setitimer(ITIMER_PROF,&zero,NULL);
        FILE *f=fopen(sample_path,"w");
        if(f) {
            fputs("frame\tstage\tgles3_pc\n",f);
            for(int i=0;i<sample_count;i++) fprintf(f,"%u\t%s\t0x%llx\n",samples[i].frame,names[samples[i].stage],(unsigned long long)samples[i].pc);
            fclose(f);
        } else perror("simulation samples output");
        /* Retain blocked SIGPROF until process exit: pending signals must not
         * invoke a previous/default handler after this report. */
        fprintf(stderr,"[SIM-SAMPLES] samples=%d overflow=%d requested_interval_us=5000 file=%s\n",sample_count,sample_overflow,sample_path);
    }
    double *values=malloc((count?count:1)*sizeof *values);
    if(!values) { perror("simulation profile report");return; }
    double overall=0,reconcile=0;
    for(unsigned i=0;i<count;i++) {
        overall+=records[i].wall; double sum=0;
        for(int k=0;k<SIM_COUNT;k++) sum+=records[i].time[k];
        double delta=sum-records[i].wall;if(delta<0)delta=-delta;if(delta>reconcile)reconcile=delta;
    }
    fprintf(stderr,"[SIM] ticks=%u from=%u overflow=%u reconciliation_max_us=%.6f\n",count,from,overflow,reconcile*1e6);
    for(int k=0;k<SIM_COUNT+2;k++) {
        if(k==SIM_VIDEO || k==SIM_PRESENT) continue; /* outside simulation; existing STAGES profiles these */
        double sum=0;
        for(unsigned i=0;i<count;i++) { double v=k<SIM_COUNT?records[i].time[k]:k==SIM_COUNT?records[i].wall:records[i].cpu;values[i]=v*1000;sum+=v; }
        qsort(values,count,sizeof *values,compare);
        fprintf(stderr,"[SIM-STAGE] name=%s n=%u avg_ms=%.6f median_ms=%.6f p95_ms=%.6f max_ms=%.6f percent=%.3f\n",
            k<SIM_COUNT?names[k]:k==SIM_COUNT?"simulation_wall":"simulation_thread_cpu",count,
            count?sum*1000/count:0,count?(values[(count-1)/2]+values[count/2])*0.5:0,count?values[((size_t)count*95+99)/100-1]:0,
            count?values[count-1]:0,overall?sum*100/overall:0);
    }
    free(values);free(records);free(samples);
}
