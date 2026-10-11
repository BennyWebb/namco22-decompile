/* Phase 1 submission of the engine's existing screen-space geometry. */
#ifndef QUAD_GLES2_H
#define QUAD_GLES2_H
#include <stdbool.h>
#include "geo_hw.h"
typedef struct { unsigned triangles, draws; } eng_gles2_stats;
bool eng_gles2_init(void);
bool eng_gles2_draw(const geo_quad *quads, int count, const float background[3],
                   eng_gles2_stats *stats);
void eng_gles2_close(void);
#endif
