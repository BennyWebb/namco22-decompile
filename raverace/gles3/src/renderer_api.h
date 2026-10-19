#ifndef ENG_GLES3_API_H
#define ENG_GLES3_API_H
#include "rr_presentation.h"
#include "render_stats.h"
double rr_audio_resampler_ratio(void);
bool rr_renderer_open(void);
bool rr_renderer_prepare(rr_presentation *s);
bool rr_renderer_draw(const rr_presentation *s, eng_render_stats *stats);
void rr_renderer_close(void);
void gles3_unsupported(unsigned, unsigned, unsigned, int, int, const int *, const char *) __attribute__((noreturn));
void gles3_pre_warm(void);
void rr_gl_renderer_select(const rr_presentation *s);
#endif
