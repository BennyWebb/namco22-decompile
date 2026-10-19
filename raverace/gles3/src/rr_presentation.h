#ifndef RR_PRESENTATION_H
#define RR_PRESENTATION_H
#include "rr_mem.h"
#include "geo_hw.h"
#ifdef RR_PARALLEL_FRAME_PREP
#include "rr_video_layers.h"
#endif
#define RR_PRESENT_DIRECT_MAX 4096
#define RR_PRESENT_POINT_WORDS 0x20000
/* No pointers into simulation state. Capacity is fixed at startup. */
typedef struct {
    uint32_t frame;
    bool walk, point_valid;
    unsigned direct_count;
    double produced;
    uint32_t poly[RR_POLY_WORDS], point[RR_PRESENT_POINT_WORDS];
    uint8_t pal[RR_PAL_SIZE], mixer[RR_MIXER_SIZE], czram[RR_CZRAM_SIZE];
    uint8_t cgram[RR_CGRAM_SIZE], text[RR_TEXT_SIZE], tilemapattr[0x10];
    uint16_t generation[128], direct[RR_PRESENT_DIRECT_MAX][0x1c];
#ifdef RR_PARALLEL_FRAME_PREP
    double frame_sim_ms, frame_block_ms, frame_audio_ratio;
    unsigned frame_underruns, frame_depth;
    uint16_t state_main, state_sub, state_screen, state_course;
    unsigned state_reason;
    uint64_t palette_revision[128];
    unsigned dynamic_cache_major;
    void *dynamic_cache_dynamic;
    void *gles3_payload;
    unsigned gles3_tag;
    double gles3_walk_ms, gles3_compile_ms;
#ifdef GLES3_VALIDATION
    unsigned record_splits[4], record_protected_visible;
#endif
    rr_video_cpu_frame video;
    double scene_wall, prep_cpu, hud_cpu, ready_at, prep_started_at, prepared_at;
#ifdef RR_PARALLEL_PREP_PROFILE
    double direct_cpu, list_cpu, sort_cpu;
#endif
#endif
} rr_presentation;
#ifdef RR_THREADED_PRESENTATION
bool rr_presentation_enabled(void);
void rr_presentation_overlay(unsigned *frame, unsigned *drops, unsigned *queued, double *age_ms, double now);
void rr_presentation_configure(bool enabled, bool serial, const char *csv, bool profile_sync);
bool rr_presentation_start(void *window, void *context, unsigned every, unsigned capture, unsigned hz, unsigned from);
bool rr_presentation_video(uint32_t frame, bool slave);
bool rr_presentation_finish(void);
void rr_gl_presentation(const rr_presentation *snapshot);
void rr_gl_presentation_release(void);
bool rr_gles3_pump(void);
bool rr_gles3_present(uint32_t frame, const geo_quad *quads, int count, const float bg[3], double scene);
#ifdef RR_PARALLEL_FRAME_PREP
bool rr_gl_presentation_into(rr_presentation *snapshot);
void rr_gl_presentation_activate(const rr_presentation *snapshot);
bool rr_gles3_present_prepared(const rr_presentation *snapshot);
#endif
bool rr_dsp_copy_point(uint32_t *out);
#endif
#endif
