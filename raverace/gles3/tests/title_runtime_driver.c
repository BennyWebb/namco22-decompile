/* Host test only: real SDL virtual joystick and physical input samples.
 * Reads emulated state to decide when to hold X; never writes game memory. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <SDL.h>
#include <dlfcn.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rr_mem.h"
#include "rr_lift_rt.h"
#include "lift_cpu.h"
#include "rr_hw.h"
#include "rr_game.h"
#include "title_input.h"
static SDL_Joystick *device;
static SDL_JoystickID instance;
static _Atomic unsigned menu_swaps;
static _Atomic bool game;
static unsigned generation;
static uint64_t hold_at, menu_press_at, deadline;
static FILE *record;
static unsigned next_frame, inputs = 0xfeff, steer = 0x800, gas, brake;
static bool next_valid;
static void next(void)
{
    char line[256];
    next_valid = false;
    while (fgets(line, sizeof line, record))
        if (sscanf(line, "F %u %x %x %x %x", &next_frame, &inputs, &steer, &gas, &brake) == 5) {
            next_valid = true;
            break;
        }
}
int SDL_Init(Uint32 flags)
{
    int (*real)(Uint32) = dlsym(RTLD_NEXT, "SDL_Init");
    int result = real(flags);
    if (result)
        return result;
    if ((flags & SDL_INIT_JOYSTICK) && !device) {
        SDL_VirtualJoystickDesc desc = {.version = SDL_VIRTUAL_JOYSTICK_DESC_VERSION,
                                        .type = SDL_JOYSTICK_TYPE_GAMECONTROLLER,
                                        .naxes = 4,
                                        .nbuttons = 16,
                                        .nhats = 1,
                                        .vendor_id = 1,
                                        .product_id = 1,
                                        .name = TITLE_DEVICE};
        int index = SDL_JoystickAttachVirtualEx(&desc);
        if (index < 0)
            abort();
        device = SDL_JoystickOpen(index);
        if (!device)
            abort();
        instance = SDL_JoystickInstanceID(device);
        deadline = SDL_GetTicks64() + 90000;
        const char *count = getenv("TITLE_TEST_COUNT");
        FILE *f = fopen(count, "r");
        if (f) {
            if (fscanf(f, "%u", &generation) != 1)
                abort();
            fclose(f);
        }
        generation++;
        f = fopen(count, "w");
        if (!f || fprintf(f, "%u\n", generation) < 0 || fclose(f))
            abort();
        record = fopen(getenv("TITLE_TEST_RECORDING"), "r");
        if (!record)
            abort();
        next();
        fprintf(stderr, "[TITLE-TEST] child=%u virtual controller ready\n", generation);
    }
    return result;
}
SDL_JoystickGUID SDL_JoystickGetGUID(SDL_Joystick *pad)
{
    if (pad && SDL_JoystickInstanceID(pad) == instance)
        return SDL_JoystickGetGUIDFromString(TITLE_GUID);
    SDL_JoystickGUID (*real)(SDL_Joystick *) = dlsym(RTLD_NEXT, "SDL_JoystickGetGUID");
    return real(pad);
}
void SDL_SetWindowTitle(SDL_Window *window, const char *title)
{
    void (*real)(SDL_Window *, const char *) = dlsym(RTLD_NEXT, "SDL_SetWindowTitle");
    real(window, title);
    if (!strcmp(title, "Rave Racer GLES3")) {
        if (rr_frame || g_hw.gas || g_hw.brake || g_hw.inputs != g_rr_game->inputs_idle || g_hw.steer != 0x800)
            abort();
        fprintf(stderr, "[TITLE-TEST] handoff inputs neutral; simulation_ticks=0\n");
    }
    atomic_store(&game, !strcmp(title, "Rave Racer GLES3"));
}
void SDL_GL_SwapWindow(SDL_Window *window)
{
    void (*real)(SDL_Window *) = dlsym(RTLD_NEXT, "SDL_GL_SwapWindow");
    if (!atomic_load(&game)) {
        if (rr_frame)
            abort();
        atomic_fetch_add(&menu_swaps, 1);
    }
    real(window);
}
static void update(void)
{
    if (!device)
        return;
    if (SDL_GetTicks64() > deadline) {
        fprintf(stderr, "[TITLE-TEST] runtime deadline exceeded\n");
        exit(91);
    }
    uint16_t buttons = 0;
    int axis = 0;
    if (!atomic_load(&game)) {
        unsigned tick = atomic_load(&menu_swaps);
        if (generation == 1) {
            if (tick >= 5 && !menu_press_at)
                menu_press_at = SDL_GetTicks64();
            if (menu_press_at && SDL_GetTicks64() - menu_press_at < 100)
                buttons = 1u << 3;
        } else if (tick >= 5)
            buttons = (1u << 9) | (1u << 10);
    } else {
        static unsigned active_inputs = 0xfeff, active_gas, active_brake, active_steer = 0x800;
        while (next_valid && next_frame <= rr_frame + 1) {
            active_inputs = inputs;
            active_gas = gas;
            active_brake = brake;
            active_steer = steer;
            next();
        }
        if (!(active_inputs & 0x1000))
            buttons |= 1u << 9;
        if (!(active_inputs & 0x40))
            buttons |= 1u << 5;
        if (!(active_inputs & 1))
            buttons |= 1u << 7;
        if (!(active_inputs & 2))
            buttons |= 1u << 8;
        if (active_gas)
            buttons |= 1u << 4;
        if (active_brake)
            buttons |= 1u << 3;
        axis = ((int)active_steer - 0x800) * 32767 / ((0xd80 - 0x280) / 2);
        if (axis > 32767)
            axis = 32767;
        if (axis < -32767)
            axis = -32767;
        unsigned state = (g_rr.wram[0x8da0] << 8) | g_rr.wram[0x8da1];
        if (state == 3 && rr_frame > 2000 && !hold_at) {
            hold_at = SDL_GetTicks64();
            fprintf(stderr, "[TITLE-TEST] hold X in race tick=%u state=%u\n", rr_frame, state);
        }
        if (hold_at)
            buttons |= 1u << 6;
        if (rr_frame > 5000 && !hold_at) {
            fprintf(stderr, "[TITLE-TEST] never entered race\n");
            exit(90);
        }
    }
    for (unsigned i = 0; i < 16; i++)
        if (SDL_JoystickSetVirtualButton(device, i, (buttons >> i) & 1))
            abort();
    if (SDL_JoystickSetVirtualAxis(device, 0, (Sint16)axis))
        abort();
}
int SDL_PollEvent(SDL_Event *event)
{
    int (*real)(SDL_Event *) = dlsym(RTLD_NEXT, "SDL_PollEvent");
    update();
    return real(event);
}
int SDL_WaitEventTimeout(SDL_Event *event, int timeout)
{
    int (*real)(SDL_Event *, int) = dlsym(RTLD_NEXT, "SDL_WaitEventTimeout");
    update();
    return real(event, timeout);
}
