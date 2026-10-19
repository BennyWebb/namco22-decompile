#ifndef ENG_RENDER_STATS_H
#define ENG_RENDER_STATS_H
#include <stddef.h>
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
} eng_render_stats;
#endif
