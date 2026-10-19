#ifndef RR_OVERLAY_H
#define RR_OVERLAY_H
#include <stdbool.h>
#include <stdint.h>
bool rr_overlay_mode(const char *mode);
void rr_overlay_toggle(void); /* Input owner only; atomically published. */
void rr_overlay_draw(void);   /* GL owner only. */
void rr_overlay_present(uint32_t frame, double timestamp); /* Immediately after successful swap. */
void rr_overlay_close(void);
#endif
