#ifndef RR_GLES3_H
#define RR_GLES3_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "geo_hw.h"
/* Backing records; GLES2 consumes the last sort's eng_quad_index mapping.
 * Storage remains in emission order under RR_GEOMETRY_INDEX_ORDER. */
const geo_quad *rr_gles3_quads(int *count);
void rr_gles3_background(float rgb[3]);
bool rr_gles3_options(int *argc, char **argv);
bool rr_gles3_open(void);
bool rr_gles3_interactive(void);
bool rr_gles3_record_exit_pending(void);
uint32_t rr_gles3_profile_from(void);
void rr_gles3_apply_input(void);
typedef enum {
    RR_STAGE_DSP, RR_STAGE_SOUND, RR_STAGE_AUDIO, RR_STAGE_SERIAL, RR_STAGE_IO,
    RR_STAGE_VIDEO, RR_STAGE_PALETTE, RR_STAGE_DIRECT, RR_STAGE_LIST, RR_STAGE_SORT, RR_STAGE_COUNT
} rr_gles3_stage;
double rr_gles3_stage_begin(void);
void rr_gles3_stage_end(rr_gles3_stage stage, double start);
bool rr_gles3_prepare_begin(uint32_t frame, bool *geometry);
void rr_gles3_scene_tick(bool walk, unsigned direct);
bool rr_gles3_frame(uint32_t frame, const geo_quad *quads, int count, const float bg[3]);
bool rr_gles3_failed(void);
double rr_gles3_last_presentation(void);
/* Presentation-owner only; copied after swap for optional buffered CSV. */
typedef struct {double draw_wall,submission_wall,swap_wall;unsigned draws,triangles,binds,uploads,misses,evictions;size_t texture_bytes;double lookup_wall,decode_wall,upload_wall,decoded_texels,eviction_wall;unsigned long long clock_probes,delete_calls;} rr_presentation_metrics;
rr_presentation_metrics rr_gles3_last_metrics(void);
void rr_gles3_close(void);
#endif
