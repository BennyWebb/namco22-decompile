/* Isolated M3 support. All timestamps/samples retained; no simulation linked. */
#include <stdarg.h>
#include <math.h>
static int native_front3,native_fold3,native_ram3;
static char trace3[16*1024*1024];static size_t trace_used3;
static double trace_wall3,trace_cpu3,check_wall3,check_cpu3,end_cpu3;
static struct rusage end_usage3;
typedef struct {unsigned tick;double gap,cpu,check,checkcpu,log,logcpu,release,releasecpu,residual;long voluntary,involuntary;} Gap3;
static Gap3 gaps3[3840*5];static unsigned gaps_count3;
static double native_stamp3(clockid_t k){struct timespec t;clock_gettime(k,&t);return t.tv_sec+t.tv_nsec/1e9;}
static int native_printf3(const char*fmt,...){
 double wall=native_stamp3(CLOCK_MONOTONIC),cpu=native_stamp3(CLOCK_THREAD_CPUTIME_ID);va_list args;va_start(args,fmt);int n;
 if(native_ram3){size_t left=sizeof trace3-trace_used3;n=vsnprintf(trace3+trace_used3,left,fmt,args);if(n<0||(size_t)n>=left){fprintf(stderr,"bounded trace exhausted\n");exit(2);}trace_used3+=(size_t)n;}else n=vfprintf(stdout,fmt,args);
 va_end(args);trace_wall3+=native_stamp3(CLOCK_MONOTONIC)-wall;trace_cpu3+=native_stamp3(CLOCK_THREAD_CPUTIME_ID)-cpu;return n;
}
static void native_flush3(void){double start=native_stamp3(CLOCK_MONOTONIC);if(native_ram3){if(fwrite(trace3,1,trace_used3,stdout)!=trace_used3)exit(2);}if(fflush(stdout))exit(2);fprintf(stderr,"TRACE_DRAIN mode=%s bytes=%zu wall_ms=%.6f outside_race=1\n",native_ram3?"ram":"sync",trace_used3,(native_stamp3(CLOCK_MONOTONIC)-start)*1000);}
#define printf native_printf3
