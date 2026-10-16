#ifndef ENG_P1_API_H
#define ENG_P1_API_H
#include "rr_presentation.h"
#include "quad_gles2.h"
double rr_p1_audio_ratio(void);
bool rr_p1_open(void);
bool rr_p1_legacy_ensure(void);
bool rr_p1_legacy_pending(void);
bool rr_p1_prepare(rr_presentation *s);
bool rr_p1_draw(const rr_presentation *s,eng_gles2_stats *stats);
void rr_p1_close(void);
void rr_p1b_warm(const rr_presentation *s);
void rr_gl_p1_select(const rr_presentation *s);
#endif
