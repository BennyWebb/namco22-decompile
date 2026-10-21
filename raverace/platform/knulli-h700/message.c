/* Setup/error screen, using the existing non-game DejaVu ASCII font. */
#include <SDL.h>
#include <stdio.h>
#include <string.h>
#include "rr_font.h"
#define COLUMNS 74
#define ROWS 22
static char text[8192];
static int draw(SDL_Renderer *renderer, int page)
{
    SDL_SetRenderDrawColor(renderer, 12, 18, 28, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
    int column = 0, line = 0;
    for (const unsigned char *s = (const unsigned char *)text; *s; s++) {
        if (*s == '\n' || column == COLUMNS) {
            column = 0;
            line++;
        }
        if (*s == '\n')
            continue;
        if (*s >= 32 && *s <= 126) {
            if (line / ROWS == page)
                for (int row = 0; row < 16; row++)
                    for (int col = 0; col < 8; col++)
                        if (rr_font8x16[*s - 32][row] & (128 >> col))
                            SDL_RenderDrawPoint(renderer, 16 + column * 8 + col, 20 + (line % ROWS) * 18 + row);
            column++;
        }
    }
    SDL_RenderPresent(renderer);
    return line / ROWS + 1;
}
int main(int argc, char **argv)
{
    if (argc != 3 || SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) < 0)
        return 2;
    SDL_Window *window = SDL_CreateWindow("Rave Racer setup", 0, 0, 640, 480, SDL_WINDOW_FULLSCREEN);
    SDL_Renderer *renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED) : NULL;
    if (!renderer) {
        fprintf(stderr, "Setup display failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 2;
    }
    SDL_RenderSetLogicalSize(renderer, 640, 480);
    int progress = !strcmp(argv[1], "progress"), page = 0, pages = 1, result = 0;
    if (!progress)
        snprintf(text, sizeof text, "Rave Racer\n\n%s", argv[2]);
    pages = draw(renderer, page);
    SDL_Joystick *joystick = SDL_NumJoysticks() ? SDL_JoystickOpen(0) : NULL;
    int waiting = 1;
    while (waiting) {
        if (progress) {
            char next[8192] = "Rave Racer\n\nPreparing setup...";
            FILE *f = fopen(argv[2], "rb");
            if (f) {
                size_t n = fread(next, 1, sizeof next - 1, f);
                next[n] = 0;
                fclose(f);
            }
            if (strcmp(text, next)) {
                snprintf(text, sizeof text, "%s", next);
                pages = draw(renderer, 0);
            }
        }
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                result = 130;
                waiting = 0;
            }
            if (!progress && (event.type == SDL_KEYDOWN || event.type == SDL_JOYBUTTONDOWN)) {
                if (++page >= pages)
                    waiting = 0;
                else
                    draw(renderer, page);
            }
        }
        if (progress && joystick && SDL_JoystickGetButton(joystick, 9) && SDL_JoystickGetButton(joystick, 10)) {
            result = 130;
            waiting = 0;
        }
        SDL_Delay(100);
    }
    if (joystick)
        SDL_JoystickClose(joystick);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return result;
}
