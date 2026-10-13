#ifndef RR_PRESENTATION_H
#define RR_PRESENTATION_H
#include "rr_mem.h"
#include "geo_hw.h"
#define RR_PRESENT_DIRECT_MAX 4096
#define RR_PRESENT_POINT_WORDS 0x20000
/* No pointers into simulation state. Capacity is fixed at startup. */
typedef struct {
    uint32_t frame; bool walk, point_valid; unsigned direct_count;
    double produced;
    uint32_t poly[RR_POLY_WORDS], point[RR_PRESENT_POINT_WORDS];
    uint8_t pal[RR_PAL_SIZE], mixer[RR_MIXER_SIZE], czram[RR_CZRAM_SIZE];
    uint8_t cgram[RR_CGRAM_SIZE],text[RR_TEXT_SIZE],tilemapattr[0x10];
    uint16_t generation[128], direct[RR_PRESENT_DIRECT_MAX][0x1c];
} rr_presentation;
#ifdef RR_THREADED_PRESENTATION
bool rr_presentation_enabled(void);
void rr_presentation_overlay(unsigned *frame,unsigned *drops,unsigned *queued,double *age_ms,double now);
void rr_presentation_configure(bool enabled, bool serial, const char *csv, bool profile_sync);
bool rr_presentation_start(void *window, void *context, unsigned every, unsigned capture, unsigned hz, unsigned from);
bool rr_presentation_video(uint32_t frame, bool slave);
bool rr_presentation_finish(void);
void rr_gl_presentation(const rr_presentation *snapshot);
void rr_gl_presentation_release(void);
bool rr_gles2_pump(void);
bool rr_gles2_present(uint32_t frame, const geo_quad *quads, int count, const float bg[3], double scene);
bool rr_dsp_copy_point(uint32_t *out);
#endif
#endif
