/* Original-ROM GLES2 backend. No atlas, fixed-function calls or render targets.
 * Sorting, screen clipping and fan indices are shared with quad_gl.c.
 * geo_hw has already applied the game's fixed-point view/object projection. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <time.h>
#include "eng_gl.h"
#include "quad_gl.h"
#include "quad_gles2.h"
#include "tex_bake.h"
#include "eng.h"

float g_scene_x0 = 0, g_scene_x1 = ENG_SCREEN_W;
#include "quad_geometry.inc"

#define MAX_VERTICES 60000
typedef struct { float position[3], colour[4], uvq[3]; } vertex;
static vertex batch[MAX_VERTICES];
/* Ordered commands share the existing CPU arena; no geometry is reordered. */
#define MAX_COMMANDS 4096
typedef struct { GLuint texture; int first, count, scissor[4]; } draw_command;
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
static GLint projection, sampler;
static GLuint batch_texture;
static eng_gles2_stats *current_stats;
static bool diagnostic, flush_failed;
static bool white_batches;
static const char *mode_name = "textured";
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
    if (!mib || mib > 64 || (size_t)mib * 1024 > RR_GLES2_TEXTURE_BUDGET_KB) return false;
    texture_budget = (size_t)mib * 1024 * 1024;
    return true;
}
static void before_texture_change(GLuint texture)
{
    if (used && texture == batch_texture && !flush(current_stats, MUTATION)) flush_failed = true;
    /* Cache updates/deletions must not change any queued draw's texels. Drain
     * the ordered prefix before mutation, retaining a different pending batch. */
    if (frame_upload) for (int i = 0; i < command_count; i++) {
        if (commands[i].texture == texture) {
            current_stats->texture_barriers++;
            if (!submit_commands(current_stats)) flush_failed = true;
            break;
        }
    }
}

static bool check_gl(const char *stage)
{
    GLenum error = glGetError();
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

bool eng_gles2_init(void)
{
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
    static const char *fs =
        "#version 100\nprecision highp float;\n"
        "uniform sampler2D u_texture;\n"
        "varying mediump vec4 v_colour;\n"
        "varying highp vec3 v_uvq;\n"
        "void main() { vec2 uv = v_uvq.xy / v_uvq.z;"
        " vec4 texel = texture2D(u_texture, uv);"
        " if (texel.a == 0.0) discard; gl_FragColor = v_colour * texel; }\n";
    GLint max_texture, precision, range[2];
    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture);
    glGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT, range, &precision);
    fprintf(stderr, "[TEX] GL max texture=%d fragment highp bits=%d\n", max_texture, precision);
    if (max_texture < 1024 || precision == 0) {
        fprintf(stderr, "[TEX] unsupported: requires 1024 texture size and fragment highp for original perspective UVs\n");
        return false;
    }
    GLuint v = compile(GL_VERTEX_SHADER, vs);
    GLuint f = compile(GL_FRAGMENT_SHADER, fs);
    if (!v || !f) { if (v) glDeleteShader(v); if (f) glDeleteShader(f); return false; }
    program = glCreateProgram();
    glAttachShader(program, v); glAttachShader(program, f);
    glBindAttribLocation(program, 0, "a_position");
    glBindAttribLocation(program, 1, "a_colour");
    glBindAttribLocation(program, 2, "a_uvq");
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
    if (projection < 0 || sampler < 0) { fprintf(stderr, "[GLES2] missing shader uniforms\n"); return false; }
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
    fprintf(stderr, "[GLES2] GLSL ES 1.00 linked; textures=%s budget=%zu; per-texture cache, atlas disabled\n",
            mode_name, texture_budget);
    fprintf(stderr, "[GLES2] unsupported/bypassed: HUD/text, fog, gamma/fade, Nuklear, blits, render targets\n");
    return true;
}

static bool submit_commands(eng_gles2_stats *stats)
{
    if (!command_count) return true;
    double start = seconds(), vertex_start = start;
    size_t bytes = (size_t)batch_first * sizeof(vertex);
    /* GLES2 glBufferData re-specifies the store rather than overwriting the
     * in-flight offset-zero range. Driver storage renaming is implementation
     * dependent; retain legacy mode for measurement on Mali. */
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)bytes, batch, GL_STREAM_DRAW);
    stats->vertex_upload_seconds += seconds() - vertex_start;
    stats->vertex_uploads++; stats->vertex_upload_bytes += bytes;
    if (!check_gl("upload vertex chunk")) return false;
    for (int i = 0; i < command_count; i++) {
        const draw_command *c = &commands[i];
        glEnable(GL_SCISSOR_TEST);
        glScissor(c->scissor[0], c->scissor[1], c->scissor[2], c->scissor[3]);
        tex_bake_gles2_bind(GL_TEXTURE_2D, white_batches ? white : c->texture);
        double call_start = seconds();
        glDrawArrays(GL_TRIANGLES, c->first, c->count);
        stats->draw_call_seconds += seconds() - call_start;
        stats->triangles += (unsigned)c->count / 3; stats->draws++;
    }
    bool ok = check_gl("submit vertex chunk");
    /* A cache barrier may drain earlier commands while another texture's
     * pending vertices remain. Retain those vertices at the arena start. */
    if (used) memmove(batch, batch + batch_first, (size_t)used * sizeof(vertex));
    batch_first = command_count = 0;
    stats->draw_seconds += seconds() - start;
    return ok;
}

static bool flush(eng_gles2_stats *stats, enum flush_reason reason)
{
    if (!used) return true;
    if (frame_upload && command_count == MAX_COMMANDS && !submit_commands(stats)) return false;
    double start = seconds();
    bool ok = true;
    if (frame_upload) {
        draw_command *c = &commands[command_count++];
        c->texture = batch_texture; c->first = batch_first; c->count = used;
        memcpy(c->scissor, batch_scissor, sizeof batch_scissor);
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
    stats->draw_seconds += seconds() - start;
    return ok;
}

bool eng_gles2_draw(const geo_quad *quads, int count, const float bg[3], eng_gles2_stats *stats)
{
    memset(stats, 0, sizeof *stats);
    double uploads_before = tex_gles2_upload_seconds;
    unsigned long long binds_before = tex_gles2_bind_calls, skips_before = tex_gles2_bind_skips;
    tex_bake_gles2_binding_reset(); /* SDL/other code may have changed GL state */
    used = batch_first = command_count = 0; batch_texture = white; current_stats = stats; flush_failed = false;
    glViewport(0, 0, ENG_SCREEN_W, ENG_SCREEN_H);
    glDisable(GL_SCISSOR_TEST); glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST); glDepthFunc(GL_LEQUAL); glDepthMask(GL_TRUE);
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(bg[0], bg[1], bg[2], 1);
    glClearDepthf(1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    /* Game projection already happened in geo_hw; this matrix maps the exact
     * screen coordinates to clip space. z encodes sorted painter priority,
     * not eye distance (System 22 has priority bands, not a conventional zbuffer). */
    const GLfloat matrix[16] = {2.0f/640,0,0,0, 0,-2.0f/480,0,0, 0,0,1,0, -1,1,0,1};
    glUseProgram(program);
    glUniformMatrix4fv(projection, 1, GL_FALSE, matrix);
    glUniform1i(sampler, 0);
    glActiveTexture(GL_TEXTURE0); tex_bake_gles2_bind(GL_TEXTURE_2D, white);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, position));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, colour));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)offsetof(vertex, uvq));
    for (int i = 0; i < 3; i++) glEnableVertexAttribArray((GLuint)i);
    int previous[4] = {-1,-1,-1,-1};
    for (int qi = 0; qi < count; qi++) {
        const geo_quad *q = &quads[qi];
        if (q->nrv < 3 || q->nrv > 10) continue;
        int left = q->clip[0] < 0 ? 0 : q->clip[0];
        int right = q->clip[1] > 639 ? 639 : q->clip[1];
        int top = q->clip[2] < 0 ? 0 : q->clip[2];
        int bottom = q->clip[3] > 479 ? 479 : q->clip[3];
        if (right < left || bottom < top) continue;
        const int box[4] = {left, 479-bottom, right-left+1, bottom-top+1};
        if (memcmp(box, previous, sizeof box)) {
            if (!flush(stats, SCISSOR)) return false;
            glEnable(GL_SCISSOR_TEST); glScissor(box[0], box[1], box[2], box[3]);
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
            texture = bake_quad_texture(min_u, min_v, ru, rv, q->texbank,
                                       (q->color >> 8) & 127, q->cmode, &su, &sv, &ou, &ov);
            stats->texture_seconds += seconds() - lookup_start -
                (tex_gles2_upload_seconds - upload_before) - (stats->draw_seconds - draw_before);
            if (!texture || flush_failed) return false;
        }
        bool texture_break = used && texture != batch_texture;
        if (texture != batch_texture) {
            if (!flush(stats, TEXTURE)) return false;
            batch_texture = texture;
        } else stats->same_texture++;
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
        for (int triangle = 1; triangle+1 < n; triangle++) {
            for (int corner = 0; corner < 3; corner++) {
                const geo_sv *v = &clipped[eng_fan_index(triangle, corner)];
                vertex *out = &batch[batch_first + used++];
                out->position[0]=v->x; out->position[1]=v->y; out->position[2]=z;
                if (diagnostic) memcpy(out->colour, colour, sizeof colour);
                else {
                    for (int c = 0; c < 3; c++) out->colour[c] = solid_rgb[c] * v->bri;
                    out->colour[3] = 1; /* shade RGB, preserve decoded alpha */
                }
                out->uvq[0]=v->s; out->uvq[1]=v->t; out->uvq[2]=v->w;
            }
        }
    }
    bool ok = flush(stats, END) && (!frame_upload || submit_commands(stats)) && check_gl("end frame");
    stats->upload_seconds = tex_gles2_upload_seconds - uploads_before;
    stats->binds = (unsigned)(tex_gles2_bind_calls - binds_before);
    stats->bind_skips = (unsigned)(tex_gles2_bind_skips - skips_before);
    return ok;
}

void eng_gles2_close(void)
{
    tex_bake_set_flush_hook(NULL);
    tex_bake_gles2_report();
    renderer_texture_init(); /* delete cache storage before context destruction */
    if (white) glDeleteTextures(1, &white);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (program) glDeleteProgram(program);
    white = vbo = program = 0;
}
