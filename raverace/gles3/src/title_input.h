#ifndef RR_TITLE_INPUT_H
#define RR_TITLE_INPUT_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define TITLE_CONFIG_SIZE 8192
#define TITLE_ROWS 32
#define TITLE_GUID "19000000010000000100000000010000"
#define TITLE_DEVICE "Anbernic RG40XX-H Controller"
#define TITLE_RETURN_CODE 42
/* ES names describe raw inputs, not SDL's swapped face-button labels. */
typedef enum {
    TI_GAS,
    TI_GAS2,
    TI_BRAKE,
    TI_BRAKE2,
    TI_UP,
    TI_DOWN,
    TI_VIEW,
    TI_OVERLAY,
    TI_STEER,
    TI_ACTIONS
} ti_action;
typedef struct {
    char name[32], type[12];
    int id, value;
    size_t begin, end;
} ti_row;
typedef struct {
    char text[TITLE_CONFIG_SIZE], guid[33], name[128];
    ti_row rows[TITLE_ROWS];
    unsigned count;
    int action[TI_ACTIONS];
} ti_profile;
typedef struct {
    uint16_t buttons;
    uint8_t hat;
    int16_t axes[4];
} ti_sample;
typedef struct {
    bool gas, brake, up, down, view, overlay, coin, start, exit, x;
    int wheel;
} ti_controls;
typedef struct {
    bool held, fired;
    uint64_t since;
} ti_hold;
extern const char *const ti_labels[TI_ACTIONS];
bool ti_parse(const char *text, ti_profile *out, char *error, size_t capacity);
bool ti_load(const char *path, ti_profile *out, char *error, size_t capacity);
bool ti_bind(ti_profile *profile, ti_action action, const char *type, int id, int value, char *error, size_t capacity);
bool ti_write(const char *path, const ti_profile *profile, char *error, size_t capacity);
void ti_poll(const ti_profile *profile, const ti_sample *sample, ti_controls *out);
bool ti_hold_update(ti_hold *hold, bool x, bool in_game, uint64_t now_ms);
bool ti_released(const ti_sample *sample);
#endif
