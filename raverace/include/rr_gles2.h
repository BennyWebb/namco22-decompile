#ifndef RR_GLES2_H
#define RR_GLES2_H
#include <stdbool.h>
#include <stdint.h>
#include "geo_hw.h"
const geo_quad *rr_gles2_quads(int *count);
void rr_gles2_background(float rgb[3]);
bool rr_gles2_options(int *argc, char **argv);
bool rr_gles2_open(void);
bool rr_gles2_interactive(void);
uint32_t rr_gles2_profile_from(void);
void rr_gles2_apply_input(void);
typedef enum {
    RR_STAGE_DSP, RR_STAGE_SOUND, RR_STAGE_AUDIO, RR_STAGE_SERIAL, RR_STAGE_IO,
    RR_STAGE_VIDEO, RR_STAGE_PALETTE, RR_STAGE_DIRECT, RR_STAGE_LIST, RR_STAGE_SORT, RR_STAGE_COUNT
} rr_gles2_stage;
double rr_gles2_stage_begin(void);
void rr_gles2_stage_end(rr_gles2_stage stage, double start);
bool rr_gles2_prepare_begin(uint32_t frame, bool *geometry);
void rr_gles2_scene_tick(bool walk, unsigned direct);
bool rr_gles2_frame(uint32_t frame, const geo_quad *quads, int count, const float bg[3]);
bool rr_gles2_failed(void);
void rr_gles2_close(void);
#endif
