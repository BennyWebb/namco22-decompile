/* Timestamp statistics test, without opening SDL/GLES or sampling game state. */
#define clock_gettime rr_test_clock
#include "../src/rr_overlay.c"
#include <assert.h>
static double fake_time;
int rr_test_clock(clockid_t id,struct timespec *t)
{ (void)id;t->tv_sec=(time_t)fake_time;t->tv_nsec=(long)((fake_time-t->tv_sec)*1e9);return 0; }
void rr_audio_output_status(rr_audio_output_stats *out){memset(out,0,sizeof *out);}
int main(void)
{
 assert(!rr_overlay_mode("invalid"));assert(rr_overlay_mode("compact"));
 for(unsigned i=1;i<600;i++){fake_time=i/59.904059904;rr_overlay_present(i,fake_time);}
 assert(count==301);assert(fps>59.903&&fps<59.905);assert(milliseconds>16.69&&milliseconds<16.70);
 assert(low>59.903&&low<59.905);
 rr_overlay_toggle();assert(atomic_load(&mode)==2);
 rr_overlay_toggle();assert(atomic_load(&mode)==0);rr_overlay_present(600,fake_time);assert(count==0&&seen==0);
 puts("Rolling swap timestamps, 300 intervals, 1% low, and mode reset passed");
}
