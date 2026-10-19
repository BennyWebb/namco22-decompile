#ifndef GLES3_TIMING_H
#define GLES3_TIMING_H
#include <stdbool.h>
typedef struct {
    bool started;
    double deadline, next_render, lag, max_lag, discarded;
    unsigned catchup, resyncs;
} rr_frame_clock;
#define RR_FRAME_SECONDS (814.0 * 525.0 / 25600000.0)
#endif
