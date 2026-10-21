#include "title.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>
void tm_init(tm_state *s, const rr_operator_file *f, const ti_profile *p, const ti_profile *defaults)
{
    memset(s, 0, sizeof *s);
    s->source = *f;
    s->draft = f->image;
    s->controls = s->saved_controls = *p;
    s->defaults = *defaults;
    s->extra = f->image.values.extra_mode == 0;
}
bool tm_names(tm_state *s, const uint8_t *rom, size_t size)
{
    if (size < 0x2dec9 + 16 * 8)
        return false;
    char base[8][24];
    for (unsigned i = 0; i < 8; i++) {
        const uint8_t *p = rom + 0x2dec9 + 16 * i;
        /* This table has a short print prefix; take its alphabetic colour word. */
        unsigned at = 0, j = 0;
        while (at < 16 && !isalpha(p[at]))
            at++;
        while (at < 16 && isalpha(p[at]) && j < sizeof base[i] - 1) {
            base[i][j] = (char)(j ? tolower(p[at]) : toupper(p[at]));
            j++;
            at++;
        }
        base[i][j] = 0;
        if (!j)
            return false;
    }
    for (unsigned i = 0; i < 8; i++) {
        unsigned ordinal = 0, total = 0;
        for (unsigned j = 0; j < 8; j++)
            if (!strcmp(base[i], base[j])) {
                total++;
                if (j <= i)
                    ordinal++;
            }
        if (total > 1)
            snprintf(s->car[i], sizeof s->car[i], "%.23s %u", base[i], ordinal);
        else
            snprintf(s->car[i], sizeof s->car[i], "%.23s", base[i]);
    }
    return true;
}
void tm_error(tm_state *s, const char *code, const char *op, const char *path)
{
    snprintf(s->error, sizeof s->error, "%s: %s\n%s", code, op, path);
    s->page = TM_ERROR;
    s->focus = 0;
    fprintf(stderr, "[TITLE-ERROR] %s operation=%s file=%s\n", code, op, path);
}
bool tm_dirty(const tm_state *s)
{
    return memcmp(s->draft.bytes, s->source.image.bytes, 8192) != 0 || s->controls_dirty;
}
int tm_rows(const tm_state *s)
{
    switch (s->page) {
    case TM_ROOT:
        return 5;
    case TM_GAME:
        return 6;
    case TM_CAR:
        return 9;
    case TM_CONTROLS:
        return TI_ACTIONS + 3;
    case TM_START_CONFIRM:
        return 3;
    case TM_QUIT_CONFIRM:
    case TM_RESET_CONFIRM:
        return 2;
    default:
        return 1;
    }
}
static void page(tm_state *s, tm_page p)
{
    s->page = p;
    s->focus = 0;
    s->status[0] = 0;
}
static void edit(tm_state *s, int direction)
{
    if (s->extra) {
        snprintf(s->status, sizeof s->status, "EXTRA_MODE: game options are locked");
        return;
    }
    rr_operator_field f = RR_OPERATOR_CAR;
    int value = 0, low = 0, high = 1;
    if (s->page == TM_CAR) {
        value = s->focus;
        high = 7;
    } else {
        const rr_operator_values *v = &s->draft.values;
        switch (s->focus) {
        case 0:
            f = RR_OPERATOR_SOLO_DIFFICULTY;
            value = v->difficulty[1];
            low = 1;
            high = 8;
            break;
        case 1:
            f = RR_OPERATOR_TIME_TRIAL_DIFFICULTY;
            value = v->difficulty[2];
            low = 1;
            high = 8;
            break;
        case 2:
            f = RR_OPERATOR_SPEED_UNITS;
            value = v->speed_units;
            break;
        case 3:
            f = RR_OPERATOR_ATTRACT_SOUND;
            value = v->attract_sound;
            break;
        case 4:
            f = RR_OPERATOR_FREE_PLAY;
            value = v->free_play;
            break;
        default:
            return;
        }
        value += direction;
        if (value > high)
            value = low;
        if (value < low)
            value = high;
    }
    rr_operator_error e;
    if (!rr_operator_edit(&s->draft, f, value, &s->draft, &e))
        tm_error(s, rr_operator_error_name(e.code), "edit EEPROM", "saves/rr_eeprom.nv");
}
tm_result tm_event(tm_state *s, tm_key key)
{
    if (key == TK_EXIT)
        return TM_QUIT;
    if (s->page == TM_ERROR)
        return key == TK_A || key == TK_B || key == TK_START ? TM_FAILED : TM_WAIT;
    if (s->page == TM_CAPTURE) {
        if (key == TK_START) {
            page(s, TM_CONTROLS);
            s->focus = s->capture;
        }
        return TM_WAIT;
    }
    if (key == TK_UP || key == TK_DOWN) {
        s->focus = (s->focus + (key == TK_UP ? tm_rows(s) - 1 : 1)) % tm_rows(s);
        return TM_WAIT;
    }
    if (key == TK_B) {
        if (s->page == TM_ROOT) {
            if (tm_dirty(s))
                page(s, TM_QUIT_CONFIRM);
            else
                return TM_QUIT;
        } else if (s->page == TM_RESET_CONFIRM)
            page(s, TM_CONTROLS);
        else
            page(s, TM_ROOT);
        return TM_WAIT;
    }
    if (key == TK_START && s->page != TM_ROOT) {
        page(s, TM_ROOT);
        return TM_WAIT;
    }
    if ((key == TK_LEFT || key == TK_RIGHT) && s->page == TM_GAME) {
        edit(s, key == TK_LEFT ? -1 : 1);
        return TM_WAIT;
    }
    if (key != TK_A && key != TK_START)
        return TM_WAIT;
    switch (s->page) {
    case TM_ROOT:
        if (key == TK_START || s->focus == 0) {
            if (tm_dirty(s))
                page(s, TM_START_CONFIRM);
            else
                return TM_BOOT;
        } else if (s->focus == 1) {
            page(s, TM_GAME);
            if (s->extra)
                strcpy(s->status, "EXTRA_MODE: game options are locked");
        } else if (s->focus == 2) {
            page(s, TM_CAR);
            s->focus = s->draft.values.car;
            if (s->extra)
                strcpy(s->status, "EXTRA_MODE: car appearance is locked");
        } else if (s->focus == 3)
            page(s, TM_CONTROLS);
        else {
            if (tm_dirty(s))
                page(s, TM_QUIT_CONFIRM);
            else
                return TM_QUIT;
        }
        break;
    case TM_GAME:
        if (s->focus == 5)
            page(s, TM_ROOT);
        else
            edit(s, 1);
        break;
    case TM_CAR:
        if (s->focus == 8)
            page(s, TM_ROOT);
        else
            edit(s, 0);
        break;
    case TM_CONTROLS:
        if (s->focus < TI_ACTIONS) {
            s->capture = s->focus;
            s->blocked = true;
            s->page = TM_CAPTURE;
        } else if (s->focus == TI_ACTIONS)
            return TM_APPLY;
        else if (s->focus == TI_ACTIONS + 1)
            page(s, TM_RESET_CONFIRM);
        else
            page(s, TM_ROOT);
        break;
    case TM_START_CONFIRM:
        if (s->focus == 0)
            return TM_BOOT;
        if (s->focus == 1) {
            s->draft = s->source.image;
            s->controls = s->saved_controls;
            s->controls_dirty = false;
            return TM_BOOT;
        }
        page(s, TM_ROOT);
        break;
    case TM_QUIT_CONFIRM:
        if (s->focus == 0)
            return TM_QUIT;
        page(s, TM_ROOT);
        break;
    case TM_RESET_CONFIRM:
        if (s->focus == 0)
            return TM_RESET;
        page(s, TM_CONTROLS);
        break;
    default:
        break;
    }
    return TM_WAIT;
}
void tm_label(const tm_state *s, int row, char *out, size_t size)
{
    const rr_operator_values *v = &s->draft.values;
    const char *root[] = {"START ARCADE", "Game", "Car appearance", "Controls", "Quit"};
    if (s->page == TM_ROOT)
        snprintf(out, size, "%s", root[row]);
    else if (s->page == TM_GAME) {
        const char *label[] = {
            "Solo difficulty", "Time-trial difficulty", "Speed units", "Attract sound", "Free play", "Back"};
        char value[32] = "";
        if (row < 2) {
            if (s->extra)
                strcpy(value, "Locked");
            else
                snprintf(value, sizeof value, "%c", 'A' + v->difficulty[row + 1] - 1);
        } else if (row == 2)
            strcpy(value, v->speed_units ? "mph" : "km/h");
        else if (row == 3)
            strcpy(value, v->attract_sound ? "Off" : "On");
        else if (row == 4)
            strcpy(value, v->free_play ? "On" : "Off");
        snprintf(out, size, "%s%s%s", label[row], row == 5 ? "" : ": ", value);
    } else if (s->page == TM_CAR)
        snprintf(out, size, "%s%s", row == 8 ? "Back" : s->car[row], row == v->car ? "  *" : "");
    else if (s->page == TM_CONTROLS) {
        if (row < TI_ACTIONS) {
            const ti_row *r = &s->controls.rows[s->controls.action[row]];
            snprintf(out,
                     size,
                     "%s: %s %d%s",
                     ti_labels[row],
                     r->type,
                     r->id,
                     !strcmp(r->type, "axis") ? (r->value < 0 ? " -" : " +") : "");
        } else
            snprintf(out,
                     size,
                     "%s",
                     row == TI_ACTIONS       ? "Apply controls"
                     : row == TI_ACTIONS + 1 ? "Reset to default controls"
                                             : "Back");
    } else if (s->page == TM_START_CONFIRM) {
        const char *a[] = {"Save and start", "Discard and start", "Back"};
        snprintf(out, size, "%s", a[row]);
    } else if (s->page == TM_QUIT_CONFIRM)
        snprintf(out, size, "%s", row ? "Back" : "Discard and quit");
    else if (s->page == TM_RESET_CONFIRM)
        snprintf(out, size, "%s", row ? "Back" : "Write default controls");
    else
        snprintf(out, size, "%s", s->page == TM_ERROR ? "Press A or B to exit" : "Release controls, then press input");
}
