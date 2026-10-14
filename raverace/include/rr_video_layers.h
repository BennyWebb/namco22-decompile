#ifndef RR_VIDEO_LAYERS_H
#define RR_VIDEO_LAYERS_H
#include <stdbool.h>
#include <stdint.h>
typedef struct {
 uint8_t *text, *shadow; /* fixed per-slot storage; never renderer-owned */
 uint8_t lut[256][4];
 uint64_t generation;
 bool any_text, any_shadow;
#ifdef RR_GPU_HUD
 const uint8_t *characters, *tilemap; /* immutable selected snapshot, or serial state */
 uint8_t palette[256][4], multipliers[3][3];
 int scroll_x, scroll_y;
 bool shadow_enabled;
#endif
} rr_video_cpu_frame;
bool rr_video_layers_gpu(void);
bool rr_video_layers_verify(void);
#ifdef RR_GPU_HUD
bool rr_video_layers_compose_gpu(const rr_video_cpu_frame *frame);
#endif
void rr_video_layers_enable(bool enabled);
void rr_video_layers_profile(bool on);
void rr_video_layers_profile_gate(bool on);
bool rr_video_layers_profile_enabled(void);
double rr_video_layers_cpu_time(void);
void rr_video_layers_decode_time(double elapsed);
bool rr_video_layers_enabled(void);
bool rr_video_layers_open(void);
void rr_video_layers_begin(void);
bool rr_video_layers_compose(const uint8_t *text, const uint8_t *shadow, bool dirty,
                             bool any_text, bool any_shadow, const uint8_t lut[256][4]);
void rr_video_layers_close(void);
bool rr_gl_video_compose(void);
#ifdef RR_PARALLEL_FRAME_PREP
bool rr_gl_video_prepare(rr_video_cpu_frame *out);
bool rr_gl_video_submit(const rr_video_cpu_frame *frame);
#endif
#endif
