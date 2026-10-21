#include "title.h"
#include "title_draw.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
bool rr_title_open_pad(SDL_Joystick **pad, char *e, size_t n)
{
    if (*pad) {
        SDL_JoystickClose(*pad);
        *pad = NULL;
    }
    if (SDL_NumJoysticks() != 1) {
        snprintf(e, n, "CONTROLLER_COUNT");
        return false;
    }
    *pad = SDL_JoystickOpen(0);
    if (!*pad) {
        snprintf(e, n, "CONTROLLER_OPEN");
        return false;
    }
    char guid[33];
    SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(*pad), guid, sizeof guid);
    const char *name = SDL_JoystickName(*pad);
    if (strcmp(guid, TITLE_GUID) || !name || strcmp(name, TITLE_DEVICE) || SDL_JoystickNumAxes(*pad) < 4 ||
        SDL_JoystickNumButtons(*pad) < 16 || SDL_JoystickNumHats(*pad) != 1) {
        snprintf(e, n, "DEVICE_IDENTITY");
        return false;
    }
    return true;
}
bool rr_title_sample(SDL_Joystick *pad, ti_sample *s)
{
    *s = (ti_sample){0};
    if (!pad || !SDL_JoystickGetAttached(pad))
        return false;
    SDL_JoystickUpdate();
    for (unsigned i = 0; i < 16; i++)
        if (SDL_JoystickGetButton(pad, i))
            s->buttons |= 1u << i;
    s->hat = SDL_JoystickGetHat(pad, 0);
    for (unsigned i = 0; i < 4; i++)
        s->axes[i] = SDL_JoystickGetAxis(pad, i);
    return true;
}
static bool keyboard_released(void)
{
    int n;
    const Uint8 *keys = SDL_GetKeyboardState(&n);
    for (int i = 0; i < n; i++)
        if (keys[i])
            return false;
    return true;
}
static bool write_controls(tm_state *s, SDL_Joystick **pad, const char *path, bool reset)
{
    char error[128];
    ti_profile p = reset ? s->defaults : s->controls;
    if (!ti_write(path, &p, error, sizeof error)) {
        char temp[4096];
        snprintf(temp, sizeof temp, "%s.controller-new", path);
        tm_error(s, error, "write/read controls", errno == EEXIST ? temp : path);
        return false;
    }
    if (!rr_title_open_pad(pad, error, sizeof error)) {
        tm_error(s, error, "reopen controller", path);
        return false;
    }
    s->controls = s->saved_controls = p;
    s->controls_dirty = false;
    s->blocked = true;
    s->page = TM_CONTROLS;
    s->focus = 0;
    strcpy(s->status, "Controls saved - release all inputs");
    return true;
}
int rr_title_run(SDL_Window *window,
                 SDL_Joystick **pad,
                 const char *save,
                 const char *override,
                 const char *defaults,
                 ti_profile *profile,
                 const uint8_t *rom,
                 size_t size,
                 uint8_t image[8192])
{
    tm_state *s = calloc(1, sizeof *s);
    if (!s) {
        fprintf(stderr, "[TITLE-ERROR] ALLOCATION\n");
        return 2;
    }
    rr_operator_file file;
    rr_operator_error oe;
    ti_profile original, packaged;
    char error[128];
    bool draw_ready = title_draw_open(error, sizeof error);
    if (!draw_ready) {
        fprintf(stderr, "[TITLE-ERROR] %s initialize GLES3\n", error);
        free(s);
        return 2;
    }
    if (!rr_title_open_pad(pad, error, sizeof error))
        tm_error(s, error, "open controller", TITLE_DEVICE);
    else if (!ti_load(defaults, &packaged, error, sizeof error))
        tm_error(s, error, "read packaged controls", defaults);
    else {
        struct stat saved;
        if (lstat(override, &saved) == 0) {
            if (!ti_load(override, &original, error, sizeof error))
                tm_error(s, error, "read saved controls", override);
        } else if (errno == ENOENT)
            original = packaged;
        else
            tm_error(s, "IO_STAT", "inspect saved controls", override);
        if (s->page != TM_ERROR) {
            if (!rr_operator_load(save, &file, &oe))
                tm_error(s, rr_operator_error_name(oe.code), "read EEPROM", save);
            else {
                tm_init(s, &file, &original, &packaged);
                if (!tm_names(s, rom, size))
                    tm_error(s, "CAR_NAMES", "read program ROM", "0x2DEC9 + 16*i");
            }
        }
    }
    fprintf(stderr, "[TITLE] opened simulation_ticks=0\n");
    SDL_SetWindowTitle(window, "Rave Racer - Title");
    ti_sample previous = {0};
    bool release = true;
    uint64_t repeat = 0;
    uint8_t old_hat = 0;
    int result = -1;
    while (result < 0) {
        SDL_Event event;
        tm_result action = TM_WAIT;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                action = TM_QUIT;
                break;
            }
            if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                SDL_Keycode k = event.key.keysym.sym;
                tm_key key;
                if (k == SDLK_UP)
                    key = TK_UP;
                else if (k == SDLK_DOWN)
                    key = TK_DOWN;
                else if (k == SDLK_LEFT)
                    key = TK_LEFT;
                else if (k == SDLK_RIGHT)
                    key = TK_RIGHT;
                else if (k == SDLK_RETURN)
                    key = TK_A;
                else if (k == SDLK_ESCAPE)
                    key = TK_B;
                else if (k == SDLK_SPACE)
                    key = TK_START;
                else
                    continue;
                action = tm_event(s, key);
            }
        }
        ti_sample sample;
        bool connected = rr_title_sample(*pad, &sample);
        if (!connected && s->page != TM_ERROR)
            tm_error(s, "CONTROLLER_DISCONNECTED", "poll controller", TITLE_DEVICE);
        if ((sample.buttons & ((1u << 9) | (1u << 10))) == ((1u << 9) | (1u << 10))) {
            result = 0;
            break;
        }
        if (release) {
            if (ti_released(&sample) && keyboard_released())
                release = false;
            previous = sample;
        } else if (s->page == TM_CAPTURE) {
            if (s->blocked) {
                if (ti_released(&sample)) {
                    s->blocked = false;
                }
                previous = sample;
            } else {
                uint16_t buttons = sample.buttons & ~previous.buttons;
                uint8_t hat = sample.hat & ~previous.hat;
                if (buttons & (1u << 10))
                    action = tm_event(s, TK_START);
                else {
                    bool captured = false;
                    const char *type = "button";
                    int id = -1, value = 1;
                    if (s->capture == TI_STEER) {
                        for (unsigned i = 0; i < 4; i++)
                            if (abs(sample.axes[i]) > 20000) {
                                type = "axis";
                                id = (int)i;
                                value = sample.axes[i] < 0 ? -1 : 1;
                                break;
                            }
                    } else {
                        if (buttons) {
                            for (unsigned i = 0; i < 16; i++)
                                if (buttons & (1u << i)) {
                                    id = (int)i;
                                    break;
                                }
                        } else if (hat) {
                            type = "hat";
                            id = 0;
                            value = hat;
                        }
                    }
                    if (id >= 0) {
                        captured = ti_bind(&s->controls, (ti_action)s->capture, type, id, value, error, sizeof error);
                        if (captured) {
                            s->controls_dirty = strcmp(s->controls.text, s->saved_controls.text) != 0;
                            s->page = TM_CONTROLS;
                            s->focus = s->capture;
                            s->status[0] = 0;
                            release = true;
                        } else
                            snprintf(s->status, sizeof s->status, "%.80s: input rejected", error);
                    }
                }
            }
        } else {
            uint16_t b = sample.buttons & ~previous.buttons;
            uint64_t now = SDL_GetTicks64();
            if (sample.hat != old_hat) {
                repeat = now + 300;
                old_hat = sample.hat;
            }
            bool direction = sample.hat && (sample.hat != previous.hat || now >= repeat);
            if (direction) {
                tm_key key = sample.hat & 1 ? TK_UP : sample.hat & 4 ? TK_DOWN : sample.hat & 8 ? TK_LEFT : TK_RIGHT;
                action = tm_event(s, key);
                if (now >= repeat)
                    repeat = now + 100;
            } else if (b & (1u << 3))
                action = tm_event(s, TK_A);
            else if (b & (1u << 4))
                action = tm_event(s, TK_B);
            else if (b & (1u << 10))
                action = tm_event(s, TK_START);
        }
        previous = sample;
        if (action == TM_QUIT) {
            result = 0;
            break;
        }
        if (action == TM_FAILED) {
            result = 2;
            break;
        }
        if (action == TM_APPLY || action == TM_RESET) {
            write_controls(s, pad, override, action == TM_RESET);
            release = true;
        }
        if (action == TM_BOOT) {
            if (s->controls_dirty && !write_controls(s, pad, override, false))
                continue;
            if (!rr_operator_commit(save, &s->source, &s->draft, &oe)) {
                char temp[4096];
                snprintf(temp, sizeof temp, "%s.operator-new", save);
                tm_error(s, rr_operator_error_name(oe.code), "commit EEPROM", oe.system_error == EEXIST ? temp : save);
                continue;
            }
            if (!rr_operator_load(save, &file, &oe)) {
                tm_error(s, rr_operator_error_name(oe.code), "read committed EEPROM", save);
                continue;
            }
            if (memcmp(file.image.bytes, s->draft.bytes, 8192)) {
                tm_error(s, "STALE_SOURCE", "read committed EEPROM", save);
                continue;
            }
            fprintf(stderr, "[TITLE] committed simulation_ticks=0; release all inputs\n");
            bool ready = false;
            while (!ready) {
                while (SDL_PollEvent(&event))
                    if (event.type == SDL_QUIT) {
                        result = 0;
                        break;
                    }
                if (result == 0)
                    break;
                if (!rr_title_sample(*pad, &sample)) {
                    tm_error(s, "CONTROLLER_DISCONNECTED", "release before boot", TITLE_DEVICE);
                    break;
                }
                if ((sample.buttons & ((1u << 9) | (1u << 10))) == ((1u << 9) | (1u << 10))) {
                    result = 0;
                    break;
                }
                ready = ti_released(&sample) && keyboard_released();
                if (!ready)
                    SDL_Delay(16);
            }
            if (result == 0)
                break;
            if (!ready)
                continue;
            *profile = s->controls;
            memcpy(image, file.image.bytes, 8192);
            /* The caller installs this image and establishes the normal save baseline. */
            result = 1;
            break;
        }
        if (!title_draw(window, s, error, sizeof error)) {
            fprintf(stderr, "[TITLE-ERROR] %s draw\n", error);
            result = 2;
            break;
        }
        SDL_GL_SwapWindow(window);
        /* Leave time before the next vblank; a full-frame sleep after swap
         * would make navigation update only every second refresh. */
        if (SDL_WaitEventTimeout(&event, 4))
            SDL_PushEvent(&event);
    }
    title_draw_close();
    free(s);
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    return result;
}
