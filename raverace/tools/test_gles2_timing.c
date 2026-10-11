#include <assert.h>
#include <stdio.h>
#include "rr_gles2_timing.h"
int main(void)
{
    rr_frame_clock c={0}; rr_clock_begin(&c,10); assert(c.deadline==10);
    assert(rr_clock_render(&c,10,30,false));
    rr_clock_begin(&c,10); assert(!rr_clock_render(&c,c.deadline,30,false));
    rr_clock_begin(&c,10); assert(rr_clock_render(&c,c.deadline+0.00001,30,false));
    for(unsigned i=0;i<RR_MAX_CATCHUP;i++) {
        rr_clock_begin(&c,10); assert(!rr_clock_render(&c,c.deadline+1,0,false));
    }
    assert(c.catchup==RR_MAX_CATCHUP);
    rr_clock_begin(&c,10); double now=c.deadline+1;
    assert(rr_clock_render(&c,now,0,false));assert(c.deadline==now && c.resyncs==1 && c.discarded>0.99);
    rr_clock_begin(&c,10); assert(rr_clock_render(&c,c.deadline+0.1,1,true)); /* exact capture */
    puts("Exact period, render cap, bounded catch-up, overload resync and forced capture passed");
}
