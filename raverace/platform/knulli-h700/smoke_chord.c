#define _GNU_SOURCE
#include <SDL.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
int SDL_PollEvent(SDL_Event *event)
{
    static double began;
    static unsigned phase;
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    double now = t.tv_sec + t.tv_nsec * 1e-9;
    if (!began)
        began = now;
    const char *s = getenv("ENG_GLES3_SMOKE_CHORD_SECONDS");
    if (event && s && now - began >= atof(s) && phase < 2) {
        SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(0);
        if (id < 0) {
            fprintf(stderr, "[INPUT-SMOKE] no physical controller; chord not injected\n");
            phase = 2;
        } else {
            SDL_zero(*event);
            event->type = SDL_CONTROLLERBUTTONDOWN;
            event->cbutton.which = id;
            event->cbutton.button = phase ? SDL_CONTROLLER_BUTTON_BACK : SDL_CONTROLLER_BUTTON_START;
            event->cbutton.state = SDL_PRESSED;
            phase++;
            if (phase == 2)
                fprintf(stderr, "[INPUT-SMOKE] START+SELECT instance=%d elapsed=%.3f\n", id, now - began);
            return 1;
        }
    }
    int (*real)(SDL_Event *) = dlsym(RTLD_NEXT, "SDL_PollEvent");
    return real(event);
}
