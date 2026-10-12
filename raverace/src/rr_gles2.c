/* Small SDL host for the existing deterministic scheduler's GLES2 build. */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include "eng_gl.h"
#include "quad_gles2.h"
#include "quad_gl.h"
#include "tex_bake.h"
#include "rr_gles2.h"
#include "rr_sim_profile.h"
#include "rr_mem.h"
#include "rr_gles2_timing.h"
#include "eng_audit.h"
static const char *audit_path;
static FILE *audit_file;
static eng_audit_scope audit_scene;
static double stage_cpu_start,stage_frame_cpu[RR_STAGE_COUNT];

static SDL_Window *window;
static SDL_GLContext context;
static SDL_GameController *controllers[16];
static SDL_Joystick *raw_joysticks[16];
#include "rr_gles2_input.inc"
static bool failed, unpaced, captured, independent, timing_explicit, profile_memory;
static unsigned render_hz, test_present_every;
static bool skip_geometry, frame_render = true;
static double early_host_wait;
static bool range_clock_started;
static unsigned range_resync_start;
static double range_discard_start;
static bool pump_input(void);
static bool choose_presentation(uint32_t frame);
static unsigned scene_ticks, scene_walks, scene_direct;
static uint64_t scene_digest = UINT64_C(14695981039346656037);
void rr_gles2_scene_tick(bool walk, unsigned direct)
{
    scene_ticks++; scene_walks += walk; scene_direct += direct;
    scene_digest = (scene_digest ^ ((uint64_t)direct << 1 | walk)) * UINT64_C(1099511628211);
}
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
uint32_t rr_gles2_profile_from(void) { return profile_from; }
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
#include "rr_gles2_gpu_audit.inc"

static double seconds(void) { return (double)SDL_GetPerformanceCounter() / SDL_GetPerformanceFrequency(); }
#include "rr_gles2_profile.inc"
static void audit_row(uint32_t frame,bool rendered,unsigned quads,const eng_gles2_stats *s)
{
    if(!eng_audit_enabled) return;
    if(frame>=profile_from) {
        fprintf(audit_file,"%u,%d,%u,%u,%u,%u,%llu,%u,%u,%u,%u,%u,%.9f",frame,rendered,quads,s->triangles,s->draws,s->vertex_uploads,
            s->vertex_upload_bytes,s->cache_hits,s->cache_misses,s->cache_evictions,s->texture_uploads,s->binds,seconds());
        for(int i=0;i<AUD_COUNT;i++) fprintf(audit_file,",%.9f,%.9f,%.9f,%.9f",eng_audit_wall[i],eng_audit_cpu[i],eng_audit_inclusive_wall[i],eng_audit_inclusive_cpu[i]);
        for(int i=0;i<RR_STAGE_COUNT;i++) fprintf(audit_file,",%.9f,%.9f",stage_frame[i],stage_frame_cpu[i]);
        const eng_quad_sort_stats *sort=eng_quad_sort_statistics();
        fprintf(audit_file,",%.9f,%.9f,%.9f,%.9f,%llu,%u",
            rendered?sort->keys_wall:0,rendered?sort->keys_cpu:0,
            rendered?sort->permutation_wall:0,rendered?sort->permutation_cpu:0,
            rendered?sort->moved_bytes:0,rendered?sort->allocations:0);
        fputc('\n',audit_file);
    }
    memset(stage_frame_cpu,0,sizeof stage_frame_cpu);eng_audit_reset();
}


bool rr_gles2_prepare_begin(uint32_t frame, bool *geometry)
{
    rr_sim_pause(frame);
    simulation_elapsed = seconds() - simulation_start;
    if (!range_clock_started && frame >= profile_from) {
        range_clock_started=true;
        range_resync_start=frame_clock.resyncs;
        range_discard_start=frame_clock.discarded;
    }
    early_host_wait = 0;
    frame_render = true;
    if (independent && (skip_geometry || test_present_every)) {
        eng_audit_scope audit_wait=eng_audit_begin(AUD_WAIT);
        double start = seconds();
        if (!choose_presentation(frame)) return false;
        early_host_wait = seconds() - start;
        eng_audit_end(audit_wait);
    }
    *geometry = !independent || !skip_geometry || frame_render;
    prepare_start = seconds();
    audit_scene=eng_audit_begin(AUD_SCENE);
    return true;
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
    if (!rr_sim_options(argc,argv)) return false;
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
        else if (!strcmp(argv[i], "--skip-geometry-on-dropped-frames") && i+1 < *argc) {
            const char *value=argv[++i];
            if (strcmp(value,"on") && strcmp(value,"off")) {
                fprintf(stderr,"[GLES2] skip-geometry-on-dropped-frames requires on or off\n"); return false;
            }
            skip_geometry=!strcmp(value,"on");
        }
        else if (!strcmp(argv[i], "--test-present-every") && i+1 < *argc) {
            char *end; const char *value=argv[++i]; errno=0;
            unsigned long n=strtoul(value,&end,10);
            if (errno || value==end || *value<'0' || *value>'9' || *end || n<2 || n>60) {
                fprintf(stderr,"[GLES2] test-present-every requires 2..60\n"); return false;
            }
            test_present_every=(unsigned)n;
        }
        else if (!strcmp(argv[i], "--profile-memory")) profile_memory = true;
        else if (!strcmp(argv[i], "--profile-stages")) profile_stages = true;
        else if (!strcmp(argv[i],"--audit-csv") && i+1<*argc) audit_path=argv[++i];
        else if (!strcmp(argv[i],"--audit-no-draw")) eng_audit_no_draw=true;
        else if (!strcmp(argv[i],"--audit-gpu-timers")) audit_gpu=true;
        else if (!strcmp(argv[i],"--audit-detail")) eng_audit_detailed=true;
        else if (!strcmp(argv[i],"--audit-flat-fragment")) eng_audit_flat_fragment=true;
        else if (!strcmp(argv[i],"--audit-half-resolution")) eng_audit_divisor=2;
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
                n > 128 || !eng_gles2_texture_budget((unsigned)n)) {
                fprintf(stderr, "[TEX] invalid budget: use 1..128 MiB, no larger than build maximum\n");
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
    if ((independent && unpaced) || (!independent && (render_hz || test_present_every))) {
        fprintf(stderr,"[TIMING] independent requires paced execution; render-hz requires independent\n");return false;
    }
    if ((capture_path != NULL) != (capture_frame != 0)) {
        fprintf(stderr, "[GLES2] specify both --capture FILE.ppm and --capture-frame N\n"); return false;
    }
    if((audit_path || eng_audit_no_draw || eng_audit_flat_fragment || eng_audit_divisor!=1) && !eng_audit_available()) {
        fprintf(stderr,"[AUDIT] diagnostics require RR_PIPELINE_AUDIT=ON\n");return false;
    }
    if(audit_path && !profile_stages) {fprintf(stderr,"[AUDIT] --audit-csv requires --profile-stages\n");return false;}
    if(audit_gpu && !audit_path) {fprintf(stderr,"[AUDIT] GPU timers require --audit-csv\n");return false;}
    if(eng_audit_detailed && !audit_path) {fprintf(stderr,"[AUDIT] detailed timing requires --audit-csv\n");return false;}
    if((audit_path || eng_audit_no_draw || eng_audit_divisor!=1) && strcmp(eng_gles2_vbo_mode_name(),"frame")) {
        fprintf(stderr,"[AUDIT] pipeline diagnostics require frame VBO mode\n");return false;
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
    if(audit_path || eng_audit_no_draw || eng_audit_divisor!=1) {
        fprintf(stderr,"[AUDIT] no_draw=%d viewport_divisor=%d; draw counters are logical when suppressed\n",eng_audit_no_draw,eng_audit_divisor);
        fprintf(stderr,"[AUDIT-EXTENSIONS] %s\n",glGetString(GL_EXTENSIONS));
    }
    if(audit_path) {
        fprintf(stderr,"[AUDIT] CPU attribution=%s; coarse mode includes draw in submit and lookup in backend; existing PROFILE wall breakdown remains available\n",eng_audit_detailed?"detailed":"coarse");
        audit_file=fopen(audit_path,"w");
        if(!audit_file) {perror("audit CSV");return false;}
        setvbuf(audit_file,NULL,_IOFBF,65536);
        fputs("frame,rendered,quads,triangles,draws,vbo_uploads,vbo_bytes,hits,misses,evictions,texture_uploads,binds,time_s",audit_file);
        const char *names[AUD_COUNT]={"sim","scene","backend","lookup","upload","submit","vertex","draw","swap","wait","decode"};
        for(int i=0;i<AUD_COUNT;i++) fprintf(audit_file,",%s_wall,%s_cpu,%s_inclusive_wall,%s_inclusive_cpu",names[i],names[i],names[i],names[i]);
        const char *stages[RR_STAGE_COUNT]={"dsp","sound","c352","serial","io","video","palette","direct","list","sort"};
        for(int i=0;i<RR_STAGE_COUNT;i++) fprintf(audit_file,",%s_stage_wall,%s_stage_cpu",stages[i],stages[i]);
        fputs(",sort_keys_wall,sort_keys_cpu,sort_permutation_wall,sort_permutation_cpu,sort_moved_bytes,sort_allocations",audit_file);
        fputc('\n',audit_file);eng_audit_enabled=true;
    }
    if(!gpu_open()) return false;
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

/* Choose before geometry only for the opt-in path. The original clock/catch-up
 * algorithm is unchanged; baseline retains its post-preparation decision. */
static bool choose_presentation(uint32_t frame)
{
    if (!pump_input()) return false;
    frame_render=true;
    if (independent) {
        rr_clock_begin(&frame_clock,seconds());
        while (seconds()<frame_clock.deadline) {
            SDL_Delay(1);
            if (!pump_input()) return false;
        }
        frame_render=rr_clock_render(&frame_clock,seconds(),render_hz,capture_frame==frame);
        /* Correctness harness only: identical presented ticks for A/B captures.
         * Never use this override for performance measurements. */
        if (test_present_every) frame_render=frame%test_present_every==0 || capture_frame==frame;
    }
    return true;
}

bool rr_gles2_frame(uint32_t frame, const geo_quad *quads, int count, const float bg[3])
{
    eng_audit_end(audit_scene);
    double scene_elapsed = seconds() - prepare_start;
    rr_sim_enter(SIM_PRESENT);
    double host_wait_start=rr_gles2_stage_begin();
    if (!(independent && (skip_geometry || test_present_every)) && !choose_presentation(frame)) return false;
    bool render=frame_render;
    if (!timing_steps) timing_start=timing_window=seconds();
    timing_steps++; timing_steps_window++; timing_sim+=simulation_elapsed; timing_scene+=scene_elapsed;
    double host_wait_elapsed=profile_stages ? early_host_wait+seconds()-host_wait_start : 0;
    if (!render) {
        const eng_gles2_stats empty={0};
        audit_row(frame,false,(unsigned)count,&empty);
        stages_add(frame,false,(unsigned)count,&empty,scene_elapsed,0,0,host_wait_elapsed,0);
        timing_skipped++;
        if (seconds()-timing_window>=5) timing_report("window");
        simulation_start=seconds();
        return true;
    }
    timing_renders++; timing_renders_window++;
    eng_gles2_stats stats;
    double draw_start = seconds();
    gpu_begin_frame(frame);
    eng_audit_scope audit_backend=eng_audit_begin(AUD_BACKEND);
    if (!eng_gles2_draw(quads, count, bg, &stats)) { failed = true; return false; }
    eng_audit_end(audit_backend);
    gpu_end_frame();
    double draw_elapsed = seconds() - draw_start;
    if (!saw_geometry && stats.triangles) {
        fprintf(stderr, "[GLES2] first geometry frame=%u quads=%d triangles=%u draws=%u\n", frame, count, stats.triangles, stats.draws);
        saw_geometry = true;
    }
    double capture_start = seconds();
    if (capture_frame == frame && !capture(frame, &stats)) { failed = true; return false; }
    double capture_elapsed = seconds() - capture_start;
    double swap_start = seconds();
    eng_audit_scope audit_swap=eng_audit_begin(AUD_SWAP);
    SDL_GL_SwapWindow(window);
    eng_audit_end(audit_swap);
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
    audit_row(frame,true,(unsigned)count,&stats);
    stages_add(frame,true,(unsigned)count,&stats,scene_elapsed,draw_elapsed,swap_elapsed,
               host_wait_elapsed+(profile_stages ? seconds()-pacing_start : 0),capture_elapsed);
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
/* Linux process peak survives between sampler polls; log before freeing GPU
 * cache storage. -1 means unavailable, not zero memory use. */
static void report_memory(void)
{
    long rss=-1, peak=-1, available=-1, cma=-1; char line[256];
    FILE *f=fopen("/proc/self/status","r");
    if(f) {
        while(fgets(line,sizeof line,f)) {
            if(!strncmp(line,"VmRSS:",6)) sscanf(line+6,"%ld",&rss);
            if(!strncmp(line,"VmHWM:",6)) sscanf(line+6,"%ld",&peak);
        }
        fclose(f);
    }
    f=fopen("/proc/meminfo","r");
    if(f) {
        while(fgets(line,sizeof line,f)) {
            if(!strncmp(line,"MemAvailable:",13)) sscanf(line+13,"%ld",&available);
            if(!strncmp(line,"CmaFree:",8)) sscanf(line+8,"%ld",&cma);
        }
        fclose(f);
    }
    fprintf(stderr,"[MEMORY] RSS_kib=%ld RSS_peak_kib=%ld MemAvailable_kib=%ld CmaFree_kib=%ld\n",rss,peak,available,cma);
}
/* Read-only, byte-defined board snapshot at exit; no register callbacks.
 * Complements the existing RAM/CPU JSON and ordered video-tick digest. */
static void report_board_state(void)
{
    const uint32_t fields[]={g_hw.irq_enabled,g_hw.irq_state,g_hw.mcu_run,g_hw.dsp_ctrl,
        g_hw.dsw,g_hw.cpuleds,g_hw.portbits[0],g_hw.portbits[1],g_hw.keycus_rng,g_hw.lcg,
        g_hw.inputs,g_hw.steer,g_hw.gas,g_hw.brake,g_hw.adc[0],g_hw.adc[1],g_hw.adc[2],g_hw.adc[3],
        (uint32_t)g_hw.old_coin,(uint32_t)g_hw.credits1,(uint32_t)g_hw.credits2};
    uint32_t h=2166136261u;
    for(unsigned i=0;i<sizeof fields/sizeof fields[0];i++)
        for(int b=24;b>=0;b-=8) h=(h^(uint8_t)(fields[i]>>b))*16777619u;
    for(unsigned i=0;i<sizeof g_rr.syscon;i++) h=(h^g_rr.syscon[i])*16777619u;
    for(unsigned i=0;i<sizeof g_rr.sci_reg;i++) h=(h^g_rr.sci_reg[i])*16777619u;
    for(unsigned i=0;i<sizeof g_rr.sci;i++) h=(h^g_rr.sci[i])*16777619u;
    fprintf(stderr,"[BOARD-STATE] fnv1a32=%08x\n",h);
}
void rr_gles2_close(void)
{
    if (!window) return;
    gpu_close();
    if(audit_file) {
        eng_audit_pmu_report();
        if(fclose(audit_file)) {perror("audit CSV close");failed=true;}
        audit_file=NULL;eng_audit_enabled=false;
    }
    report_board_state();
    rr_memory_profile_report();
    rr_sim_report();
    if(profile_memory) report_memory();
    stages_report("window-final",stage_window,stage_window_start);
    stages_report("total",stage_total,stage_start);
    timing_report("total");
    fprintf(stderr,"[GEOMETRY] skip=%s effective=%s test_present_every=%u ticks=%u walks=%u direct_consumed=%u digest=%016llx\n",
        skip_geometry?"on":"off",independent&&skip_geometry?"on":"off",test_present_every,
        scene_ticks,scene_walks,scene_direct,(unsigned long long)scene_digest);
    if (range_clock_started)
        fprintf(stderr,"[GEOMETRY-RATE] from=%u resyncs=%u discarded_lag_ms=%.3f\n",
            profile_from,frame_clock.resyncs-range_resync_start,(frame_clock.discarded-range_discard_start)*1000);
    profile_report("window-final", &window_profile);
    profile_report("total", &total_profile);
    if (capture_frame && !captured) { fprintf(stderr, "[GLES2] requested geometry capture was not completed\n"); failed = true; }
    for (int i = 0; i < 16; i++) if (controllers[i]) { SDL_GameControllerClose(controllers[i]); controllers[i] = NULL; }
    for (int i=0;i<16;i++) if (raw_joysticks[i]) { SDL_JoystickClose(raw_joysticks[i]); raw_joysticks[i]=NULL; }
    if (context) { eng_gles2_close(); SDL_GL_DeleteContext(context); context = NULL; }
    SDL_DestroyWindow(window); window = NULL;
    SDL_Quit();
}
