/* Wall-clock pacing only. The cooperative scheduler still executes every tick.
 * Namco S22: pixel clock 25.6 MHz, totals 814 x 525 (MAME namcos22.cpp). */
#ifndef RR_GLES2_TIMING_H
#define RR_GLES2_TIMING_H
#include <stdbool.h>
#define RR_FRAME_SECONDS (814.0 * 525.0 / 25600000.0)
#define RR_MAX_CATCHUP 4u

typedef struct {
    bool started;
    double deadline, next_render, lag, max_lag, discarded;
    unsigned catchup, resyncs;
} rr_frame_clock;
static inline void rr_clock_begin(rr_frame_clock *c, double now)
{
    if (!c->started) { c->started=true; c->deadline=now; c->next_render=now; }
    else c->deadline += RR_FRAME_SECONDS;
}
/* Called after waiting to deadline. No emulated steps are ever discarded.
 * Four consecutive overdue ticks may omit pictures. Sustained overload then
 * discards wall-clock debt and presents, making slow execution visible in logs. */
static inline bool rr_clock_render(rr_frame_clock *c, double now, unsigned render_hz, bool force)
{
    c->lag=now>c->deadline ? now-c->deadline : 0;
    if (c->lag>c->max_lag) c->max_lag=c->lag;
    if (c->lag >= RR_FRAME_SECONDS && c->catchup >= RR_MAX_CATCHUP) {
        c->discarded += c->lag; c->resyncs++; c->deadline=now; c->lag=0;
        c->catchup=0; force=true;
    }
    if (!force && c->lag >= RR_FRAME_SECONDS) { c->catchup++; return false; }
    c->catchup=0;
    if (!force && render_hz && now<c->next_render) return false;
    if (render_hz) {
        c->next_render += 1.0/render_hz;
        if (c->next_render<now) c->next_render=now+1.0/render_hz;
    }
    return true;
}
#endif
