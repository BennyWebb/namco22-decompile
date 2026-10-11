/* Flat-colour GLES2 backend. No atlas, fixed-function calls or render targets.
 * Sorting, screen clipping and fan indices are shared with quad_gl.c.
 * geo_hw has already applied the game's fixed-point view/object projection. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include "eng_gl.h"
#include "quad_gl.h"
#include "quad_gles2.h"

float g_scene_x0 = 0, g_scene_x1 = ENG_SCREEN_W;
#include "quad_geometry.inc"

#define MAX_VERTICES 60000
typedef struct { float position[3], colour[4], uvq[3]; } vertex;
static vertex batch[MAX_VERTICES];
static int used;
static GLuint program, vbo, white;
static GLint projection, sampler;

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
        "varying mediump vec3 v_uvq;\n"
        "void main() { gl_Position = u_projection * vec4(a_position, 1.0);"
        " v_colour = a_colour; v_uvq = a_uvq; }\n";
    static const char *fs =
        "#version 100\nprecision mediump float;\n"
        "uniform sampler2D u_texture;\n"
        "varying mediump vec4 v_colour;\n"
        "varying mediump vec3 v_uvq;\n"
        "void main() { vec2 uv = v_uvq.xy / max(v_uvq.z, 0.0001);"
        " gl_FragColor = v_colour * texture2D(u_texture, uv); }\n";
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
    glBufferData(GL_ARRAY_BUFFER, sizeof batch, NULL, GL_DYNAMIC_DRAW);
    if (!check_gl("allocate triangle VBO")) return false;
    fprintf(stderr, "[GLES2] triangle storage: CPU=%zu bytes, GPU buffer=%zu bytes\n", sizeof batch, sizeof batch);
    const size_t texture_budget = (size_t)RR_GLES2_TEXTURE_BUDGET_KB * 1024;
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
    fprintf(stderr, "[GLES2] GLSL ES 1.00 linked; texture bytes=4 budget=%zu; source atlas disabled\n", texture_budget);
    fprintf(stderr, "[GLES2] unsupported/bypassed: texture ROM sampling, HUD/text, fog, gamma/fade, Nuklear, blits, render targets\n");
    return true;
}

static bool flush(eng_gles2_stats *stats)
{
    if (!used) return true;
    glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(used * sizeof(vertex)), batch);
    glDrawArrays(GL_TRIANGLES, 0, used);
    stats->triangles += (unsigned)used / 3;
    stats->draws++;
    used = 0;
    return check_gl("submit triangles");
}

bool eng_gles2_draw(const geo_quad *quads, int count, const float bg[3], eng_gles2_stats *stats)
{
    memset(stats, 0, sizeof *stats);
    used = 0;
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
    glActiveTexture(GL_TEXTURE0); glBindTexture(GL_TEXTURE_2D, white);
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
            if (!flush(stats)) return false;
            glEnable(GL_SCISSOR_TEST); glScissor(box[0], box[1], box[2], box[3]);
            memcpy(previous, box, sizeof box);
        }
        geo_sv source[16], clipped[32];
        for (int i = 0; i < q->nrv; i++) {
            const geo_vert *v = &q->rv[i];
            float iw = v->z > 0 ? 1.0f / v->z : 1.0f;
            source[i] = (geo_sv){v->sx16/16.0f, v->sy16/16.0f,
                (v->u+0.5f)/4096.0f*iw, (v->v+0.5f)/4096.0f*iw, iw, v->bri/64.0f};
        }
        int n = clip_to_screen(source, q->nrv, clipped);
        if (n < 3) continue;
        if (used + (n-2)*3 > MAX_VERTICES && !flush(stats)) return false;
        /* Clearly diagnostic per-object colours; q->color is a palette selector,
         * never RGB. Actual texture/palette resolution is deferred to Phase 2. */
        uint32_t hash = ((uint32_t)q->pick_code ^ q->color ^ (q->direct ? 0x1234u : 0)) * 2654435761u;
        float colour[4] = {0.25f+(hash>>24&255)/340.0f,
            0.25f+(hash>>16&255)/340.0f, 0.25f+(hash>>8&255)/340.0f, 1};
        float z = 0.9f - 1.8f * (qi+1.0f) / (count+1.0f);
        for (int triangle = 1; triangle+1 < n; triangle++) {
            for (int corner = 0; corner < 3; corner++) {
                const geo_sv *v = &clipped[eng_fan_index(triangle, corner)];
                vertex *out = &batch[used++];
                out->position[0]=v->x; out->position[1]=v->y; out->position[2]=z;
                memcpy(out->colour, colour, sizeof colour);
                out->uvq[0]=v->s; out->uvq[1]=v->t; out->uvq[2]=v->w;
            }
        }
    }
    return flush(stats) && check_gl("end frame");
}

void eng_gles2_close(void)
{
    if (white) glDeleteTextures(1, &white);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (program) glDeleteProgram(program);
    white = vbo = program = 0;
}
