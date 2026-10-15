#ifndef RR_NATIVE_LIVE_API_H
#define RR_NATIVE_LIVE_API_H
#include <stdbool.h>
#include <stdint.h>
#include "quad_gles2.h"
bool rr_native_open(void);
bool rr_native_active(void);
void rr_native_set_frame(uint32_t frame);
void rr_native_prepare(bool slave, bool geometry);
bool rr_native_draw(eng_gles2_stats *stats);
void rr_native_close(void);
#endif
