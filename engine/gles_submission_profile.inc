/* Sparse diagnostic only. No timers/counters in headline builds. */
#ifdef RR_GLES_SUBMISSION_PROFILE
#define SP_MAX 8192
static const char *sp_names[]={"setup","attributes","state","draw","vertex","query"};
static struct { unsigned calls,n[2]; double value[2][SP_MAX],raw[2],empty[2]; } sp_data[6];
static unsigned sp_from=~0u,sp_to,sp_rng=0x35268000u,sp_kind_rng=0x12345678u;
static unsigned long long sp_prep_calls;
static double sp_prep_wall;
static double sp_now(clockid_t clock) {struct timespec t;clock_gettime(clock,&t);return t.tv_sec+t.tv_nsec*1e-9;}
typedef struct {int id,kind;double start,empty;} sp_timer;
__attribute__((constructor)) static void sp_init(void) {
 const char *s=getenv("GLES_PROFILE_FROM");
 if(s){sp_from=strtoul(s,NULL,10);s=getenv("GLES_PROFILE_TO");sp_to=s?strtoul(s,NULL,10):~0u;}
}
static void sp_prep(double elapsed) {
 if(g_eng_frame>=sp_from && g_eng_frame<=sp_to){sp_prep_calls++;sp_prep_wall+=elapsed;}
}
static sp_timer sp_begin(int id,int selected) {
 if(g_eng_frame<sp_from || g_eng_frame>sp_to)return (sp_timer){.id=-1};
 sp_data[id].calls++;
 if(!selected)return (sp_timer){.id=-1};
 sp_kind_rng=sp_kind_rng*1664525u+1013904223u;
 int kind=sp_kind_rng>>31;
 if(sp_data[id].n[kind]==SP_MAX)return (sp_timer){.id=-1};
 clockid_t clock=kind?CLOCK_MONOTONIC:CLOCK_THREAD_CPUTIME_ID;
 /* Pair the empty interval immediately with the operation at the same CPU
  * frequency. Never nest CPU clocks inside a measured wall interval. */
 double a=sp_now(clock),b=sp_now(clock),start=sp_now(clock);
 return (sp_timer){id,kind,start,b-a};
}
static void sp_end(sp_timer t) {
 if(t.id<0)return;
 double elapsed=sp_now(t.kind?CLOCK_MONOTONIC:CLOCK_THREAD_CPUTIME_ID)-t.start;
 unsigned n=sp_data[t.id].n[t.kind]++;
 sp_data[t.id].value[t.kind][n]=elapsed-t.empty;
 sp_data[t.id].raw[t.kind]+=elapsed;
 sp_data[t.id].empty[t.kind]+=t.empty;
}
static int sp_compare(const void *a,const void *b){double x=*(const double*)a,y=*(const double*)b;return (x>y)-(x<y);}
__attribute__((destructor)) static void sp_report(void) {
 if(sp_from==~0u)return;
 fprintf(stderr,"[SUBMIT-PREP] commands=%llu wall_total_ms=%.6f scope=frame_command_construction_existing_timers\n",sp_prep_calls,sp_prep_wall*1000);
 fprintf(stderr,"[SUBMIT-TIMER] sampling_probability=1/32 samples_cap=%d calibration=paired_live CPU_and_wall_separate_cohorts; small_negative_estimates_are_below_resolution\n",SP_MAX);
 for(int id=0;id<6;id++)for(int kind=0;kind<2;kind++) {
  unsigned n=sp_data[id].n[kind];if(!n)continue;
  qsort(sp_data[id].value[kind],n,sizeof(double),sp_compare);
  double mean=(sp_data[id].raw[kind]-sp_data[id].empty[kind])/n;
  fprintf(stderr,"[SUBMIT-OP] op=%s clock=%s calls=%u samples=%u raw_mean_us=%.6f empty_mean_us=%.6f adjusted_mean_us=%.6f adjusted_median_us=%.6f adjusted_p95_us=%.6f estimated_total_ms=%.6f\n",sp_names[id],kind?"wall":"cpu",sp_data[id].calls,n,sp_data[id].raw[kind]*1e6/n,sp_data[id].empty[kind]*1e6/n,mean*1e6,sp_data[id].value[kind][n/2]*1e6,sp_data[id].value[kind][(n-1)*95/100]*1e6,mean*sp_data[id].calls*1000);
 }
}
#define SP_BEGIN(name,id,selected) sp_timer name=sp_begin(id,selected)
#define SP_END(name) sp_end(name)
static int sp_sample(void){sp_rng=sp_rng*1664525u+1013904223u;return (sp_rng>>27)==0;}
#define SP_SAMPLE sp_sample()
#define SP_PREP(elapsed) sp_prep(elapsed)
#else
#define SP_BEGIN(name,id,selected)
#define SP_END(name)
#define SP_SAMPLE 0
#define SP_PREP(elapsed)
#endif
