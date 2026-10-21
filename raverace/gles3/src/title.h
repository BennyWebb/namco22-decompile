#ifndef RR_TITLE_H
#define RR_TITLE_H
#include "title_input.h"
#include "operator_settings.h"
#include <SDL.h>
typedef enum {
    TM_ROOT,
    TM_GAME,
    TM_CAR,
    TM_CONTROLS,
    TM_START_CONFIRM,
    TM_QUIT_CONFIRM,
    TM_RESET_CONFIRM,
    TM_CAPTURE,
    TM_ERROR
} tm_page;
typedef enum {
    TK_UP,
    TK_DOWN,
    TK_LEFT,
    TK_RIGHT,
    TK_A,
    TK_B,
    TK_START,
    TK_EXIT
} tm_key;
typedef enum {
    TM_WAIT,
    TM_BOOT,
    TM_QUIT,
    TM_FAILED,
    TM_APPLY,
    TM_RESET
} tm_result;
typedef struct {
    tm_page page;
    int focus, capture;
    bool blocked, controls_dirty, extra;
    rr_operator_file source;
    rr_operator_image draft;
    ti_profile controls, saved_controls, defaults;
    char car[8][32], error[256], status[128];
} tm_state;
void tm_init(tm_state *s, const rr_operator_file *file, const ti_profile *profile, const ti_profile *defaults);
bool tm_names(tm_state *s, const uint8_t *rom, size_t size);
void tm_error(tm_state *s, const char *code, const char *operation, const char *path);
bool tm_dirty(const tm_state *s);
int tm_rows(const tm_state *s);
tm_result tm_event(tm_state *s, tm_key key);
void tm_label(const tm_state *s, int row, char *text, size_t size);
/* Product owns the GL context here; no simulation/presentation worker exists. */
int rr_title_run(SDL_Window *window,
                 SDL_Joystick **joystick,
                 const char *save_path,
                 const char *override_path,
                 const char *default_path,
                 ti_profile *profile,
                 const uint8_t *rom,
                 size_t size,
                 uint8_t image[8192]);
bool rr_title_open_pad(SDL_Joystick **pad, char *error, size_t size);
bool rr_title_sample(SDL_Joystick *pad, ti_sample *sample);
#endif
