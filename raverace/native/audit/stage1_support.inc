/* Stage1 diagnostics only. No gameplay/scheduling/renderer policy changes. */
#include "rr_presentation.h"
#include <sched.h>
#define S1_PARTS 14
static const char *s1_names[S1_PARTS]={"initialization","mesh_resolution","uv_brightness","material_palette_resolution","transforms_clip_protection","conservative_culling","direct_geometry","painter_sort","depth_assignment","batch_sort","instance_dedup","visible_sort","groups","packet_copy"};
typedef struct {unsigned tick,n,instances,visible,groups,order;int native_core,original_core,native_end_core,original_end_core;double copy_wall,copy_cpu,original_wall,original_cpu,walk_wall,walk_cpu,compile_wall,compile_cpu,emit_wall,parts[S1_PARTS],orig_parts[3];unsigned long marks;} S1Row;
static S1Row s1_rows[6006],*s1_row;static unsigned s1_count;static int s1_detail;static uint64_t s1_last,s1_parts[S1_PARTS],s1_emit;static int s1_part;static unsigned long s1_marks;static double s1_frequency;
double s1_original_parts[3];
static uint64_t s1_counter(void){
#ifdef __aarch64__
 uint64_t n;__asm__ volatile("isb; mrs %0,cntvct_el0":"=r"(n)::"memory");return n;
#else
 struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (uint64_t)t.tv_sec*1000000000ull+t.tv_nsec;
#endif
}
double s1_seconds(void){return s1_counter()/s1_frequency;}
static double s1_cpu(void){struct timespec t;clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return t.tv_sec+t.tv_nsec/1e9;}
static void s1_mark(int part){if(!s1_detail)return;uint64_t t=s1_counter();if(s1_part>=0)s1_parts[s1_part]+=t-s1_last;s1_last=t;s1_part=part;s1_marks++;}
static void s1_compile_begin(void){memset(s1_parts,0,sizeof s1_parts);s1_part=-1;s1_marks=0;s1_mark(0);s1_row->compile_cpu=s1_cpu();}
static void s1_compile_end(unsigned n,unsigned ni,unsigned nv,unsigned ng){s1_mark(-1);s1_row->compile_cpu=s1_cpu()-s1_row->compile_cpu;s1_row->n=n;s1_row->instances=ni;s1_row->visible=nv;s1_row->groups=ng;s1_row->marks=s1_marks;for(int i=0;i<S1_PARTS;i++)s1_row->parts[i]=s1_parts[i]/s1_frequency;}
static rr_presentation s1_snapshot;
static void s1_snapshot_copy(unsigned frame,bool walk){double cpu=s1_cpu(),start=s1_seconds();rr_presentation*s=&s1_snapshot;s->frame=frame;s->walk=walk;s->direct_count=rr_scene_direct_count();
#define S1_COPY(f) memcpy(s->f,g_rr.f,sizeof s->f)
 S1_COPY(poly);S1_COPY(pal);S1_COPY(mixer);S1_COPY(czram);S1_COPY(cgram);S1_COPY(text);S1_COPY(tilemapattr);
#undef S1_COPY
 s->point_valid=rr_dsp_copy_point(s->point);for(unsigned i=0;i<s->direct_count;i++)memcpy(s->direct[i],rr_scene_direct(i),56);s1_row->copy_wall=s1_seconds()-start;s1_row->copy_cpu=s1_cpu()-cpu;}
static void s1_original(void){double cpu=s1_cpu(),start=s1_seconds();s1_row->original_core=sched_getcpu();CHECK3(rr_gl_presentation_into(&s1_snapshot));rr_gl_presentation_release();s1_row->original_wall=s1_seconds()-start;s1_row->original_cpu=s1_cpu()-cpu;s1_row->original_end_core=sched_getcpu();memcpy(s1_row->orig_parts,s1_original_parts,sizeof s1_original_parts);}
static void s1_open(void){
#ifdef __aarch64__
 uint64_t f;__asm__ volatile("mrs %0,cntfrq_el0":"=r"(f));s1_frequency=f;
#else
 s1_frequency=1e9;
#endif
 s1_detail=getenv("ENG_STAGE1_DETAIL")&&atoi(getenv("ENG_STAGE1_DETAIL"));uint64_t values[10001];for(int i=0;i<10001;i++){uint64_t t=s1_counter();values[i]=s1_counter()-t;}for(int i=1;i<10001;i++){uint64_t x=values[i];int j=i;while(j&&values[j-1]>x){values[j]=values[j-1];j--;}values[j]=x;}fprintf(stderr,"[STAGE1] counter_hz=%.0f detail=%d counter_pair_median_ns=%.3f p95_ns=%.3f worst_ns=%.3f snapshot_bytes=%zu shadow=alternating-order no_new_threads=1\n",s1_frequency,s1_detail,values[5000]*1e9/s1_frequency,values[9500]*1e9/s1_frequency,values[10000]*1e9/s1_frequency,sizeof s1_snapshot);}
static void s1_close(void){static bool done;if(done)return;done=true;const char*p=getenv("ENG_STAGE1_CSV");CHECK3(p);FILE*f=fopen(p,"wx");CHECK3(f);fprintf(f,"tick,n,instances,visible,groups,original_first,native_core,original_core,native_end_core,original_end_core,copy_wall_ms,copy_cpu_ms,original_wall_ms,original_cpu_ms,walk_wall_ms,walk_cpu_ms,compile_wall_ms,compile_cpu_ms,emit_wall_ms,original_direct_ms,original_list_ms,original_sort_ms,marks");for(int i=0;i<S1_PARTS;i++)fprintf(f,",%s_ms",s1_names[i]);fputc('\n',f);for(unsigned j=0;j<s1_count;j++){S1Row*r=s1_rows+j;fprintf(f,"%u,%u,%u,%u,%u,%u,%d,%d,%d,%d",r->tick,r->n,r->instances,r->visible,r->groups,r->order,r->native_core,r->original_core,r->native_end_core,r->original_end_core);double v[]={r->copy_wall,r->copy_cpu,r->original_wall,r->original_cpu,r->walk_wall,r->walk_cpu,r->compile_wall,r->compile_cpu,r->emit_wall,r->orig_parts[0],r->orig_parts[1],r->orig_parts[2]};for(unsigned k=0;k<sizeof v/sizeof *v;k++)fprintf(f,",%.9f",v[k]*1000);fprintf(f,",%lu",r->marks);for(int i=0;i<S1_PARTS;i++)fprintf(f,",%.9f",r->parts[i]*1000);fputc('\n',f);}CHECK3(!fclose(f));}
