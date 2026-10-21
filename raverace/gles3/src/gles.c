/* GLES3-only SDL/input host. No world-renderer backend or legacy switches. */
#include "renderer_api.h"
#include <SDL.h>
#include <GLES3/gl3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "rr_gles3.h"
#include "rr_sim_profile.h"
#include "rr_video_layers.h"
#include "rr_overlay.h"
#include "rr_sound.h"
#ifdef RR_TITLE_OPTIONS
#include "title.h"
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
static SDL_Joystick *title_pad;
static ti_profile title_profile;
static ti_hold return_hold;
static bool return_requested, overlay_held;
static int save_ack = -1;
#endif
static SDL_Window *window;
static SDL_GLContext context;
#include "rr_input_stream.h"
#include "rr_hw.h"
/* Host adapter only: unchanged EEPROM state is snapshotted on the simulation
 * thread; the low-priority supervisor persists it atomically in gles3 saves. */
static bool save_enabled, failed;
static unsigned save_ticks;
static unsigned char save_last_eeprom[RR_EEPROM_SIZE];
static void save_emit(void)
{
    bool force = false;
#ifdef RR_TITLE_OPTIONS
    force = return_requested;
#endif
    if (!save_enabled || (!force && !memcmp(save_last_eeprom, g_rr.eeprom, RR_EEPROM_SIZE)))
        return;
    static const char hex[] = "0123456789abcdef";
    char line[32 + RR_EEPROM_SIZE * 2];
    unsigned at = 0;
    const char *prefix = "[SAVE-EEPROM] ";
    while (*prefix)
        line[at++] = *prefix++;
    for (unsigned i = 0; i < RR_EEPROM_SIZE; i++) {
        line[at++] = hex[g_rr.eeprom[i] >> 4];
        line[at++] = hex[g_rr.eeprom[i] & 15];
    }
    line[at++] = '\n';
    if (fwrite(line, 1, at, stderr) != at) {
        failed = true;
        return;
    }
    memcpy(save_last_eeprom, g_rr.eeprom, RR_EEPROM_SIZE);
}
static void save_tick(void)
{
    if (save_enabled && ++save_ticks % 120 == 0)
        save_emit();
}
static void save_begin(bool enabled)
{
    save_enabled = enabled;
    save_ticks = 0;
    if (enabled) {
        memcpy(save_last_eeprom, g_rr.eeprom, RR_EEPROM_SIZE);
        fprintf(stderr, "[SAVE] baseline bytes=%u transport=async-supervisor\n", RR_EEPROM_SIZE);
    }
}

/* Small SDL input bridge. No desktop UI, audio or simulation timing changes. */
#include "rr_hw.h"
#include "rr_game.h"
static bool interactive, input_log;
static const char *controller_db, *es_input;
static bool input_coin, input_gas, input_brake, input_view, input_up, input_down;
static int input_wheel;
static unsigned coin_edges, start_ticks;
#ifdef RR_TITLE_OPTIONS
static bool chord_start[16], chord_select[16];
#endif
/* Event edges survive DOWN+UP in one queue drain (including a slow swap).
 * Only consume them when an emulated input sample is applied. */
static bool input_event(const SDL_Event *event)
{
    if (event->type == SDL_KEYDOWN && !event->key.repeat && event->key.keysym.scancode == SDL_SCANCODE_F3)
        rr_overlay_toggle();
    if (!interactive)
        return true;
    if (event->type == SDL_KEYDOWN && !event->key.repeat) {
        if (event->key.keysym.scancode == SDL_SCANCODE_5 && coin_edges < 8)
            coin_edges++;
        if (event->key.keysym.scancode == SDL_SCANCODE_RETURN)
            start_ticks = 10;
    }
#ifdef RR_TITLE_OPTIONS
    if ((event->type == SDL_JOYBUTTONDOWN || event->type == SDL_JOYBUTTONUP) && title_pad &&
        event->jbutton.which == SDL_JoystickInstanceID(title_pad)) {
        bool down = event->type == SDL_JOYBUTTONDOWN;
        if (event->jbutton.button == 10) {
            chord_start[0] = down;
            if (down)
                start_ticks = 10;
        }
        if (event->jbutton.button == 9) {
            chord_select[0] = down;
            if (down && coin_edges < 8)
                coin_edges++;
        }
        if (chord_start[0] && chord_select[0])
            return false;
    }
#endif
    return true;
}
static bool input_poll(void)
{
    const Uint8 *keys = SDL_GetKeyboardState(NULL);
    input_coin = keys[SDL_SCANCODE_5];
    input_gas = keys[SDL_SCANCODE_X] || keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_RETURN];
    input_brake = keys[SDL_SCANCODE_Z] || keys[SDL_SCANCODE_DOWN];
    input_view = keys[SDL_SCANCODE_C];
    input_up = keys[SDL_SCANCODE_S];
    input_down = keys[SDL_SCANCODE_A];
    int direction = keys[SDL_SCANCODE_RIGHT] - keys[SDL_SCANCODE_LEFT];
    input_wheel = 0;
#ifdef RR_TITLE_OPTIONS
    ti_sample sample;
    ti_controls controls;
    if (!rr_title_sample(title_pad, &sample)) {
        fprintf(stderr, "[TITLE-ERROR] CONTROLLER_DISCONNECTED operation=game input file=%s\n", TITLE_DEVICE);
        failed = true;
        return false;
    }
    ti_poll(&title_profile, &sample, &controls);
    if (controls.exit)
        return false;
    input_coin |= controls.coin;
    input_gas |= controls.gas || controls.start;
    input_brake |= controls.brake;
    input_up |= controls.up;
    input_down |= controls.down;
    input_view |= controls.view;
    input_wheel = controls.wheel;
    if (controls.overlay && !overlay_held)
        rr_overlay_toggle();
    overlay_held = controls.overlay;
    if (ti_hold_update(&return_hold, controls.x, true, SDL_GetTicks64())) {
        return_requested = true;
        return false;
    }
#endif
    if (direction)
        input_wheel = direction > 0 ? 32767 : -32767;
    return true;
}
static void input_bit(uint16_t bit, bool pressed)
{
    if (pressed)
        g_hw.inputs &= (uint16_t)~bit;
    else
        g_hw.inputs |= bit;
}
static void input_ramp(uint16_t *value, int target, int delta)
{
    int v = *value;
    if (v < target) {
        v += delta;
        if (v > target)
            v = target;
    } else if (v > target) {
        v -= delta;
        if (v < target)
            v = target;
    }
    *value = (uint16_t)v;
}
void rr_gles3_apply_input(void)
{
    save_tick();
    if (!interactive)
        return;
    bool coin = input_coin;
    if (coin_edges && (g_hw.inputs & 0x1000)) {
        coin = true;
        coin_edges--;
    }
    input_bit(0x1000, coin);
    input_bit(0x0040, input_view);
    input_bit(0x0001, input_down);
    input_bit(0x0002, input_up);
    g_hw.steer = (uint16_t)(0x800 + input_wheel * ((g_rr_game->steer_max - g_rr_game->steer_min) / 2) / 32767);
    bool gas = input_gas || start_ticks;
    if (start_ticks)
        start_ticks--;
    input_ramp(&g_hw.gas, gas ? g_rr_game->gas_max : 0, g_rr_game->gas_delta);
    input_ramp(&g_hw.brake, input_brake ? g_rr_game->brake_max : 0, g_rr_game->brake_delta);
}
bool rr_gles3_interactive(void)
{
    return interactive;
}

static bool audio_enabled, deferred_record_exit;
static const char *thread_csv;
bool rr_gles3_full_telemetry(void)
{
    return thread_csv != NULL;
}
static uint32_t profile_from = 1;
static double last_presentation;
static rr_presentation_metrics last_metrics;
static double seconds(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}
double rr_gles3_last_presentation(void)
{
    return last_presentation;
}
rr_presentation_metrics rr_gles3_last_metrics(void)
{
    return last_metrics;
}
uint32_t rr_gles3_profile_from(void)
{
    return profile_from;
}
bool rr_gles3_failed(void)
{
    return failed;
}
bool rr_gles3_record_exit_pending(void)
{
    return deferred_record_exit;
}
double rr_gles3_stage_begin(void)
{
    return 0;
}
void rr_gles3_stage_end(rr_gles3_stage stage, double start)
{
    (void)stage;
    (void)start;
}
void rr_gles3_scene_tick(bool walk, unsigned direct)
{
    (void)walk;
    (void)direct;
}

static bool input_exit(void)
{
    save_emit();
#ifdef RR_TITLE_OPTIONS
    if (return_requested) {
        fflush(stderr);
        fprintf(stderr, "[SAVE-FLUSH] return-title\n");
        fflush(stderr);
        struct pollfd p = {.fd = save_ack, .events = POLLIN};
        char ack[64] = {0};
        int ready;
        do {
            ready = poll(&p, 1, 5000);
        } while (ready < 0 && errno == EINTR);
        ssize_t n = ready > 0 ? read(save_ack, ack, sizeof ack - 1) : -1;
        if (n <= 0 || strcmp(ack, "OK return-title\n")) {
            fprintf(stderr, "[TITLE-ERROR] SAVE_ACK operation=return to title file=rr_eeprom.nv\n");
            failed = true;
            return_requested = false;
        } else
            fprintf(stderr, "[RETURN-TITLE] save acknowledged before worker shutdown\n");
        return false;
    }
#endif
    if (rr_input_recording()) {
        deferred_record_exit = true;
        return true;
    }
    return false;
}
static bool pump_input(void)
{
    if (deferred_record_exit)
        return true;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (!input_event(&event))
            return input_exit();
        if (event.type == SDL_KEYDOWN && !event.key.repeat && event.key.keysym.scancode == SDL_SCANCODE_F3)
            rr_input_record_marker();
        if (input_log && (event.type == SDL_JOYBUTTONDOWN || event.type == SDL_JOYBUTTONUP))
            fprintf(stderr,
                    "[INPUT] joystick=%d button=%u pressed=%u\n",
                    event.jbutton.which,
                    event.jbutton.button,
                    event.jbutton.state);
        if (input_log && event.type == SDL_JOYAXISMOTION && abs(event.jaxis.value) > 8192)
            fprintf(stderr,
                    "[INPUT] joystick=%d axis=%u value=%d\n",
                    event.jaxis.which,
                    event.jaxis.axis,
                    event.jaxis.value);
        if (input_log && event.type == SDL_JOYHATMOTION)
            fprintf(
                stderr, "[INPUT] joystick=%d hat=%u value=%u\n", event.jhat.which, event.jhat.hat, event.jhat.value);
        if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE))
            return input_exit();
    }
    if (interactive && !input_poll())
        return input_exit();
    return true;
}
bool rr_gles3_pump(void)
{
    return pump_input();
}
bool rr_gles3_options(int *argc, char **argv)
{
    if (!rr_sim_options(argc, argv))
        return false;
    int at = 2;
    for (int i = 2; i < *argc; i++) {
        const char *p = argv[i];
        if (!strcmp(p, "--interactive"))
            interactive = true;
        else if (!strcmp(p, "--audio") && i + 1 < *argc) {
            p = argv[++i];
            if (strcmp(p, "on") && strcmp(p, "off"))
                return false;
            audio_enabled = !strcmp(p, "on");
        } else if (!strcmp(p, "--thread-profile") && i + 1 < *argc)
            thread_csv = argv[++i];
        else if (!strcmp(p, "--es-input") && i + 1 < *argc)
            es_input = argv[++i];
        else if (!strcmp(p, "--controller-db") && i + 1 < *argc)
            controller_db = argv[++i];
        else if (!strcmp(p, "--overlay") && i + 1 < *argc) {
            if (!rr_overlay_mode(argv[++i]))
                return false;
        } else if (!strcmp(p, "--input-log"))
            input_log = true;
        else if (!strcmp(p, "--presentation-thread") && i + 1 < *argc) {
            if (strcmp(argv[++i], "on"))
                return false;
        } else if (!strcmp(p, "--timing") && i + 1 < *argc) {
            if (strcmp(argv[++i], "independent"))
                return false;
        } else
            argv[at++] = argv[i];
    }
    *argc = at;
    argv[at] = NULL;
    rr_presentation_configure(true, false, thread_csv, false);
    return true;
}
bool rr_gles3_mode(bool diagnostics)
{
    if (!interactive) {
        if (getenv("ENG_GLES3_SESSION") || getenv("ENG_GLES3_SAVE_PATH") || getenv("ENG_GLES3_CONTROLLER_PATH") ||
            getenv("ENG_GLES3_SAVE_ACK_FD") || es_input || controller_db) {
            fprintf(stderr, "[TITLE-ERROR] MODE_ISOLATION: personal session in noninteractive mode\n");
            return false;
        }
        return true;
    }
#ifndef RR_TITLE_OPTIONS
    (void)diagnostics;
    fprintf(stderr, "[TITLE-ERROR] MODE_ISOLATION: interactive product only\n");
    return false;
#else
    const char *session = getenv("ENG_GLES3_SESSION"), *save = getenv("ENG_GLES3_SAVE_PATH"),
               *seed = getenv("ENG_GLES3_INITIAL_EEPROM"), *ack = getenv("ENG_GLES3_SAVE_ACK_FD");
    if (diagnostics || getenv("ENG_GLES3_FACTORY_EEPROM") || !session || strcmp(session, "1") || !save || !seed ||
        strcmp(save, seed) || !getenv("ENG_GLES3_CONTROLLER_PATH") || !getenv("ENG_GLES3_DEFAULT_INPUT") || !ack ||
        es_input || controller_db) {
        fprintf(stderr, "[TITLE-ERROR] MODE_ISOLATION: interactive session configuration\n");
        return false;
    }
    char *end;
    errno = 0;
    long fd = strtol(ack, &end, 10);
    if (errno || !*ack || *end || fd < 3 || fd > 65535 || fcntl((int)fd, F_GETFD) < 0) {
        fprintf(stderr, "[TITLE-ERROR] SAVE_ACK: invalid supervisor channel\n");
        return false;
    }
    save_ack = (int)fd;
    return true;
#endif
}
int rr_gles3_exit_code(void)
{
    if (failed)
        return 2;
#ifdef RR_TITLE_OPTIONS
    if (return_requested)
        return TITLE_RETURN_CODE;
#endif
    return 0;
}
bool rr_gles3_open(void)
{
    save_begin(false);
    bool mali = false;
    for (int i = 0; i < SDL_GetNumVideoDrivers(); i++)
        if (!strcmp(SDL_GetVideoDriver(i), "mali"))
            mali = true;
    if (!getenv("SDL_VIDEODRIVER") && mali)
        SDL_setenv("SDL_VIDEODRIVER", "mali", 0);
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK)) {
        fprintf(stderr, "[TITLE-ERROR] SDL_INIT operation=initialize file=SDL: %s\n", SDL_GetError());
        return false;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    window = SDL_CreateWindow("Rave Racer GLES3", 0, 0, 640, 480, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
    if (!window) {
        fprintf(stderr, "[TITLE-ERROR] SDL_WINDOW operation=create 640x480 window file=SDL: %s\n", SDL_GetError());
        return false;
    }
    atexit(rr_gles3_close);
    context = SDL_GL_CreateContext(window);
    if (!context) {
        fprintf(stderr, "[TITLE-ERROR] GL_CONTEXT operation=create GLES3 context file=SDL: %s\n", SDL_GetError());
        return false;
    }
    fprintf(stderr, "[GLES3-GL] %s; %s\n", glGetString(GL_RENDERER), glGetString(GL_VERSION));
    if (SDL_GL_SetSwapInterval(1) && !getenv("ENG_GLES3_HOST")) {
        fprintf(stderr, "[TITLE-ERROR] VSYNC operation=set swap interval file=SDL: %s\n", SDL_GetError());
        return false;
    }
#ifdef RR_TITLE_OPTIONS
    if (interactive) {
        const char *save = getenv("ENG_GLES3_SAVE_PATH"), *override = getenv("ENG_GLES3_CONTROLLER_PATH"),
                   *defaults = getenv("ENG_GLES3_DEFAULT_INPUT");
        int menu = rr_title_run(
            window, &title_pad, save, override, defaults, &title_profile, g_rr.rom, RR_ROM_SIZE, g_rr.eeprom);
        if (menu != 1) {
            rr_gles3_close();
            exit(menu);
        }
        save_begin(true);
        coin_edges = start_ticks = 0;
        memset(chord_start, 0, sizeof chord_start);
        memset(chord_select, 0, sizeof chord_select);
        return_hold = (ti_hold){0};
        overlay_held = false;
        g_hw.inputs = g_rr_game->inputs_idle;
        g_hw.steer = 0x800;
        g_hw.gas = g_hw.brake = 0;
        SDL_SetWindowTitle(window, "Rave Racer GLES3");
        fprintf(stderr, "[TITLE] handoff simulation_ticks=0\n");
    }
#endif
    if (!rr_video_layers_open() || !rr_renderer_open())
        return false;
    if (audio_enabled && !rr_audio_output_open())
        return false;
    if (!rr_presentation_start(window, context, 0, 0, 0, 1))
        return false;
    return true;
}
bool rr_gles3_present_prepared(const rr_presentation *s)
{
    eng_render_stats stats;
    double start = seconds();
    if (!rr_renderer_draw(s, &stats)) {
        failed = true;
        return false;
    }
#ifdef GLES3_VALIDATION
    extern void gles3_check_boot(const rr_presentation *);
    gles3_check_boot(s);
#endif
    rr_overlay_draw();
    double begin = seconds();
    SDL_GL_SwapWindow(window);
    last_presentation = seconds();
    rr_overlay_present(s->frame, last_presentation);
    last_metrics = (rr_presentation_metrics){.draw_wall = begin - start,
                                             .submission_wall = begin - start,
                                             .swap_wall = last_presentation - begin,
                                             .draws = stats.draws,
                                             .triangles = stats.triangles};
    return true;
}
void rr_gles3_close(void)
{
    if (!window)
        return;
    if (!rr_presentation_finish())
        failed = true;
    rr_sim_report();
    rr_memory_profile_report();
    rr_renderer_close();
    rr_overlay_close();
    rr_video_layers_close();
#ifdef RR_TITLE_OPTIONS
    if (title_pad) {
        SDL_JoystickClose(title_pad);
        title_pad = NULL;
    }
#endif
    if (context) {
        SDL_GL_DeleteContext(context);
        context = NULL;
    }
    SDL_DestroyWindow(window);
    window = NULL;
    SDL_Quit();
}
