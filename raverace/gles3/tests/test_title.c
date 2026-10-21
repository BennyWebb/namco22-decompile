#include "title.h"
#include "title_draw.h"
#include <GLES3/gl3.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
static void seal(uint8_t *p)
{
    unsigned sum = 0;
    for (unsigned i = 0; i < 30; i++)
        sum += p[i];
    sum = (~sum) & 65535;
    p[30] = sum >> 8;
    p[31] = sum;
}
static void capture(SDL_Window *window, tm_state *s, const char *directory, const char *name)
{
    char error[128];
    assert(title_draw(window, s, error, sizeof error));
    uint8_t *pixels = malloc(640 * 480 * 4);
    assert(pixels);
    glReadPixels(0, 0, 640, 480, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    char path[4096];
    snprintf(path, sizeof path, "%s/%s.rgba", directory, name);
    FILE *f = fopen(path, "wbx");
    assert(f && fwrite(pixels, 1, 640 * 480 * 4, f) == 640 * 480 * 4 && !fclose(f));
    free(pixels);
}
int main(int argc, char **argv)
{
    assert(argc == 5);
    const char *seed = argv[1], *config = argv[2], *roms = argv[3], *out = argv[4];
    char error[128];
    ti_profile p;
    assert(ti_load(config, &p, error, sizeof error));
    ti_profile copy;
    assert(ti_parse(p.text, &copy, error, sizeof error) && !strcmp(copy.text, p.text));
    for (unsigned i = 0; i < TI_ACTIONS; i++)
        for (unsigned j = 0; j < 3; j++) {
            int ids[] = {6, 9, 10};
            copy = p;
            assert(!ti_bind(&copy, (ti_action)i, "button", ids[j], 1, error, sizeof error));
            assert(!strcmp(copy.text, p.text));
        }
    copy = p;
    assert(ti_bind(&copy, TI_GAS, "button", 11, 1, error, sizeof error));
    assert(ti_bind(&copy, TI_VIEW, "button", 4, 1, error, sizeof error));
    ti_sample physical_b = {.buttons = 1u << 4};
    ti_controls rebound;
    ti_poll(&copy, &physical_b, &rebound);
    assert(rebound.view && !rebound.gas);
    assert(!strcmp(copy.guid, p.guid) && !strcmp(copy.name, p.name));
    assert(strstr(copy.text, "name=\"joystick2up\" type=\"axis\" id=\"3\" value=\"-1\" code=\"5\""));
    char path[4096];
    snprintf(path, sizeof path, "%s/controller-input.cfg", out);
    assert(ti_write(path, &copy, error, sizeof error));
    assert(ti_write(path, &p, error, sizeof error));
    assert(ti_load(path, &copy, error, sizeof error) && !strcmp(copy.text, p.text));
    char temporary[4096];
    assert(snprintf(temporary, sizeof temporary, "%s.controller-new", path) < (int)sizeof temporary);
    FILE *stale = fopen(temporary, "wbx");
    assert(stale && !fclose(stale));
    assert(!ti_write(path, &p, error, sizeof error) && !strcmp(error, "IO_CREATE"));
    assert(ti_load(path, &copy, error, sizeof error) && !strcmp(copy.text, p.text));
    char bad[TITLE_CONFIG_SIZE];
    strcpy(bad, p.text);
    char *guid = strstr(bad, TITLE_GUID);
    assert(guid);
    guid[0] = '0';
    assert(!ti_parse(bad, &copy, error, sizeof error));
    strcpy(bad, p.text);
    strstr(bad, "</inputConfig>")[0] = 0;
    assert(!ti_parse(bad, &copy, error, sizeof error));
    strcpy(bad, p.text);
    strstr(bad, "id=\"3\"")[4] = 'x';
    assert(!ti_parse(bad, &copy, error, sizeof error));
    strcpy(bad, p.text);
    char *separator = strstr(bad, "<inputConfig") + strlen("<inputConfig");
    memmove(separator, separator + 1, strlen(separator));
    assert(!ti_parse(bad, &copy, error, sizeof error));
    for (unsigned i = 0; i < 16; i++) {
        ti_sample sample = {.buttons = 1u << i};
        ti_controls c;
        ti_poll(&p, &sample, &c);
        assert(c.gas == (i == 4 || i == 14));
        assert(c.brake == (i == 3 || i == 13));
        assert(c.up == (i == 8));
        assert(c.down == (i == 7));
        assert(c.view == (i == 5));
        assert(c.overlay == (i == 15));
        assert(c.coin == (i == 9));
        assert(c.start == (i == 10));
    }
    for (unsigned hat = 1; hat <= 8; hat *= 2) {
        ti_sample sample = {.hat = hat};
        ti_controls c;
        ti_poll(&p, &sample, &c);
        assert(!c.up && !c.down);
    }
    ti_sample chord = {.buttons = (1u << 9) | (1u << 10) | (1u << 3)};
    ti_controls controls;
    ti_poll(&p, &chord, &controls);
    assert(controls.exit && !controls.coin && !controls.start && !controls.gas);
    assert(!ti_released(&chord));
    ti_sample neutral = {0};
    assert(ti_released(&neutral));
    ti_hold h = {0};
    assert(!ti_hold_update(&h, true, true, 0));
    assert(!ti_hold_update(&h, true, true, 990));
    assert(ti_hold_update(&h, true, true, 1000));
    assert(!ti_hold_update(&h, true, true, 2000));
    assert(!ti_hold_update(&h, true, false, 3000));
    assert(!ti_hold_update(&h, true, false, 5000));
    rr_operator_file file;
    rr_operator_error oe;
    assert(rr_operator_load(seed, &file, &oe));
    tm_state *s = calloc(1, sizeof *s);
    assert(s);
    tm_init(s, &file, &p, &p);
    assert(tm_event(s, TK_DOWN) == TM_WAIT && s->focus == 1);
    tm_event(s, TK_A);
    assert(s->page == TM_GAME);
    tm_event(s, TK_RIGHT);
    assert(tm_dirty(s));
    tm_event(s, TK_B);
    assert(s->page == TM_ROOT);
    assert(tm_event(s, TK_START) == TM_WAIT && s->page == TM_START_CONFIRM);
    tm_event(s, TK_DOWN);
    assert(tm_event(s, TK_A) == TM_BOOT && !tm_dirty(s));
    for (unsigned page = TM_ROOT; page <= TM_ERROR; page++) {
        s->page = (tm_page)page;
        assert(tm_event(s, TK_EXIT) == TM_QUIT);
    }
    tm_init(s, &file, &p, &p);
    s->page = TM_CAPTURE;
    s->capture = 0;
    tm_event(s, TK_B);
    assert(s->page == TM_CAPTURE);
    tm_event(s, TK_START);
    assert(s->page == TM_CONTROLS);
    s->page = TM_RESET_CONFIRM;
    tm_event(s, TK_B);
    assert(s->page == TM_CONTROLS);
    rr_operator_file extra = file;
    uint8_t *g = extra.image.bytes + extra.image.record[3];
    g[18] = 0;
    memset(g + 2, 9, 3);
    memset(g + 5, 4, 8);
    seal(g);
    assert(rr_operator_decode(extra.image.bytes, 8192, &extra.image, &oe));
    tm_init(s, &extra, &p, &p);
    s->focus = 1;
    tm_event(s, TK_A);
    assert(s->page == TM_GAME && strstr(s->status, "EXTRA_MODE"));
    tm_event(s, TK_RIGHT);
    assert(!tm_dirty(s));
    tm_event(s, TK_B);
    assert(tm_event(s, TK_START) == TM_BOOT);
    uint8_t *rom = malloc(0x200000), *lane = malloc(0x80000);
    assert(rom && lane);
    const char *names[] = {"rv2_prguub.6d", "rv2_prgumb.8d", "rv2_prglmb.2d", "rv2_prgllb.4d"};
    for (unsigned i = 0; i < 4; i++) {
        snprintf(path, sizeof path, "%s/%s", roms, names[i]);
        FILE *f = fopen(path, "rb");
        assert(f && fread(lane, 1, 0x80000, f) == 0x80000 && !fclose(f));
        for (unsigned j = 0; j < 0x80000; j++)
            rom[j * 4 + i] = lane[j];
    }
    tm_init(s, &file, &p, &p);
    assert(tm_names(s, rom, 0x200000));
    for (unsigned i = 0; i < 8; i++)
        printf("car %u %s\n", i, s->car[i]);
    assert(!SDL_Init(SDL_INIT_VIDEO));
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
    SDL_Window *window = SDL_CreateWindow("Title visual test", 0, 0, 640, 480, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    assert(window);
    SDL_GLContext context = SDL_GL_CreateContext(window);
    assert(context && title_draw_open(error, sizeof error));
    for (unsigned focus = 0; focus < 5; focus++) {
        s->page = TM_ROOT;
        s->focus = focus;
        snprintf(path, sizeof path, "root-focus-%u", focus);
        capture(window, s, out, path);
    }
    s->page = TM_GAME;
    for (unsigned i = 0; i < 6; i++) {
        s->focus = i;
        snprintf(path, sizeof path, "game-focus-%u", i);
        capture(window, s, out, path);
    }
    s->page = TM_CAR;
    for (unsigned i = 0; i < 9; i++) {
        s->focus = i;
        snprintf(path, sizeof path, "car-%u", i);
        capture(window, s, out, path);
    }
    s->page = TM_CONTROLS;
    for (unsigned i = 0; i < 12; i++) {
        s->focus = i;
        snprintf(path, sizeof path, "controls-focus-%u", i);
        capture(window, s, out, path);
    }
    s->page = TM_CAPTURE;
    s->capture = 0;
    s->blocked = true;
    capture(window, s, out, "rebind-release");
    s->blocked = false;
    capture(window, s, out, "rebind-capture");
    s->page = TM_START_CONFIRM;
    s->focus = 0;
    capture(window, s, out, "save-confirm");
    s->page = TM_RESET_CONFIRM;
    s->focus = 0;
    capture(window, s, out, "reset-confirm");
    s->page = TM_QUIT_CONFIRM;
    s->focus = 0;
    capture(window, s, out, "quit-confirm");
    tm_error(s, "IO", "commit EEPROM", "/userdata/roms/ports/rave-racer/saves/rr_eeprom.nv.operator-new");
    capture(window, s, out, "error");
    tm_init(s, &extra, &p, &p);
    assert(tm_names(s, rom, 0x200000));
    s->focus = 1;
    tm_event(s, TK_A);
    capture(window, s, out, "extra-mode");
    strcpy(s->status, "\001");
    assert(!title_draw(window, s, error, sizeof error) && !strcmp(error, "GLYPH_INVALID"));
    s->status[0] = 0;
    glEnable(0xffffffffu);
    assert(!title_draw(window, s, error, sizeof error) && !strcmp(error, "GL_DRAW"));
    SDL_SetWindowSize(window, 320, 240);
    assert(!title_draw(window, s, error, sizeof error) && !strcmp(error, "DRAWABLE_SIZE"));
    title_draw_close();
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    free(s);
    free(lane);
    free(rom);
    puts("PASS title: parser/writer, reserved IDs, defaults, mapping, menu, hold, EXTRA_MODE, captures");
    return 0;
}
