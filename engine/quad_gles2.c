/* Original-ROM GLES backend with an optional compatible-dimension array atlas.
 * No fixed-function calls; the presentation layer owns the render target.
 * Sorting, screen clipping and fan indices are shared with quad_gl.c.
 * geo_hw has already applied the game's fixed-point view/object projection. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <time.h>
#include "eng_gl.h"
#ifdef RR_GLES_WORLD_ATLAS
#include <GLES3/gl3.h>
#endif
#include "quad_gl.h"
#include "quad_gles2.h"
#include "tex_bake.h"
#include "eng.h"
#include "eng_audit.h"

float g_scene_x0 = 0, g_scene_x1 = ENG_SCREEN_W;
#include "quad_geometry.inc"
#include "gles_submission_profile.inc"

#define MAX_VERTICES 60000
typedef struct { float position[3], colour[4], uvq[3];
#ifdef RR_GLES_WORLD_ATLAS
    float layer;
#endif
#ifdef RR_GLES2_MULTITEXTURE_BATCH
    float texture_slot;
#endif
} vertex;
static vertex batch[MAX_VERTICES];
/* Ordered commands share the existing CPU arena; no geometry is reordered. */
#define MAX_COMMANDS 4096
typedef struct { GLuint texture; int first, count, scissor[4];
#ifdef RR_GLES_WORLD_ATLAS
    GLuint reference;int array,layer,sensitive;
#endif
} draw_command;
#ifdef RR_GLES_WORLD_ATLAS
static int world_mode,batch_array,batch_layer;
static GLuint batch_reference;
#endif
bool eng_gles2_world_atlas_mode(const char *name)
{
#ifdef RR_GLES_WORLD_ATLAS
    if(!strcmp(name,"off"))world_mode=0;else if(!strcmp(name,"ordered"))world_mode=1;else if(!strcmp(name,"group"))world_mode=2;else return false;return true;
#else
    return !strcmp(name,"off");
#endif
}
static draw_command commands[MAX_COMMANDS];
static int used, batch_first, command_count, batch_scissor[4];
static bool frame_upload = true;
bool eng_gles2_vbo_mode(const char *name)
{
    if (strcmp(name, "frame") && strcmp(name, "legacy")) return false;
    frame_upload = !strcmp(name, "frame"); return true;
}
const char *eng_gles2_vbo_mode_name(void) { return frame_upload ? "frame" : "legacy"; }
static GLuint program, vbo, white;
static GLint projection, sampler, priority_uniform;
#ifdef RR_GLES2_MULTITEXTURE_BATCH
enum { MULTI_UNITS = 4 };
enum { MULTI_OFF, MULTI_SHADER_PROBE, MULTI_BATCH };
static int multi_mode;
static GLint multi_samplers[MULTI_UNITS];
typedef struct {
    int first, count, scissor[4];
    GLuint textures[MULTI_UNITS];
    unsigned texture_count;
} multi_group;
static multi_group multi_groups[MAX_COMMANDS];
static int multi_group_count;
#endif
#include "gles_multi_gpu_trace.inc"
bool eng_gles2_multitexture_mode(const char *name)
{
#ifdef RR_GLES2_MULTITEXTURE_BATCH
    if (!strcmp(name, "off")) multi_mode = MULTI_OFF;
    else if (!strcmp(name, "shader")) multi_mode = MULTI_SHADER_PROBE;
    else if (!strcmp(name, "batch")) multi_mode = MULTI_BATCH;
    else return false;
    return true;
#else
    return !strcmp(name, "off");
#endif
}
bool eng_gles2_multi_gpu_trace(const char *path)
{
#ifdef RR_GLES2_MULTITEXTURE_BATCH
    multi_gpu_path = path;
    return true;
#else
    (void)path;
    return false;
#endif
}
static bool priority_enabled;
void eng_gles2_priority(bool enabled) {priority_enabled=enabled;}
static GLuint batch_texture;
static eng_gles2_stats *current_stats;
static bool diagnostic, flush_failed;
static bool white_batches;
static const char *mode_name = "textured";
#include "gles_multi_validation.inc"
#ifdef RR_GLES_SUBMISSION_PROFILE
/* Diagnostic upper bounds: count contiguous draws that could share a shader
 * with N texture units. Scissor and submission/mutation barriers still split
 * groups. No drawing or texture ownership is changed. */
static FILE *batch_trace;
static struct {
    unsigned chunks, commands, scissor_breaks, texture_breaks;
    unsigned groups[6], largest_group;
} batch_bounds;
static const unsigned batch_unit_caps[6] = {1, 2, 4, 8, 16, 4096};
static void batch_trace_chunk(void)
{
    if (!batch_trace || !command_count) return;
    batch_bounds.chunks++;
    batch_bounds.commands += (unsigned)command_count;
    for (unsigned mode = 0; mode < 6; mode++) {
        GLuint resident[16];
        unsigned count = 0, length = 0;
        int scissor[4] = {-1,-1,-1,-1};
        for (int i = 0; i < command_count; i++) {
            const draw_command *c = &commands[i];
            bool changed = i && memcmp(scissor, c->scissor, sizeof scissor);
            if (mode == 0 && i) {
                batch_bounds.scissor_breaks += changed;
                batch_bounds.texture_breaks += commands[i-1].texture != c->texture;
            }
            bool found = false;
            for (unsigned j = 0; j < count; j++)
                if (resident[j] == c->texture) { found = true; break; }
            if (!i || changed || (!found && count == batch_unit_caps[mode])) {
                if (length > batch_bounds.largest_group) batch_bounds.largest_group = length;
                batch_bounds.groups[mode]++;
                count = length = 0;
                found = false;
            }
            if (!found && count < 16) resident[count++] = c->texture;
            /* The unlimited mode keeps no set: it only splits on scissor. */
            length++;
            memcpy(scissor, c->scissor, sizeof scissor);
        }
        if (length > batch_bounds.largest_group) batch_bounds.largest_group = length;
    }
}
#endif
static double seconds(void)
{
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}
static size_t texture_budget = (size_t)RR_GLES2_TEXTURE_BUDGET_KB * 1024;
enum flush_reason { TEXTURE, SCISSOR, CAPACITY, MUTATION, END };
static bool flush(eng_gles2_stats *stats, enum flush_reason reason);
static bool submit_commands(eng_gles2_stats *stats);
const char *eng_gles2_mode_name(void) { return mode_name; }
bool eng_gles2_mode(const char *name)
{
    if (strcmp(name, "textured") && strcmp(name, "diagnostic") &&
        strcmp(name, "white-batches") && strcmp(name, "textured-no-rebind")) return false;
    diagnostic = !strcmp(name, "diagnostic");
    white_batches = !strcmp(name, "white-batches");
    tex_bake_gles2_binding_mode(!strcmp(name, "textured-no-rebind"));
    mode_name = diagnostic ? "diagnostic" : white_batches ? "white-batches" :
        !strcmp(name, "textured-no-rebind") ? "textured-no-rebind" : "textured";
    return true;
}
void eng_gles2_diagnostic(bool enabled) { eng_gles2_mode(enabled ? "diagnostic" : "textured"); }
bool eng_gles2_texture_budget(unsigned mib)
{
    if (!mib || mib > 128 || (size_t)mib * 1024 > RR_GLES2_TEXTURE_MAX_BUDGET_KB) return false;
    texture_budget = (size_t)mib * 1024 * 1024;
    return true;
}
static void before_texture_change(GLuint texture)
{
    if (used && texture == batch_texture && !flush(current_stats, MUTATION)) flush_failed = true;
    /* Cache updates/deletions must not change any queued draw's texels. Drain
     * the ordered prefix before mutation, retaining a different pending batch. */
    if (frame_upload) for (int i = 0; i < command_count; i++) {
        if (commands[i].texture == texture
#ifdef RR_GLES_WORLD_ATLAS
            || (world_mode && commands[i].reference==texture)
#endif
        ) {
            current_stats->texture_barriers++;
            if (!submit_commands(current_stats)) flush_failed = true;
            break;
        }
    }
}

static bool check_gl(const char *stage)
{
    SP_BEGIN(sp_query,5,1);
    GLenum error = glGetError();
    SP_END(sp_query);
    if (error == GL_NO_ERROR) return true;
    fprintf(stderr, "[GLES2] %s failed: GL error 0x%x\n", stage, error);
    return false;
}

static GLuint compile(GLenum type, const char *source)
{
    GLuint shader = glCreateShader(type);
    if (!shader) { check_gl("create shader"); return 0; }
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint ok, length;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
    if (length > 1) {
        char *log = malloc((size_t)length);
        if (log) {
            glGetShaderInfoLog(shader, length, NULL, log);
            fprintf(stderr, "[GLES2] %s shader: %s\n",
                    type == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
            free(log);
        }
    }
    if (!ok) { fprintf(stderr, "[GLES2] shader compilation failed\n"); glDeleteShader(shader); return 0; }
    return shader;
}

#ifdef RR_GLES_WORLD_ATLAS
#include "gles_world_array_submit.inc"
#endif

bool eng_gles2_init(void)
{
#ifdef RR_GLES2_MULTITEXTURE_BATCH
    if (multi_mode == MULTI_BATCH && !frame_upload) {
        fprintf(stderr, "[GLES2] multi-texture batches require --vbo-upload frame\n");
        return false;
    }
    fprintf(stderr, "[GLES2] four-texture experiment=%s\n",
            multi_mode == MULTI_BATCH ? "batch" : multi_mode == MULTI_SHADER_PROBE ? "shader" : "off");
#endif
#ifdef RR_GLES_SUBMISSION_PROFILE
    const char *trace_path = getenv("GLES_BATCH_TRACE");
    if (trace_path) {
        batch_trace = fopen(trace_path, "wx");
        if (!batch_trace) { perror("GLES_BATCH_TRACE"); return false; }
        fputs("frame,draws,triangles,chunks,scissor_breaks,texture_breaks,groups_1,groups_2,groups_4,groups_8,groups_16,groups_unlimited,largest_group,mutation_barriers,flush_scissor,flush_capacity,empty_texture_flush\n", batch_trace);
    }
#endif
    static const char *vs =
        "#version 100\n"
        "attribute vec3 a_position;\n"
        "attribute vec4 a_colour;\n"
        "attribute vec3 a_uvq;\n"
        "uniform mat4 u_projection;\n"
        "varying mediump vec4 v_colour;\n"
        "varying highp vec3 v_uvq;\n"
        "void main() { gl_Position = u_projection * vec4(a_position, 1.0);"
        " v_colour = a_colour; v_uvq = a_uvq; }\n";
#ifdef RR_GLES2_MULTITEXTURE_BATCH
    static const char *vs_multi =
        "#version 100\n"
        "attribute vec3 a_position; attribute vec4 a_colour; attribute vec3 a_uvq;"
        "attribute float a_texture_slot; uniform mat4 u_projection;"
        "varying mediump vec4 v_colour; varying highp vec3 v_uvq;"
        "varying mediump float v_texture_slot;"
        "void main() { gl_Position = u_projection * vec4(a_position, 1.0);"
        " v_colour = a_colour; v_uvq = a_uvq; v_texture_slot = a_texture_slot; }\n";
    static const char *fs_multi_priority = "#version 100\nprecision highp float;uniform sampler2D u_tex0;uniform sampler2D u_tex1;uniform sampler2D u_tex2;uniform sampler2D u_tex3;uniform bool u_priority;varying mediump vec4 v_colour;varying highp vec3 v_uvq;varying mediump float v_texture_slot;void main(){vec2 uv=v_uvq.xy/v_uvq.z;vec4 texel;if(v_texture_slot<0.5)texel=texture2D(u_tex0,uv);else if(v_texture_slot<1.5)texel=texture2D(u_tex1,uv);else if(v_texture_slot<2.5)texel=texture2D(u_tex2,uv);else texel=texture2D(u_tex3,uv);if(texel.a==0.0)discard;vec4 c=v_colour*texel;gl_FragColor=u_priority?vec4(c.rgb,v_colour.a):c;}";
    static const char *fs_multi_legacy = "#version 100\nprecision highp float;uniform sampler2D u_tex0;uniform sampler2D u_tex1;uniform sampler2D u_tex2;uniform sampler2D u_tex3;varying mediump vec4 v_colour;varying highp vec3 v_uvq;varying mediump float v_texture_slot;void main(){vec2 uv=v_uvq.xy/v_uvq.z;vec4 texel;if(v_texture_slot<0.5)texel=texture2D(u_tex0,uv);else if(v_texture_slot<1.5)texel=texture2D(u_tex1,uv);else if(v_texture_slot<2.5)texel=texture2D(u_tex2,uv);else texel=texture2D(u_tex3,uv);if(texel.a==0.0)discard;gl_FragColor=v_colour*texel;}";
#endif
    static const char *fs_priority =
        "#version 100\nprecision highp float;\n"
        "uniform sampler2D u_texture;uniform bool u_priority;\n"
        "varying mediump vec4 v_colour;\n"
        "varying highp vec3 v_uvq;\n"
        "void main() { vec2 uv = v_uvq.xy / v_uvq.z;"
        " vec4 texel = texture2D(u_texture, uv);"
        " if (texel.a == 0.0) discard; vec4 c=v_colour*texel; gl_FragColor=u_priority?vec4(c.rgb,v_colour.a):c; }\n";
    /* Keep the exact original shader in 3D-only reference mode. Mali's compiler
     * rounds some colour products differently when a priority branch is added. */
    static const char *fs_legacy =
        "#version 100\nprecision highp float;\n"
        "uniform sampler2D u_texture;\n"
        "varying mediump vec4 v_colour;\n"
        "varying highp vec3 v_uvq;\n"
        "void main() { vec2 uv = v_uvq.xy / v_uvq.z;"
        " vec4 texel = texture2D(u_texture, uv);"
        " if (texel.a == 0.0) discard; gl_FragColor = v_colour * texel; }\n";
    const char *fs=priority_enabled?fs_priority:fs_legacy;
#ifdef RR_GLES2_MULTITEXTURE_BATCH
    mv_vs=vs;mv_fs=fs;
#endif
#ifdef RR_GLES2_MULTITEXTURE_BATCH
    if (multi_mode) { vs = vs_multi; fs = priority_enabled ? fs_multi_priority : fs_multi_legacy; }
#endif
    GLint max_units=0;glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS,&max_units);
#ifdef RR_GLES2_MULTITEXTURE_BATCH
    if(multi_mode && max_units<MULTI_UNITS){fprintf(stderr,"[GLES2] four texture units required\n");return false;}
#endif
    GLint max_texture, precision, range[2];
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture);
    glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT, range, &precision);
    fprintf(stderr, "[TEX] GL max texture=%d fragment highp bits=%d\n", max_texture, precision);
#ifdef RR_GLES_SUBMISSION_PROFILE
    GLint texture_units = 0, fragment_uniforms = 0, varyings = 0, attributes = 0;
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &texture_units);
    glGetIntegerv(GL_MAX_FRAGMENT_UNIFORM_VECTORS, &fragment_uniforms);
    glGetIntegerv(GL_MAX_VARYING_VECTORS, &varyings);
    glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &attributes);
    fprintf(stderr, "[BATCH-LIMITS] texture_units=%d fragment_uniform_vectors=%d varying_vectors=%d vertex_attributes=%d max_texture=%d highp_bits=%d\n",
            texture_units, fragment_uniforms, varyings, attributes, max_texture, precision);
#endif
    if (max_texture < 1024 || precision == 0) {
        fprintf(stderr, "[TEX] unsupported: requires 1024 texture size and fragment highp for original perspective UVs\n");
        return false;
    }
    GLuint v = compile(GL_VERTEX_SHADER, vs);
    static const char *flat_fs="#version 100\nprecision mediump float;\nvarying mediump vec4 v_colour;\nvoid main(){gl_FragColor=v_colour;}\n";
    GLuint f = compile(GL_FRAGMENT_SHADER, AUDIT_FLAT?flat_fs:fs);
    if (!v || !f) { if (v) glDeleteShader(v); if (f) glDeleteShader(f); return false; }
    program = glCreateProgram();
    glAttachShader(program, v); glAttachShader(program, f);
    glBindAttribLocation(program, 0, "a_position");
    glBindAttribLocation(program, 1, "a_colour");
    glBindAttribLocation(program, 2, "a_uvq");
#ifdef RR_GLES2_MULTITEXTURE_BATCH
    if (multi_mode) glBindAttribLocation(program, 3, "a_texture_slot");
#endif
    glLinkProgram(program);
    glDeleteShader(v); glDeleteShader(f);
    GLint ok, length;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
    if (length > 1) {
        char *log = malloc((size_t)length);
        if (log) { glGetProgramInfoLog(program, length, NULL, log); fprintf(stderr, "[GLES2] link: %s\n", log); free(log); }
    }
    if (!ok) { fprintf(stderr, "[GLES2] program link failed\n"); return false; }
    projection = glGetUniformLocation(program, "u_projection");
    sampler = glGetUniformLocation(program, "u_texture");
    priority_uniform=glGetUniformLocation(program,"u_priority");
#ifdef RR_GLES2_MULTITEXTURE_BATCH
    if (multi_mode) for (int i = 0; i < MULTI_UNITS; i++) {
        char name[16]; snprintf(name, sizeof name, "u_tex%d", i);
        multi_samplers[i] = glGetUniformLocation(program, name);
        if (multi_samplers[i] < 0) { fprintf(stderr, "[GLES2] missing multi-texture sampler %d\n", i); return false; }
    }
#endif
    if (projection < 0 ||
#ifdef RR_GLES2_MULTITEXTURE_BATCH
        (!multi_mode &&
#endif
        (sampler < 0 && !AUDIT_FLAT)
#ifdef RR_GLES2_MULTITEXTURE_BATCH
        )
#endif
        ) { fprintf(stderr, "[GLES2] missing shader uniforms\n"); return false; }
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    if (!frame_upload) glBufferData(GL_ARRAY_BUFFER, sizeof batch, NULL, GL_DYNAMIC_DRAW);
    if (!check_gl("allocate triangle VBO")) return false;
    fprintf(stderr, "[GLES2] triangle storage: CPU=%zu bytes, GPU store upper bound=%zu bytes\n", sizeof batch, sizeof batch);
    fprintf(stderr, "[GLES2] VBO mode=%s command storage=%zu bytes; frame mode re-specifies storage per contiguous chunk\n",
            eng_gles2_vbo_mode_name(), sizeof commands);
    const GLubyte pixel[4] = {255, 255, 255, 255};
    if (sizeof pixel > texture_budget) { fprintf(stderr, "[GLES2] texture budget exhausted\n"); return false; }
    glGenTextures(1, &white);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    if (!check_gl("allocate white texture")) return false;
    tex_cache_budget = texture_budget - 4; /* white texel is also charged */
    renderer_texture_init();
    tex_bake_set_budget(0, 0); /* no deferred/coarse placeholders */
    tex_bake_set_flush_hook(before_texture_change);
#ifdef RR_GLES_WORLD_ATLAS
    if(!world_open())return false;
#endif
    fprintf(stderr, "[GLES2] GLSL ES 1.00 linked; textures=%s budget=%zu; per-texture cache, atlas disabled\n",
            mode_name, texture_budget);
    fprintf(stderr, "[GLES2] unsupported/bypassed: polygon fog, Nuklear, desktop blits/render targets\n");
#ifdef RR_GLES2_MULTITEXTURE_BATCH
    if (!multi_gpu_open() || !mv_open()) return false;
#endif
    return true;
}

#ifdef RR_GLES2_MULTITEXTURE_BATCH
static bool multi_prepare_groups(void)
{
    multi_group_count = 0;
    for (int i = 0; i < command_count; i++) {
        const draw_command *c = &commands[i];
        multi_group *g = multi_group_count ? &multi_groups[multi_group_count-1] : NULL;
        unsigned slot = 0;
        if (g && !memcmp(g->scissor, c->scissor, sizeof g->scissor) &&
            g->first + g->count == c->first) {
            while (slot < g->texture_count && g->textures[slot] != c->texture) slot++;
            if (slot == MULTI_UNITS) g = NULL;
        } else g = NULL;
        if (!g) {
            if (multi_group_count >= MAX_COMMANDS) return false;
            g = &multi_groups[multi_group_count++];
            g->first = c->first; g->count = 0; g->texture_count = 0;
            memcpy(g->scissor, c->scissor, sizeof g->scissor);
            slot = 0;
        }
        if (slot == g->texture_count) g->textures[g->texture_count++] = c->texture;
        for (int v = c->first; v < c->first + c->count; v++) batch[v].texture_slot = (float)slot;
        g->count += c->count;
    }
    return true;
}
#endif
static bool submit_commands(eng_gles2_stats *stats)
{
#ifdef RR_GLES_WORLD_ATLAS
    if(world_mode)return world_submit(stats);
#endif
    if (!command_count) return true;
#ifdef RR_GLES_SUBMISSION_PROFILE
    batch_trace_chunk();
#endif
    SP_FULL_BEGIN();
    eng_audit_scope audit_submit=eng_audit_begin(AUD_SUBMIT);
    double start = seconds();
#ifdef RR_GLES2_MULTITEXTURE_BATCH
    if (multi_mode == MULTI_BATCH) {
        SP_BEGIN(sp_group,6,1);
        if (!multi_prepare_groups()) return false;
        SP_END(sp_group);
    }
#endif
    double vertex_start = seconds();
    size_t bytes = (size_t)batch_first * sizeof(vertex);
    /* GLES2 glBufferData re-specifies the store rather than overwriting the
     * in-flight offset-zero range. Driver storage renaming is implementation
     * dependent; retain legacy mode for measurement on Mali. */
    eng_audit_scope audit_vertex=eng_audit_begin(AUD_VERTEX);
    SP_BEGIN(sp_vertex,4,1);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)bytes, batch, GL_STREAM_DRAW);
    SP_END(sp_vertex);
    eng_audit_end(audit_vertex);
    stats->vertex_upload_seconds += seconds() - vertex_start;
    stats->vertex_uploads++; stats->vertex_upload_bytes += bytes;
    if (!check_gl("upload vertex chunk")) return false;
#ifdef RR_ORDERED_SUBMISSION
    /* Geometry construction and texture barriers can change GL state. Establish
     * ownership afresh for each submitted chunk, never cache across chunks. */
    int submitted_scissor[4] = {-1,-1,-1,-1};
    glEnable(GL_SCISSOR_TEST);
#endif
#ifdef RR_GLES2_MULTITEXTURE_BATCH
    if (multi_mode == MULTI_BATCH) {
        for (int i = 0; i < multi_group_count; i++) {
            const multi_group *g = &multi_groups[i];
            int sampled=SP_SAMPLE;
            SP_BEGIN(sp_state,2,sampled);
            if (memcmp(submitted_scissor, g->scissor, sizeof submitted_scissor)) {
                glScissor(g->scissor[0]/AUDIT_DIVISOR, g->scissor[1]/AUDIT_DIVISOR,
                    (g->scissor[2]+AUDIT_DIVISOR-1)/AUDIT_DIVISOR,
                    (g->scissor[3]+AUDIT_DIVISOR-1)/AUDIT_DIVISOR);
                memcpy(submitted_scissor, g->scissor, sizeof submitted_scissor);
            }
            for (unsigned slot = 0; slot < g->texture_count; slot++) {
                glActiveTexture(GL_TEXTURE0 + slot);
                tex_bake_gles2_binding_reset();
                tex_bake_gles2_bind(GL_TEXTURE_2D, white_batches ? white : g->textures[slot]);
            }
            glActiveTexture(GL_TEXTURE0);
            tex_bake_gles2_binding_reset();
            SP_END(sp_state);
            double call_start = seconds();
            eng_audit_scope audit_draw=eng_audit_begin(AUD_DRAW);
            SP_BEGIN(sp_draw,3,sampled);
            if (!AUDIT_NO_DRAW) glDrawArrays(GL_TRIANGLES, g->first, g->count);
            SP_END(sp_draw);
            eng_audit_end(audit_draw);
            stats->draw_call_seconds += seconds() - call_start;
            stats->triangles += (unsigned)g->count / 3; stats->draws++;
        }
    } else
#endif
    for (int i = 0; i < command_count; i++) {
        const draw_command *c = &commands[i];
        int sampled=SP_SAMPLE;
        SP_BEGIN(sp_state,2,sampled);
#ifdef RR_ORDERED_SUBMISSION
        if (memcmp(submitted_scissor,c->scissor,sizeof submitted_scissor)) {
#else
        glEnable(GL_SCISSOR_TEST);
#endif
        glScissor(c->scissor[0]/AUDIT_DIVISOR, c->scissor[1]/AUDIT_DIVISOR,
            (c->scissor[2]+AUDIT_DIVISOR-1)/AUDIT_DIVISOR,(c->scissor[3]+AUDIT_DIVISOR-1)/AUDIT_DIVISOR);
#ifdef RR_ORDERED_SUBMISSION
            memcpy(submitted_scissor,c->scissor,sizeof submitted_scissor);
        }
#endif
        tex_bake_gles2_bind(GL_TEXTURE_2D, white_batches ? white : c->texture);
        SP_END(sp_state);
        double call_start = seconds();
        eng_audit_scope audit_draw=eng_audit_begin(AUD_DRAW);
        SP_BEGIN(sp_draw,3,sampled);
        if(!AUDIT_NO_DRAW) glDrawArrays(GL_TRIANGLES, c->first, c->count);
        SP_END(sp_draw);
        eng_audit_end(audit_draw);
        stats->draw_call_seconds += seconds() - call_start;
        stats->triangles += (unsigned)c->count / 3; stats->draws++;
    }
    if (!mv_chunk()) return false;
    bool ok = check_gl("submit vertex chunk");
    /* A cache barrier may drain earlier commands while another texture's
     * pending vertices remain. Retain those vertices at the arena start. */
    if (used) memmove(batch, batch + batch_first, (size_t)used * sizeof(vertex));
    batch_first = command_count = 0;
    stats->draw_seconds += seconds() - start;
    eng_audit_end(audit_submit);
    SP_FULL_END();
    return ok;
}

static bool flush(eng_gles2_stats *stats, enum flush_reason reason)
{
    if (!used) return true;
    if (frame_upload && command_count == MAX_COMMANDS && !submit_commands(stats)) return false;
    double start = seconds();
    bool ok = true;
    if (frame_upload) {
#ifdef RR_ORDERED_SUBMISSION
        draw_command *last = command_count ? &commands[command_count-1] : NULL;
        /* Only adjoining vertex ranges with identical effective state. Never
         * cross a cache-mutation drain or change primitive/depth ordering. */
        if (last && last->texture == batch_texture &&
#ifdef RR_GLES_WORLD_ATLAS
            (!world_mode || (last->array==batch_array && last->layer==batch_layer && last->reference==batch_reference)) &&
#endif
            last->first + last->count == batch_first &&
            !memcmp(last->scissor,batch_scissor,sizeof batch_scissor)) {
            last->count += used;
        } else {
#endif
        draw_command *c = &commands[command_count++];
        c->texture = batch_texture; c->first = batch_first; c->count = used;
#ifdef RR_GLES_WORLD_ATLAS
        c->array=batch_array;c->layer=batch_layer;c->reference=batch_reference;
#endif
        memcpy(c->scissor, batch_scissor, sizeof batch_scissor);
#ifdef RR_ORDERED_SUBMISSION
        }
#endif
        batch_first += used;
    } else {
        tex_bake_gles2_bind(GL_TEXTURE_2D, white_batches ? white : batch_texture);
        double vertex_start = seconds();
        size_t bytes = (size_t)used * sizeof(vertex);
        glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)bytes, batch);
        double call_start = seconds();
        stats->vertex_upload_seconds += call_start - vertex_start;
        stats->vertex_uploads++; stats->vertex_upload_bytes += bytes;
        glDrawArrays(GL_TRIANGLES, 0, used);
        stats->draw_call_seconds += seconds() - call_start;
        stats->triangles += (unsigned)used / 3; stats->draws++;
        ok = check_gl("submit triangles");
    }
    switch (reason) {
        case TEXTURE: stats->flush_texture++; break;
        case SCISSOR: stats->flush_scissor++; break;
        case CAPACITY: stats->flush_capacity++; break;
        case MUTATION: stats->flush_mutation++; break;
        case END: stats->flush_end++; break;
    }
    used = 0;
    double elapsed = seconds() - start;
    stats->draw_seconds += elapsed;
    if (frame_upload) { SP_PREP(elapsed); }
    return ok;
}

#ifdef RR_PARALLEL_FRAME_PREP
bool eng_gles2_draw_ordered(const geo_quad *quads, const int *order, int count,
                            const float bg[3], eng_gles2_stats *stats)
#else
bool eng_gles2_draw(const geo_quad *quads, int count, const float bg[3], eng_gles2_stats *stats)
#endif
{
    SP_FULL_FRAME_BEGIN();
    memset(stats, 0, sizeof *stats);
    int hits_before = tex_frame_hits, misses_before = tex_frame_misses;
    int evictions_before = tex_cache_evictions, uploads_count_before = tex_reallocs + tex_subimages;
    double uploads_before = tex_gles2_upload_seconds;
    double decode_before = tex_gles2_decode_seconds, texels_before = g_bake_texels;
    double eviction_before = tex_gles2_eviction_seconds;
    unsigned long long probes_before = tex_gles2_clock_probes, deletes_before = tex_gles2_delete_calls;
    unsigned long long binds_before = tex_gles2_bind_calls, skips_before = tex_gles2_bind_skips;
    tex_bake_gles2_binding_reset(); /* SDL/other code may have changed GL state */
    used = batch_first = command_count = 0; batch_texture = white; current_stats = stats; flush_failed = false;
#ifdef RR_GLES2_MULTITEXTURE_BATCH
    multi_gpu_begin(g_eng_frame);
#endif
#ifdef RR_GLES_SUBMISSION_PROFILE
    memset(&batch_bounds, 0, sizeof batch_bounds);
#endif
    SP_BEGIN(sp_setup,0,1);
    glViewport(0, 0, ENG_SCREEN_W/AUDIT_DIVISOR, ENG_SCREEN_H/AUDIT_DIVISOR);
    glDisable(GL_SCISSOR_TEST); glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_TRUE);
    if(priority_enabled)glDisable(GL_BLEND);else {glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);}
    glClearColor(bg[0], bg[1], bg[2], priority_enabled?0:1);
    glClearDepthf(1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    /* Game projection already happened in geo_hw; this matrix maps the exact
     * screen coordinates to clip space. z encodes sorted painter priority,
     * not eye distance (System 22 has priority bands, not a conventional zbuffer). */
    const GLfloat matrix[16] = {2.0f/640,0,0,0, 0,-2.0f/480,0,0, 0,0,1,0, -1,1,0,1};
    glUseProgram(program);
    glUniformMatrix4fv(projection, 1, GL_FALSE, matrix);
#ifdef RR_GLES2_MULTITEXTURE_BATCH
    if (multi_mode) for (int i = 0; i < MULTI_UNITS; i++)
        glUniform1i(multi_samplers[i], multi_mode == MULTI_BATCH ? i : 0);
    else
#endif
        glUniform1i(sampler, 0);
    if(priority_uniform>=0)glUniform1i(priority_uniform,priority_enabled);
    glActiveTexture(GL_TEXTURE0); tex_bake_gles2_bind(GL_TEXTURE_2D, white);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    if (!mv_begin(bg)) return false;
#ifdef RR_GLES_WORLD_ATLAS
    world_begin(bg,count);
#endif
    SP_END(sp_setup);
    SP_BEGIN(sp_attributes,1,1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, position));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, colour));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, uvq));
    for (int i = 0; i < 3; i++) glEnableVertexAttribArray((GLuint)i);
#ifdef RR_GLES2_MULTITEXTURE_BATCH
    if (multi_mode) {
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, texture_slot));
        glEnableVertexAttribArray(3);
    }
#endif
    SP_END(sp_attributes);
    int previous[4] = {-1,-1,-1,-1};
    for (int qi = 0; qi < count; qi++) {
#ifdef RR_PARALLEL_FRAME_PREP
        const geo_quad *q = &quads[order ? order[qi] : eng_quad_index(qi)];
#else
        const geo_quad *q = &quads[eng_quad_index(qi)];
#endif
        if (q->nrv < 3 || q->nrv > 10) continue;
        int left = q->clip[0] < 0 ? 0 : q->clip[0];
        int right = q->clip[1] > 639 ? 639 : q->clip[1];
        int top = q->clip[2] < 0 ? 0 : q->clip[2];
        int bottom = q->clip[3] > 479 ? 479 : q->clip[3];
        if (right < left || bottom < top) continue;
        const int box[4] = {left, 479-bottom, right-left+1, bottom-top+1};
        if (memcmp(box, previous, sizeof box)) {
            if (!flush(stats, SCISSOR)) return false;
#ifdef RR_ORDERED_SUBMISSION
            /* Frame commands carry scissor state; GL needs it only at draw.
             * Legacy immediate submission still requires the original update. */
            if (!frame_upload) {
#endif
            glEnable(GL_SCISSOR_TEST); glScissor(box[0], box[1], box[2], box[3]);
#ifdef RR_ORDERED_SUBMISSION
            }
#endif
            memcpy(previous, box, sizeof box);
            memcpy(batch_scissor, box, sizeof box);
        }
        /* Same UV bbox, degenerate expansion, bake scales and palette-solid
         * rules as the desktop submission. The decoder/cache itself is shared. */
        int min_u = q->uvbox[0], min_v = q->uvbox[2];
        int ru = q->uvbox[1] - min_u + 1, rv = q->uvbox[3] - min_v + 1;
        if (ru < 1) ru = 1;
        if (rv < 1) rv = 1;
        float su = 1, sv = 1, ou = 0, ov = 0, solid_rgb[3] = {1, 1, 1};
        bool solid = q->objectflags != 0, no_shade = false;
        GLuint texture = white;
#ifdef RR_GLES_WORLD_ATLAS
        int array=0,layer=0;GLuint reference=white;
#endif
        if (!diagnostic && solid) {
            int col = (q->color >> 8) & 255, pen;
            if (q->objectflags & 6) { pen = q->cz_adjust & 0x7fff; no_shade = true; }
            else pen = ((col & 127) << 8) + (((q->cz_adjust >> 16) & 127) & (col | 31));
            pen &= 0x7fff;
            for (int c = 0; c < 3; c++) solid_rgb[c] = direct_palette[pen >> 8][pen & 255][c] / 255.0f;
        } else if (!diagnostic) {
            int x0 = 640, x1 = 0, y0 = 480, y1 = 0;
            for (int i = 0; i < q->nrv; i++) {
                int x = q->rv[i].sx16 >> 4, y = q->rv[i].sy16 >> 4;
                if (x < x0) x0 = x;
                if (x > x1) x1 = x;
                if (y < y0) y0 = y;
                if (y > y1) y1 = y;
            }
            if (x0 < left) x0 = left;
            if (x1 > right) x1 = right;
            if (y0 < top) y0 = top;
            if (y1 > bottom) y1 = bottom;
            g_tex_bake_cap_req = (x1 - x0 > y1 - y0 ? x1 - x0 : y1 - y0) + 1;
            double lookup_start = seconds(), draw_before = stats->draw_seconds;
            double upload_before = tex_gles2_upload_seconds;
            eng_audit_scope audit_lookup=eng_audit_begin(AUD_LOOKUP);
            texture = bake_quad_texture(min_u, min_v, ru, rv, q->texbank,
                                       (q->color >> 8) & 127, q->cmode, &su, &sv, &ou, &ov);
            eng_audit_end(audit_lookup);
            stats->texture_seconds += seconds() - lookup_start -
                (tex_gles2_upload_seconds - upload_before) - (stats->draw_seconds - draw_before);
            if (!texture || flush_failed) return false;
#ifdef RR_GLES_WORLD_ATLAS
            if(world_mode){layer=tex_bake_world_layer();array=layer>=0;reference=array?tex_bake_world_reference():texture;}
#endif
        }
        bool different=texture!=batch_texture;
#ifdef RR_GLES_WORLD_ATLAS
        if(world_mode)different|=array!=batch_array || layer!=batch_layer || reference!=batch_reference;
#endif
        bool texture_break = used && different;
        if (different) {
            if (!flush(stats, TEXTURE)) return false;
            batch_texture = texture;
        } else stats->same_texture++;
#ifdef RR_GLES_WORLD_ATLAS
        batch_array=array;batch_layer=layer;batch_reference=reference;
#endif
        int bu = min_u, bv = min_v, bru = ru, brv = rv;
        if (bru < 16) { bu = (bu * 2 + bru) / 2 - 8; bru = 16; }
        if (brv < 16) { bv = (bv * 2 + brv) / 2 - 8; brv = 16; }
        if (bu < 0) bu = 0;
        if (bv < 0) bv = 0;
        geo_sv source[16], clipped[32];
        for (int i = 0; i < q->nrv; i++) {
            const geo_vert *v = &q->rv[i];
            float iw = v->z > 0 ? 1.0f / v->z : 1.0f;
            float tu = ou + ((int)v->u - bu + 0.5f) / bru * su;
            float tv = ov + ((int)v->v - bv + 0.5f) / brv * sv;
            source[i] = (geo_sv){v->sx16 / 16.0f, v->sy16 / 16.0f,
                tu * iw, tv * iw, iw, no_shade ? 1.0f : v->bri / 64.0f};
        }
        int n = clip_to_screen(source, q->nrv, clipped);
        if (n < 3) {
            if (texture_break) stats->empty_texture_flush++;
            continue;
        }
        if (batch_first + used + (n-2)*3 > MAX_VERTICES) {
            if (!flush(stats, CAPACITY) || (frame_upload && !submit_commands(stats))) return false;
        }
        /* Optional diagnostic per-object colours; q->color is a palette selector,
         * never RGB. Normal rendering uses the decoder and palette above. */
        uint32_t hash = ((uint32_t)q->pick_code ^ q->color ^ (q->direct ? 0x1234u : 0)) * 2654435761u;
        float colour[4] = {0.25f+(hash>>24&255)/340.0f,
            0.25f+(hash>>16&255)/340.0f, 0.25f+(hash>>8&255)/340.0f, 1};
        float z = 0.9f - 1.8f * (qi+1.0f) / (count+1.0f);

#ifdef RR_GLES_SUBMISSION_PROFILE
        static FILE *resource_output;FILE *resource_dump=NULL;
        if(getenv("GLES_RESOURCE_DUMP") && multi_present_tick==(unsigned)strtoul(getenv("GLES_RESOURCE_DUMP_FRAME")?getenv("GLES_RESOURCE_DUMP_FRAME"):"2500",NULL,10)){
            if(!resource_output){resource_output=fopen(getenv("GLES_RESOURCE_DUMP"),"wx");if(!resource_output)return false;}
            resource_dump=resource_output;
        }
        if(resource_dump){
            fprintf(resource_dump,"P %d %u %d %d %d %d %d %d %d %d %d %d %a %a %a %a %d %d %d %d\n",qi,texture,q->pick_code,q->texbank,q->color,q->cmode,bu,bv,bru,brv,n,solid,su,sv,ou,ov,box[0],box[1],box[2],box[3]);
            extern void tex_bake_gles2_resource_dump(FILE *,GLuint);tex_bake_gles2_resource_dump(resource_dump,texture);
        }
#endif
        for (int triangle = 1; triangle+1 < n; triangle++) {
            for (int corner = 0; corner < 3; corner++) {
                const geo_sv *v = &clipped[eng_fan_index(triangle, corner)];
                vertex *out = &batch[batch_first + used++];
                out->position[0]=v->x; out->position[1]=v->y; out->position[2]=z;
                if (diagnostic) {memcpy(out->colour, colour, sizeof colour);if(priority_enabled)out->colour[3]=(q->cmode&7)==1;}
                else {
                    for (int c = 0; c < 3; c++) out->colour[c] = solid_rgb[c] * v->bri;
                    out->colour[3] = priority_enabled ? ((q->cmode&7)==1 ? 1 : 0) : 1;
                }
                out->uvq[0]=v->s; out->uvq[1]=v->t; out->uvq[2]=v->w;
#ifdef RR_GLES_WORLD_ATLAS
                out->layer=(float)layer;
#endif
#ifdef RR_GLES2_MULTITEXTURE_BATCH
                out->texture_slot=0;
#endif
#ifdef RR_GLES_SUBMISSION_PROFILE
                if(resource_dump)fprintf(resource_dump,"V %a %a %a %a %a %a %a %a %a %a\n",out->position[0],out->position[1],out->position[2],out->colour[0],out->colour[1],out->colour[2],out->colour[3],out->uvq[0],out->uvq[1],out->uvq[2]);
#endif
            }
        }
    }
#ifdef RR_GLES_SUBMISSION_PROFILE
    /* Manifest runs end at the selected capture tick. */
    if(getenv("GLES_RESOURCE_DUMP"))fflush(NULL);
#endif
    bool ok = flush(stats, END) && (!frame_upload || submit_commands(stats)) && check_gl("end frame");
#ifdef RR_GLES2_MULTITEXTURE_BATCH
    multi_gpu_end();
#endif
    if (!mv_end()) return false;
#ifdef RR_GLES_WORLD_ATLAS
    if(!world_end())return false;
#endif
    SP_FULL_FRAME_END();
    stats->cache_hits = (unsigned)(tex_frame_hits - hits_before);
    stats->cache_misses = (unsigned)(tex_frame_misses - misses_before);
    stats->cache_evictions = (unsigned)(tex_cache_evictions - evictions_before);
    stats->texture_uploads = (unsigned)(tex_reallocs + tex_subimages - uploads_count_before);
    stats->texture_bytes = tex_bake_gles2_resident_bytes();
    stats->upload_seconds = tex_gles2_upload_seconds - uploads_before;
    stats->decode_seconds = tex_gles2_decode_seconds - decode_before;
    stats->decoded_texels = g_bake_texels - texels_before;
    stats->eviction_seconds = tex_gles2_eviction_seconds - eviction_before;
    stats->clock_probes = tex_gles2_clock_probes - probes_before;
    stats->delete_calls = tex_gles2_delete_calls - deletes_before;
    stats->binds = (unsigned)(tex_gles2_bind_calls - binds_before);
    stats->bind_skips = (unsigned)(tex_gles2_bind_skips - skips_before);
#ifdef RR_GLES_SUBMISSION_PROFILE
    if (batch_trace && g_eng_frame >= sp_from && g_eng_frame <= sp_to) {
        fprintf(batch_trace, "%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",
                g_eng_frame, stats->draws, stats->triangles, batch_bounds.chunks,
                batch_bounds.scissor_breaks, batch_bounds.texture_breaks,
                batch_bounds.groups[0], batch_bounds.groups[1], batch_bounds.groups[2],
                batch_bounds.groups[3], batch_bounds.groups[4], batch_bounds.groups[5],
                batch_bounds.largest_group, stats->texture_barriers,
                stats->flush_scissor, stats->flush_capacity, stats->empty_texture_flush);
    }
#endif
    return ok;
}
#ifdef RR_PARALLEL_FRAME_PREP
bool eng_gles2_draw(const geo_quad *quads, int count, const float bg[3], eng_gles2_stats *stats)
{
    return eng_gles2_draw_ordered(quads, NULL, count, bg, stats);
}
#endif

void eng_gles2_close(void)
{
#ifdef RR_GLES2_MULTITEXTURE_BATCH
    mv_close();
    multi_gpu_close();
#endif
#ifdef RR_GLES_SUBMISSION_PROFILE
    if (batch_trace) { fclose(batch_trace); batch_trace = NULL; }
#endif
    tex_bake_set_flush_hook(NULL);
#ifdef RR_GLES_WORLD_ATLAS
    world_close();
#endif
    tex_bake_gles2_experiment_close();
    tex_bake_gles2_report();
    renderer_texture_init(); /* delete cache storage before context destruction */
    if (white) glDeleteTextures(1, &white);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (program) glDeleteProgram(program);
    white = vbo = program = 0;
}
