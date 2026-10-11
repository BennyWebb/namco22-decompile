/* Small SDL host for the existing deterministic scheduler's GLES2 build. */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include "eng_gl.h"
#include "quad_gles2.h"
#include "tex_bake.h"
#include "rr_gles2.h"
#include "rr_gles2_timing.h"

static SDL_Window *window;
static SDL_GLContext context;
static SDL_GameController *controllers[16];
static SDL_Joystick *raw_joysticks[16];
#include "rr_gles2_input.inc"
static bool failed, unpaced, captured, independent, timing_explicit;
static unsigned render_hz;
static rr_frame_clock frame_clock;
static double timing_start, timing_window, timing_sim, timing_scene;
static unsigned timing_steps, timing_renders, timing_skipped, timing_steps_window, timing_renders_window;
static void timing_report(const char *kind);
static uint32_t capture_frame;
static const char *capture_path;
static double next_frame, report_start;
static unsigned report_frames;
static bool saw_geometry;
static uint32_t profile_from = 1;
static double simulation_start, prepare_start, simulation_elapsed, profile_report_start;
typedef struct {
    unsigned frames, first, last;
    double simulation, scene, lookup, upload, submission, swap, other, pacing, capture;
    double vertex_upload, draw_call;
    unsigned long long triangles, draws, binds, skips, texture, scissor, capacity, mutation, end, same;
    unsigned long long empty_texture, vertex_uploads, vertex_bytes, texture_barriers;
    unsigned long long cache_hits, cache_misses, cache_evictions, texture_uploads;
    size_t texture_bytes, texture_peak;
} profile;
static profile window_profile, total_profile;

static double seconds(void) { return (double)SDL_GetPerformanceCounter() / SDL_GetPerformanceFrequency(); }
void rr_gles2_prepare_begin(void)
{
    prepare_start = seconds();
    simulation_elapsed = prepare_start - simulation_start;
}
static void profile_add(profile *p, unsigned frame, const eng_gles2_stats *s,
                        double scene, double draw_total, double swap, double pacing, double capture_time)
{
    if (!p->frames) p->first = frame;
    p->frames++; p->last = frame;
    p->simulation += simulation_elapsed; p->scene += scene;
    p->lookup += s->texture_seconds; p->upload += s->upload_seconds;
    p->submission += s->draw_seconds; p->swap += swap; p->pacing += pacing; p->capture += capture_time;
    p->vertex_upload += s->vertex_upload_seconds; p->draw_call += s->draw_call_seconds;
    p->other += draw_total - s->texture_seconds - s->upload_seconds - s->draw_seconds;
    p->triangles += s->triangles; p->draws += s->draws;
    p->binds += s->binds; p->skips += s->bind_skips;
    p->texture += s->flush_texture; p->scissor += s->flush_scissor;
    p->capacity += s->flush_capacity; p->mutation += s->flush_mutation;
    p->end += s->flush_end; p->same += s->same_texture;
    p->empty_texture += s->empty_texture_flush;
    p->vertex_uploads += s->vertex_uploads; p->vertex_bytes += s->vertex_upload_bytes;
    p->texture_barriers += s->texture_barriers;
    p->cache_hits += s->cache_hits; p->cache_misses += s->cache_misses;
    p->cache_evictions += s->cache_evictions; p->texture_uploads += s->texture_uploads;
    p->texture_bytes = s->texture_bytes;
    if (s->texture_bytes > p->texture_peak) p->texture_peak = s->texture_bytes;
}
static void profile_report(const char *kind, const profile *p)
{
    if (!p->frames) return;
    double ms = 1000.0 / p->frames;
    fprintf(stderr, "[PROFILE] %s mode=%s frames=%u..%u samples=%u avg_ms simulation=%.3f scene=%.3f lookup_decode=%.3f uploads=%.3f submission=%.3f swap=%.3f render_other=%.3f pacing=%.3f capture=%.3f vertex_upload=%.3f draw_call=%.3f avg_triangles=%.1f avg_draws=%.1f avg_binds=%.1f avg_skipped_binds=%.1f flushes texture=%llu scissor=%llu capacity=%llu mutation=%llu end=%llu same_texture=%llu empty_texture_flush=%llu vbo=%s avg_vertex_uploads=%.2f avg_vertex_bytes=%.1f texture_barriers=%llu\n",
            kind, eng_gles2_mode_name(), p->first, p->last, p->frames,
            p->simulation*ms, p->scene*ms, p->lookup*ms, p->upload*ms,
            p->submission*ms, p->swap*ms, p->other*ms, p->pacing*ms, p->capture*ms,
            p->vertex_upload*ms, p->draw_call*ms,
            (double)p->triangles/p->frames, (double)p->draws/p->frames,
            (double)p->binds/p->frames, (double)p->skips/p->frames,
            p->texture, p->scissor, p->capacity, p->mutation, p->end, p->same, p->empty_texture, eng_gles2_vbo_mode_name(),
            (double)p->vertex_uploads/p->frames, (double)p->vertex_bytes/p->frames, p->texture_barriers);
    fprintf(stderr, "[PROFILE-CACHE] %s mode=%s frames=%u..%u samples=%u hits=%llu misses=%llu evictions=%llu texture_uploads=%llu GPU_end=%zu GPU_frame_end_peak=%zu\n",
            kind, eng_gles2_mode_name(), p->first, p->last, p->frames,
            p->cache_hits, p->cache_misses, p->cache_evictions, p->texture_uploads,
            p->texture_bytes, p->texture_peak);
}
static bool sdl_ok(bool ok, const char *stage)
{
    if (!ok) { fprintf(stderr, "[GLES2] %s: %s\n", stage, SDL_GetError()); failed = true; }
    return ok;
}

bool rr_gles2_options(int *argc, char **argv)
{
    int output = 1;
    for (int i = 1; i < *argc; i++) {
        if (!strcmp(argv[i], "--timing") && i+1 < *argc) {
            const char *mode=argv[++i];
            if (strcmp(mode,"independent") && strcmp(mode,"coupled")) {
                fprintf(stderr,"[TIMING] use independent or coupled\n"); return false;
            }
            independent=!strcmp(mode,"independent"); timing_explicit=true;
        }
        else if (!strcmp(argv[i],"--render-hz") && i+1 < *argc) {
            char *end; const char *n=argv[++i]; errno=0; unsigned long hz=strtoul(n,&end,10);
            if(errno || n==end || *n<'0' || *n>'9' || *end || !hz || hz>60) {
                fprintf(stderr,"[TIMING] render-hz must be 1..60\n");return false;
            }
            render_hz=(unsigned)hz;
        }
        else if (!strcmp(argv[i], "--interactive")) interactive = true;
        else if (!strcmp(argv[i], "--input-log")) input_log = true;
        else if (!strcmp(argv[i], "--controller-db") && i+1 < *argc) controller_db = argv[++i];
        else if (!strcmp(argv[i], "--es-input") && i+1 < *argc) es_input = argv[++i];
        else if (!strcmp(argv[i], "--unpaced")) unpaced = true;
        else if (!strcmp(argv[i], "--diagnostic-colours")) eng_gles2_diagnostic(true);
        else if (!strcmp(argv[i], "--render-mode") && i+1 < *argc) {
            if (!eng_gles2_mode(argv[++i])) {
                fprintf(stderr, "[GLES2] render mode: textured, diagnostic, white-batches or textured-no-rebind\n");
                return false;
            }
        }
        else if (!strcmp(argv[i], "--vbo-upload") && i+1 < *argc) {
            if (!eng_gles2_vbo_mode(argv[++i])) {
                fprintf(stderr, "[GLES2] VBO upload mode: frame or legacy\n"); return false;
            }
        }
        else if (!strcmp(argv[i], "--profile-from") && i+1 < *argc) {
            char *end; const char *number = argv[++i]; errno = 0;
            unsigned long n = strtoul(number, &end, 10);
            if (errno || number == end || *number < '0' || *number > '9' || *end || !n || n > INT32_MAX) {
                fprintf(stderr, "[GLES2] invalid profile start frame\n"); return false;
            }
            profile_from = (uint32_t)n;
        }
        else if (!strcmp(argv[i], "--texture-budget-mib") && i+1 < *argc) {
            char *end; const char *number = argv[++i]; errno = 0;
            unsigned long n = strtoul(number, &end, 10);
            if (errno || number == end || *number < '0' || *number > '9' || *end ||
                n > 64 || !eng_gles2_texture_budget((unsigned)n)) {
                fprintf(stderr, "[TEX] invalid budget: use 1..64 MiB, no larger than build budget\n");
                return false;
            }
        }
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
    if (!timing_explicit) independent=interactive;
    if ((independent && unpaced) || (!independent && render_hz)) {
        fprintf(stderr,"[TIMING] independent requires paced execution; render-hz requires independent\n");return false;
    }
    if ((capture_path != NULL) != (capture_frame != 0)) {
        fprintf(stderr, "[GLES2] specify both --capture FILE.ppm and --capture-frame N\n"); return false;
    }
    return true;
}

static void scan_controllers(void)
{
    for (int i = 0; i < 16; i++) {
        if (raw_joysticks[i] && !SDL_JoystickGetAttached(raw_joysticks[i])) {
            SDL_JoystickClose(raw_joysticks[i]); raw_joysticks[i] = NULL;
        }
        if (controllers[i] && !SDL_GameControllerGetAttached(controllers[i])) {
            SDL_GameControllerClose(controllers[i]); controllers[i] = NULL;
            chord_start[i]=chord_select[i]=false;
        }
    }
    for (int device = 0; device < SDL_NumJoysticks(); device++) {
        input_map_device(device);
        if (!SDL_IsGameController(device)) {
            fprintf(stderr, "[GLES2] unmapped joystick: %s (START+SELECT needs an SDL mapping)\n", SDL_JoystickNameForIndex(device));
            SDL_JoystickID raw_id = SDL_JoystickGetDeviceInstanceID(device);
            int slot = -1; bool present = false;
            for (int i=0;i<16;i++) {
                if (!raw_joysticks[i]) slot=i;
                else if (SDL_JoystickInstanceID(raw_joysticks[i])==raw_id) present=true;
            }
            if (!present && slot>=0) raw_joysticks[slot]=SDL_JoystickOpen(device);
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
            if (controllers[slot]) {
                SDL_Joystick *js = SDL_GameControllerGetJoystick(controllers[slot]);
                char guid[33]; SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(js),guid,sizeof guid);
                fprintf(stderr,"[INPUT] controller=%s GUID=%s instance=%d axes=%d buttons=%d hats=%d\n",
                    SDL_GameControllerName(controllers[slot]),guid,SDL_JoystickInstanceID(js),
                    SDL_JoystickNumAxes(js),SDL_JoystickNumButtons(js),SDL_JoystickNumHats(js));
            }
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
    window = SDL_CreateWindow("Rave Racer GLES2", 0, 0, 640, 480,
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
    if (SDL_GL_SetSwapInterval((unpaced || independent) ? 0 : 1) != 0)
        fprintf(stderr, "[GLES2] swap interval: %s; software pacing available\n", SDL_GetError());
    if (!eng_gles2_init()) { failed = true; return false; }
    if (controller_db && SDL_GameControllerAddMappingsFromFile(controller_db) < 0) {
        fprintf(stderr,"[INPUT] controller database: %s\n",SDL_GetError()); failed=true; return false;
    }
    scan_controllers();
    fprintf(stderr,"[INPUT] mode=%s; interactive: SELECT coin, START/south/R1 gas-confirm, east/L1 brake, stick/D-pad steer, START+SELECT quit\n",
            interactive ? "interactive" : "scripted");
    fprintf(stderr, "[GLES2] exit: START+SELECT/BACK, Escape or SDL quit; frames limited by --frames\n");
    next_frame = report_start = seconds();
    simulation_start = profile_report_start = next_frame;
    fprintf(stderr, "[PROFILE] CPU wall time (driver waits included); no glFinish; start frame=%u mode=%s\n",
            profile_from, eng_gles2_mode_name());
    fprintf(stderr,"[TIMING] mode=%s target_hz=%.9f render_hz=%u (0=adaptive) catchup_limit=%u\n",
            independent?"independent":"coupled",1.0/RR_FRAME_SECONDS,render_hz,RR_MAX_CATCHUP);
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
    tex_bake_gles2_report();
    return true;
}

static bool pump_input(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (!input_event(&event)) return false;
        if (input_log && (event.type == SDL_JOYBUTTONDOWN || event.type == SDL_JOYBUTTONUP))
            fprintf(stderr,"[INPUT] joystick=%d button=%u pressed=%u\n",event.jbutton.which,event.jbutton.button,event.jbutton.state);
        if (input_log && event.type == SDL_JOYAXISMOTION && abs(event.jaxis.value)>8192)
            fprintf(stderr,"[INPUT] joystick=%d axis=%u value=%d\n",event.jaxis.which,event.jaxis.axis,event.jaxis.value);
        if (input_log && event.type == SDL_JOYHATMOTION)
            fprintf(stderr,"[INPUT] joystick=%d hat=%u value=%u\n",event.jhat.which,event.jhat.hat,event.jhat.value);
        if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) return false;
        if (event.type == SDL_CONTROLLERDEVICEADDED || event.type == SDL_CONTROLLERDEVICEREMOVED ||
            event.type == SDL_JOYDEVICEADDED || event.type == SDL_JOYDEVICEREMOVED) scan_controllers();
    }
    if (interactive && !input_poll()) return false;
    for (int i = 0; i < 16; i++) if (controllers[i] &&
        SDL_GameControllerGetButton(controllers[i], SDL_CONTROLLER_BUTTON_START) &&
        SDL_GameControllerGetButton(controllers[i], SDL_CONTROLLER_BUTTON_BACK)) return false;
    return true;
}

static void timing_report(const char *kind)
{
    if (!timing_steps) return;
    double now=seconds(), elapsed=now-timing_start, window_elapsed=now-timing_window;
    fprintf(stderr,"[TIMING] %s mode=%s steps=%u renders=%u skipped=%u sim_hz=%.2f render_fps=%.2f window_sim_hz=%.2f window_render_fps=%.2f lag_ms=%.3f max_lag_ms=%.3f discarded_lag_ms=%.3f resyncs=%u avg_sim_ms=%.3f avg_scene_ms=%.3f\n",
        kind,independent?"independent":"coupled",timing_steps,timing_renders,timing_skipped,
        elapsed>0?timing_steps/elapsed:0,elapsed>0?timing_renders/elapsed:0,
        window_elapsed>0?timing_steps_window/window_elapsed:0,window_elapsed>0?timing_renders_window/window_elapsed:0,
        frame_clock.lag*1000,frame_clock.max_lag*1000,frame_clock.discarded*1000,frame_clock.resyncs,
        timing_sim*1000/timing_steps,timing_scene*1000/timing_steps);
    timing_window=now; timing_steps_window=timing_renders_window=0;
}

bool rr_gles2_frame(uint32_t frame, const geo_quad *quads, int count, const float bg[3])
{
    double scene_elapsed = seconds() - prepare_start;
    if (!pump_input()) return false;
    bool render=true;
    if (!timing_steps) timing_start=timing_window=seconds();
    timing_steps++; timing_steps_window++; timing_sim+=simulation_elapsed; timing_scene+=scene_elapsed;
    if (independent) {
        rr_clock_begin(&frame_clock,seconds());
        while (seconds()<frame_clock.deadline) {
            SDL_Delay(1); /* poll during waits, without applying extra emulated samples */
            if (!pump_input()) return false;
        }
        render=rr_clock_render(&frame_clock,seconds(),render_hz,capture_frame==frame);
    }
    if (!render) {
        timing_skipped++;
        if (seconds()-timing_window>=5) timing_report("window");
        simulation_start=seconds();
        return true;
    }
    timing_renders++; timing_renders_window++;
    eng_gles2_stats stats;
    double draw_start = seconds();
    if (!eng_gles2_draw(quads, count, bg, &stats)) { failed = true; return false; }
    double draw_elapsed = seconds() - draw_start;
    if (!saw_geometry && stats.triangles) {
        fprintf(stderr, "[GLES2] first geometry frame=%u quads=%d triangles=%u draws=%u\n", frame, count, stats.triangles, stats.draws);
        saw_geometry = true;
    }
    double capture_start = seconds();
    if (capture_frame == frame && !capture(frame, &stats)) { failed = true; return false; }
    double capture_elapsed = seconds() - capture_start;
    double swap_start = seconds();
    SDL_GL_SwapWindow(window);
    double swap_elapsed = seconds() - swap_start;
    report_frames++;
    double now = seconds();
    if (now - report_start >= 5) {
        fprintf(stderr, "[GLES2] frame=%u fps=%.2f quads=%d triangles=%u draws=%u\n",
                frame, report_frames / (now-report_start), count, stats.triangles, stats.draws);
        tex_bake_gles2_report();
        report_start = now; report_frames = 0;
    }
    double pacing_start = seconds();
    if (!unpaced && !independent) {
        next_frame += 1.0 / 59.906;
        double remaining = next_frame - seconds();
        if (remaining > 0) SDL_Delay((Uint32)(remaining * 1000 + 0.999));
        else if (remaining < -1.0 / 59.906) next_frame = seconds();
    }
    if (independent && seconds()-timing_window>=5) timing_report("window");
    if (frame >= profile_from) {
        double pacing_elapsed = seconds() - pacing_start;
        profile_add(&window_profile, frame, &stats, scene_elapsed, draw_elapsed, swap_elapsed, pacing_elapsed, capture_elapsed);
        profile_add(&total_profile, frame, &stats, scene_elapsed, draw_elapsed, swap_elapsed, pacing_elapsed, capture_elapsed);
        if (seconds() - profile_report_start >= 5) {
            profile_report("window", &window_profile);
            memset(&window_profile, 0, sizeof window_profile); profile_report_start = seconds();
        }
    } else profile_report_start = seconds();
    simulation_start = seconds();
    return true;
}

bool rr_gles2_failed(void) { return failed; }
void rr_gles2_close(void)
{
    if (!window) return;
    timing_report("total");
    profile_report("window-final", &window_profile);
    profile_report("total", &total_profile);
    if (capture_frame && !captured) { fprintf(stderr, "[GLES2] requested geometry capture was not completed\n"); failed = true; }
    for (int i = 0; i < 16; i++) if (controllers[i]) { SDL_GameControllerClose(controllers[i]); controllers[i] = NULL; }
    for (int i=0;i<16;i++) if (raw_joysticks[i]) { SDL_JoystickClose(raw_joysticks[i]); raw_joysticks[i]=NULL; }
    if (context) { eng_gles2_close(); SDL_GL_DeleteContext(context); context = NULL; }
    SDL_DestroyWindow(window); window = NULL;
    SDL_Quit();
}
