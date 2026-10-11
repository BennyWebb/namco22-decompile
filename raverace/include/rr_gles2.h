#ifndef RR_GLES2_H
#define RR_GLES2_H
#include <stdbool.h>
#include <stdint.h>
#include "geo_hw.h"
const geo_quad *rr_gles2_quads(int *count);
void rr_gles2_background(float rgb[3]);
bool rr_gles2_options(int *argc, char **argv);
bool rr_gles2_open(void);
void rr_gles2_prepare_begin(void);
bool rr_gles2_frame(uint32_t frame, const geo_quad *quads, int count, const float bg[3]);
bool rr_gles2_failed(void);
void rr_gles2_close(void);
#endif
