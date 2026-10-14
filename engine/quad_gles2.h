/* GLES2 submission of the engine's existing screen-space geometry. */
#ifndef QUAD_GLES2_H
#define QUAD_GLES2_H
#include <stdbool.h>
#include <stddef.h>
#include "geo_hw.h"
typedef struct {
    unsigned triangles, draws, vertex_uploads, texture_barriers;
    unsigned cache_hits, cache_misses, cache_evictions, texture_uploads;
    size_t texture_bytes;
    unsigned long long vertex_upload_bytes;
    unsigned flush_texture, flush_scissor, flush_capacity, flush_mutation, flush_end;
    unsigned same_texture, binds, bind_skips;
    unsigned empty_texture_flush;
    double texture_seconds, upload_seconds, draw_seconds, decode_seconds;
    double decoded_texels, eviction_seconds;
    unsigned long long clock_probes, delete_calls;
    double vertex_upload_seconds, draw_call_seconds;
} eng_gles2_stats;
bool eng_gles2_world_atlas_mode(const char *name);
bool eng_gles2_multitexture_mode(const char *name);
bool eng_gles2_multi_gpu_trace(const char *path);
void eng_gles2_present_tick(unsigned tick);
bool eng_gles2_init(void);
void eng_gles2_priority(bool enabled);
bool eng_gles2_vbo_mode(const char *name);
const char *eng_gles2_vbo_mode_name(void);
void eng_gles2_diagnostic(bool enabled);
bool eng_gles2_mode(const char *name);
const char *eng_gles2_mode_name(void);
bool eng_gles2_texture_budget(unsigned mib);
bool eng_gles2_draw(const geo_quad *quads, int count, const float background[3],
                   eng_gles2_stats *stats);
#ifdef RR_PARALLEL_FRAME_PREP
bool eng_gles2_draw_ordered(const geo_quad *quads, const int *order, int count,
                            const float background[3], eng_gles2_stats *stats);
#endif
void eng_gles2_close(void);
#endif
