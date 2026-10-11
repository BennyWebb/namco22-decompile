/* Small SDL host for the existing deterministic scheduler's GLES2 build. */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include "eng_gl.h"
#include "quad_gles2.h"
#include "rr_gles2.h"

static SDL_Window *window;
static SDL_GLContext context;
static SDL_GameController *controllers[16];
static bool failed, unpaced, captured;
static uint32_t capture_frame;
static const char *capture_path;
static double next_frame, report_start;
static unsigned report_frames;
static bool saw_geometry;

static double seconds(void) { return (double)SDL_GetPerformanceCounter() / SDL_GetPerformanceFrequency(); }
static bool sdl_ok(bool ok, const char *stage)
{
    if (!ok) { fprintf(stderr, "[GLES2] %s: %s\n", stage, SDL_GetError()); failed = true; }
    return ok;
}

bool rr_gles2_options(int *argc, char **argv)
{
    int output = 1;
    for (int i = 1; i < *argc; i++) {
        if (!strcmp(argv[i], "--unpaced")) unpaced = true;
        else if (!strcmp(argv[i], "--capture") && i+1 < *argc) capture_path = argv[++i];
        else if (!strcmp(argv[i], "--capture-frame") && i+1 < *argc) {
            char *end; const char *number = argv[++i]; errno = 0;
            unsigned long n = strtoul(number, &end, 10);
            if (errno || number == end || *number < '0' || *number > '9' || *end || !n || n > INT32_MAX) {
                fprintf(stderr, "[GLES2] invalid capture frame\n"); return false;
            }
            capture_frame = (uint32_t)n;
        } else argv[output++] = argv[i];
    }
    *argc = output; argv[output] = NULL;
    if ((capture_path != NULL) != (capture_frame != 0)) {
        fprintf(stderr, "[GLES2] specify both --capture FILE.ppm and --capture-frame N\n"); return false;
    }
    return true;
}

static void scan_controllers(void)
{
    for (int i = 0; i < 16; i++) {
        if (controllers[i] && !SDL_GameControllerGetAttached(controllers[i])) {
            SDL_GameControllerClose(controllers[i]); controllers[i] = NULL;
        }
    }
    for (int device = 0; device < SDL_NumJoysticks(); device++) {
        if (!SDL_IsGameController(device)) {
            fprintf(stderr, "[GLES2] unmapped joystick: %s (START+SELECT needs an SDL mapping)\n", SDL_JoystickNameForIndex(device));
            continue;
        }
        SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(device);
        int slot = -1; bool present = false;
        for (int i = 0; i < 16; i++) {
            if (!controllers[i]) slot = i;
            else if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controllers[i])) == id) present = true;
        }
        if (!present && slot >= 0) {
            controllers[slot] = SDL_GameControllerOpen(device);
            if (controllers[slot]) fprintf(stderr, "[GLES2] controller: %s\n", SDL_GameControllerName(controllers[slot]));
            else fprintf(stderr, "[GLES2] controller open: %s\n", SDL_GetError());
        }
    }
}

bool rr_gles2_open(void)
{
    bool mali = false;
    for (int i = 0; i < SDL_GetNumVideoDrivers(); i++) {
        const char *name = SDL_GetVideoDriver(i);
        fprintf(stderr, "[GLES2] SDL driver %d: %s\n", i, name);
        if (!strcmp(name, "mali")) mali = true;
    }
    if (!getenv("SDL_VIDEODRIVER") && mali) SDL_setenv("SDL_VIDEODRIVER", "mali", 0);
    if (!sdl_ok(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER) == 0, "SDL_Init")) return false;
    SDL_version compiled, linked;
    SDL_VERSION(&compiled); SDL_GetVersion(&linked);
    fprintf(stderr, "[GLES2] SDL compiled=%u.%u.%u runtime=%u.%u.%u\n",
            compiled.major, compiled.minor, compiled.patch, linked.major, linked.minor, linked.patch);
    struct { SDL_GLattr attr; int value; } attrs[] = {
        {SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES},
        {SDL_GL_CONTEXT_MAJOR_VERSION, 2}, {SDL_GL_CONTEXT_MINOR_VERSION, 0},
        {SDL_GL_DOUBLEBUFFER, 1}, {SDL_GL_DEPTH_SIZE, 16},
        {SDL_GL_RED_SIZE, 8}, {SDL_GL_GREEN_SIZE, 8}, {SDL_GL_BLUE_SIZE, 8}};
    for (size_t i = 0; i < sizeof attrs / sizeof *attrs; i++)
        if (!sdl_ok(SDL_GL_SetAttribute(attrs[i].attr, attrs[i].value) == 0, "context attribute")) { SDL_Quit(); return false; }
    window = SDL_CreateWindow("Rave Racer GLES2 Phase 1", 0, 0, 640, 480,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN);
    if (!sdl_ok(window != NULL, "fullscreen window")) { SDL_Quit(); return false; }
    atexit(rr_gles2_close);
    context = SDL_GL_CreateContext(window);
    if (!sdl_ok(context != NULL, "ES 2.0 context") ||
        !sdl_ok(SDL_GL_MakeCurrent(window, context) == 0, "make current")) return false;
    fprintf(stderr, "[GLES2] active SDL video driver: %s\n", SDL_GetCurrentVideoDriver());
    const GLenum names[] = {GL_VENDOR, GL_RENDERER, GL_VERSION, GL_SHADING_LANGUAGE_VERSION};
    const char *labels[] = {"vendor", "renderer", "version", "GLSL"};
    for (int i = 0; i < 4; i++) {
        const GLubyte *value = glGetString(names[i]);
        if (!value) { fprintf(stderr, "[GLES2] missing GL %s\n", labels[i]); failed = true; return false; }
        fprintf(stderr, "[GLES2] %s: %s\n", labels[i], value);
    }
    int profile = 0;
    if (!sdl_ok(SDL_GL_GetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, &profile) == 0, "query context profile") || profile != SDL_GL_CONTEXT_PROFILE_ES) {
        fprintf(stderr, "[GLES2] expected an ES context\n"); failed = true; return false;
    }
    int w, h; SDL_GL_GetDrawableSize(window, &w, &h);
    GLint depth; glGetIntegerv(GL_DEPTH_BITS, &depth);
    fprintf(stderr, "[GLES2] drawable %dx%d depth bits=%d\n", w, h, depth);
    if (w != 640 || h != 480 || depth < 16) {
        fprintf(stderr, "[GLES2] Phase 1 requires 640x480 with a depth buffer\n"); failed = true; return false;
    }
    if (SDL_GL_SetSwapInterval(unpaced ? 0 : 1) != 0)
        fprintf(stderr, "[GLES2] swap interval: %s; software pacing available\n", SDL_GetError());
    if (!eng_gles2_init()) { failed = true; return false; }
    scan_controllers();
    fprintf(stderr, "[GLES2] exit: START+SELECT/BACK, Escape or SDL quit; frames limited by --frames\n");
    next_frame = report_start = seconds();
    return true;
}

static bool capture(uint32_t frame, const eng_gles2_stats *stats)
{
    /* RGBA/UNSIGNED_BYTE is the portable GLES2 framebuffer readback format. */
    const size_t bytes = 640 * 480 * 4;
    unsigned char *pixels = malloc(bytes);
    if (!pixels) { fprintf(stderr, "[GLES2] screenshot allocation failed (%zu bytes)\n", bytes); return false; }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, 640, 480, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) { fprintf(stderr, "[GLES2] readback error 0x%x\n", error); free(pixels); return false; }
    FILE *fp = fopen(capture_path, "wb");
    if (!fp) { fprintf(stderr, "[GLES2] capture %s: %s\n", capture_path, strerror(errno)); free(pixels); return false; }
    fprintf(fp, "P6\n640 480\n255\n");
    for (int y = 479; y >= 0; y--) for (int x = 0; x < 640; x++)
        fwrite(pixels + (y * 640 + x) * 4, 1, 3, fp);
    bool ok = !ferror(fp);
    if (fclose(fp)) ok = false;
    free(pixels);
    if (!ok) { fprintf(stderr, "[GLES2] capture write failed\n"); return false; }
    fprintf(stderr, "[GLES2] captured frame %u: triangles=%u draws=%u file=%s\n", frame, stats->triangles, stats->draws, capture_path);
    if (!stats->triangles) { fprintf(stderr, "[GLES2] capture contains no submitted geometry; choose a later frame\n"); return false; }
    captured = true;
    return true;
}

bool rr_gles2_frame(uint32_t frame, const geo_quad *quads, int count, const float bg[3])
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) return false;
        if (event.type == SDL_CONTROLLERDEVICEADDED || event.type == SDL_CONTROLLERDEVICEREMOVED) scan_controllers();
    }
    for (int i = 0; i < 16; i++) if (controllers[i] &&
        SDL_GameControllerGetButton(controllers[i], SDL_CONTROLLER_BUTTON_START) &&
        SDL_GameControllerGetButton(controllers[i], SDL_CONTROLLER_BUTTON_BACK)) return false;
    eng_gles2_stats stats;
    if (!eng_gles2_draw(quads, count, bg, &stats)) { failed = true; return false; }
    if (!saw_geometry && stats.triangles) {
        fprintf(stderr, "[GLES2] first geometry frame=%u quads=%d triangles=%u draws=%u\n", frame, count, stats.triangles, stats.draws);
        saw_geometry = true;
    }
    if (capture_frame == frame && !capture(frame, &stats)) { failed = true; return false; }
    SDL_GL_SwapWindow(window);
    report_frames++;
    double now = seconds();
    if (now - report_start >= 5) {
        fprintf(stderr, "[GLES2] frame=%u fps=%.2f quads=%d triangles=%u draws=%u texture_bytes=4\n",
                frame, report_frames / (now-report_start), count, stats.triangles, stats.draws);
        report_start = now; report_frames = 0;
    }
    if (!unpaced) {
        next_frame += 1.0 / 59.906;
        double remaining = next_frame - seconds();
        if (remaining > 0) SDL_Delay((Uint32)(remaining * 1000 + 0.999));
        else if (remaining < -1.0 / 59.906) next_frame = seconds();
    }
    return true;
}

bool rr_gles2_failed(void) { return failed; }
void rr_gles2_close(void)
{
    if (!window) return;
    if (capture_frame && !captured) { fprintf(stderr, "[GLES2] requested geometry capture was not completed\n"); failed = true; }
    for (int i = 0; i < 16; i++) if (controllers[i]) { SDL_GameControllerClose(controllers[i]); controllers[i] = NULL; }
    if (context) { eng_gles2_close(); SDL_GL_DeleteContext(context); context = NULL; }
    SDL_DestroyWindow(window); window = NULL;
    SDL_Quit();
}
