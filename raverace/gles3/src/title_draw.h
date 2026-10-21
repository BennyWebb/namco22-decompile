#ifndef RR_TITLE_DRAW_H
#define RR_TITLE_DRAW_H
#include "title.h"
bool title_draw_open(char *error, size_t size);
bool title_draw(SDL_Window *window, const tm_state *s, char *error, size_t size);
void title_draw_close(void);
#endif
