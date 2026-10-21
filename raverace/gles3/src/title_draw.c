/* Utility font only. Native GLES3, independent of the emulated HUD/CGRAM. */
#include "title_draw.h"
#include "rr_font.h"
#include <GLES3/gl3.h>
#include <stdio.h>
#include <string.h>
typedef struct {
    float x, y, u, v, r, g, b, a;
} vertex;
static vertex vertices[6 * 1088];
static size_t used;
static GLuint program, vao, vbo, atlas;
static char *diagnostic;
static size_t diagnostic_size;
static bool valid;
static bool error(const char *s)
{
    snprintf(diagnostic, diagnostic_size, "%s", s);
    valid = false;
    return false;
}
static GLuint shader(GLenum type, const char *text)
{
    GLuint id = glCreateShader(type);
    glShaderSource(id, 1, &text, NULL);
    glCompileShader(id);
    GLint ok;
    glGetShaderiv(id, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(id, sizeof log, NULL, log);
        fprintf(stderr, "[TITLE-GL] %s\n", log);
        glDeleteShader(id);
        error("GL_SHADER");
        return 0;
    }
    return id;
}
bool title_draw_open(char *e, size_t n)
{
    diagnostic = e;
    diagnostic_size = n;
    valid = true;
    const char *vs = "#version 300 es\nprecision highp float;layout(location=0) in vec2 p;layout(location=1) in vec2 "
                     "uv;layout(location=2) in vec4 c;out "
                     "vec2 t;out vec4 color;void main(){gl_Position=vec4(p.x/320.-1.,1.-p.y/240.,0,1);t=uv;color=c;}";
    const char *fs = "#version 300 es\nprecision highp float;uniform sampler2D font;in vec2 t;in vec4 color;out vec4 "
                     "o;void main(){o=vec4(color.rgb,color.a*texture(font,t).r);}";
    GLuint a = shader(GL_VERTEX_SHADER, vs), b = shader(GL_FRAGMENT_SHADER, fs);
    if (!a || !b)
        return false;
    program = glCreateProgram();
    glAttachShader(program, a);
    glAttachShader(program, b);
    glLinkProgram(program);
    glDeleteShader(a);
    glDeleteShader(b);
    GLint ok;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok)
        return error("GL_LINK");
    uint8_t pens[128 * 128] = {0};
    for (unsigned c = 0; c < 95; c++)
        for (unsigned y = 0; y < 16; y++)
            for (unsigned x = 0; x < 8; x++)
                pens[((c / 16) * 16 + y) * 128 + (c % 16) * 8 + x] = (rr_font8x16[c][y] & (128 >> x)) ? 255 : 0;
    pens[128 * 128 - 1] = 255;
    glGenTextures(1, &atlas);
    glBindTexture(GL_TEXTURE_2D, atlas);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, 128, 128, 0, GL_RED, GL_UNSIGNED_BYTE, pens);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof vertices, NULL, GL_DYNAMIC_DRAW);
    for (unsigned i = 0; i < 3; i++)
        glEnableVertexAttribArray(i);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)(2 * sizeof(float)));
    glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *)(4 * sizeof(float)));
    return glGetError() == GL_NO_ERROR ? true : error("GL_INIT");
}
static void quad(float x, float y, float w, float h, float u, float v, float uw, float vh, float r, float g, float b)
{
    if (used + 6 > sizeof vertices / sizeof *vertices) {
        error("GLYPH_OVERFLOW");
        return;
    }
    const int ix[] = {0, 1, 0, 0, 1, 1}, iy[] = {0, 0, 1, 1, 0, 1};
    for (unsigned i = 0; i < 6; i++)
        vertices[used++] = (vertex){x + ix[i] * w, y + iy[i] * h, u + ix[i] * uw, v + iy[i] * vh, r, g, b, 1};
}
static void rect(float x, float y, float w, float h, float r, float g, float b)
{
    quad(x, y, w, h, 127.5f / 128, 127.5f / 128, 0, 0, r, g, b);
}
static void text(float x, float y, const char *s, int scale, float r, float g, float b)
{
    float start = x;
    for (; *s; s++) {
        if (*s == '\n') {
            x = start;
            y += 18 * scale;
            continue;
        }
        unsigned c = (unsigned char)*s;
        if (c < 32 || c > 126) {
            error("GLYPH_INVALID");
            return;
        }
        c -= 32;
        quad(x, y, 8 * scale, 16 * scale, (c % 16) * 8.f / 128, (c / 16) * 16.f / 128, 8.f / 128, 16.f / 128, r, g, b);
        x += 8 * scale;
        if (x > 624) {
            error("TEXT_OVERFLOW");
            return;
        }
    }
}
bool title_draw(SDL_Window *window, const tm_state *s, char *e, size_t n)
{
    diagnostic = e;
    diagnostic_size = n;
    valid = true;
    used = 0;
    int w, h;
    SDL_GL_GetDrawableSize(window, &w, &h);
    if (w != 640 || h != 480)
        return error("DRAWABLE_SIZE");
    glViewport(0, 0, w, h);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(.025f, .045f, .095f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    for (unsigned i = 0; i < 5; i++)
        rect(24 + i * 134, 462 - i * 6, 70, 2, .08f, .12f, .19f);
    text(24, 24, "RAVE RACER", 2, .98f, .87f, .62f);
    const char *pages[] = {
        "START", "GAME", "CAR APPEARANCE", "CONTROLS", "SAVE CHANGES?", "QUIT?", "RESET CONTROLS?", "REBIND", "ERROR"};
    text(24, 68, pages[s->page], 1, .6f, .72f, .83f);
    if (s->page == TM_ERROR) {
        /* Paths wrap on character boundaries instead of disappearing offscreen. */
        char line[71];
        const char *p = s->error;
        float y = 120;
        while (*p) {
            unsigned i = 0;
            while (*p && *p != '\n' && i < 70)
                line[i++] = *p++;
            line[i] = 0;
            if (*p == '\n')
                p++;
            text(24, y, line, 1, 1, .55f, .45f);
            y += 22;
            if (y > 400) {
                error("ERROR_TEXT_OVERFLOW");
                break;
            }
        }
    } else if (s->page == TM_CAPTURE) {
        text(24, 120, ti_labels[s->capture], 2, .98f, .94f, .8f);
        text(24,
             180,
             s->blocked ? "Release all controls first" : "Press a button / hat; steer left for axis",
             1,
             .8f,
             .86f,
             .92f);
        text(24, 220, "START, SELECT and X are reserved", 1, .8f, .86f, .92f);
    } else {
        int first = s->focus >= 6 ? s->focus - 5 : 0, rows = tm_rows(s);
        for (int row = first; row < rows && row < first + 6; row++) {
            float y = 118 + (row - first) * 46;
            bool focus = row == s->focus;
            if (focus)
                rect(20, y - 4, 600, 40, .78f, .46f, .12f);
            char label[128];
            tm_label(s, row, label, sizeof label);
            text(32, y, label, 2, focus ? .05f : .96f, focus ? .045f : .92f, focus ? .03f : .8f);
        }
        if (s->page == TM_CAR && s->focus < 8) {
            const float c[4][3] = {{.2f, .4f, 1}, {1, .85f, .15f}, {1, .15f, .12f}, {.15f, .8f, .3f}};
            unsigned color = (unsigned)s->focus % 4;
            rect(546, 36, 66, 40, c[color][0], c[color][1], c[color][2]);
        }
    }
    if (s->status[0])
        text(24, 404, s->status, 1, 1, .7f, .35f);
    else if (tm_dirty(s))
        text(24, 404, "Unsaved changes - applied at next game boot", 1, 1, .7f, .35f);
    text(24,
         436,
         s->page == TM_CAPTURE ? "Press input  START cancel  START+SELECT exit"
         : s->page == TM_ERROR ? "A/B acknowledge  START+SELECT exit"
                               : "D-pad move  A confirm  B back  START+SELECT exit",
         1,
         .65f,
         .75f,
         .85f);
    if (!valid)
        return false;
    glUseProgram(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, atlas);
    glUniform1i(glGetUniformLocation(program, "font"), 0);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, used * sizeof(vertex), vertices);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)used);
    if (glGetError() != GL_NO_ERROR)
        return error("GL_DRAW");
    return true;
}
void title_draw_close(void)
{
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteTextures(1, &atlas);
    glDeleteProgram(program);
    vbo = vao = atlas = program = 0;
}
