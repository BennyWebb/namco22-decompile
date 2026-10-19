#ifndef RR_INPUT_STREAM_H
#define RR_INPUT_STREAM_H
#include <stdbool.h>
#include <stdint.h>
bool rr_input_replay_open(const char *path);
bool rr_input_replaying(void);
bool rr_input_record_start(const char *path);
void rr_input_record_stop(void);
bool rr_input_recording(void);
void rr_input_frame(uint32_t frame);           /* apply replay / log recording */

void rr_input_record_marker(void); /* host-only annotation, committed at the next input boundary */
bool rr_input_stream_ok(void);
void rr_input_stream_close(void);
#endif
