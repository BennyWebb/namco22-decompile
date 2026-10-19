#define _GNU_SOURCE 1
static int hud_control = 0, hud_fences = 0;
static double hud_pen_wall, hud_pen_cpu;
#include <time.h>
static double hud_stamp(clockid_t k)
{
    struct timespec t;
    clock_gettime(k, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}
/* System 22 composition on an RGBA8 target: framebuffer alpha is prioverchar.
 * One final pass applies original text/shadow priority and fade/gamma LUTs.
 * No emulation pointers or mutable hardware state enter this module. */
#include "rr_video_layers.h"
#include "eng_gl.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#define W 640
#define H 480
static bool enabled = true, profiling;
static _Thread_local bool profile_gate = true;
static unsigned profile_frames;
static unsigned decode_frames;
static double decode_cpu, upload_cpu, upload_wall, compose_cpu, compose_wall;
static double stamp(clockid_t clock)
{
    struct timespec t;
    clock_gettime(clock, &t);
    return t.tv_sec + t.tv_nsec * 1e-9;
}
void rr_video_layers_profile(bool on)
{
    profiling = on;
}
void rr_video_layers_profile_gate(bool on)
{
    profile_gate = on;
}
bool rr_video_layers_profile_enabled(void)
{
    return profiling && profile_gate;
}
double rr_video_layers_cpu_time(void)
{
    return rr_video_layers_profile_enabled() ? stamp(CLOCK_THREAD_CPUTIME_ID) : 0;
}
void rr_video_layers_decode_time(double elapsed)
{
    decode_cpu += elapsed;
    decode_frames++;
}
static unsigned readback_output;
void rr_video_readback_output(unsigned f)
{
    readback_output = f;
}
static GLuint target, depth, scene, lut_tex, vbo;
static uint8_t last_lut[256][4];
static bool lut_valid;
static unsigned frames;
void rr_video_layers_enable(bool on)
{
    enabled = on;
}
bool rr_video_layers_enabled(void)
{
    return enabled;
}
bool rr_video_layers_gpu(void)
{
    return true;
}
bool rr_video_layers_verify(void)
{
    return false;
}
static bool error(const char *where)
{
    GLenum e = glGetError();
    if (e == GL_NO_ERROR)
        return true;
    fprintf(stderr, "[VIDEO] %s: GLES error 0x%x\n", where, e);
    return false;
}
static GLuint shader(GLenum type, const char *source)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &source, NULL);
    glCompileShader(s);
    GLint ok, n;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    glGetShaderiv(s, GL_INFO_LOG_LENGTH, &n);
    if (n > 1) {
        char *log = malloc((size_t)n);
        if (log) {
            glGetShaderInfoLog(s, n, NULL, log);
            fprintf(stderr, "[VIDEO] shader: %s\n", log);
            free(log);
        }
    }
    if (!ok) {
        glDeleteShader(s);
        return 0;
    }
    return s;
}
static GLuint texture(int w, int h)
{
    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    return t;
}
#include "rr_gpu_hud.inc"
bool rr_video_layers_open(void)
{
    scene = texture(W, H);
    lut_tex = texture(256, 1);
    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, W, H);
    glGenFramebuffers(1, &target);
    glBindFramebuffer(GL_FRAMEBUFFER, target);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, scene, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        return false;
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    const GLfloat vertices[] = {-1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1};
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof vertices, vertices, GL_STATIC_DRAW);
    if (!gpu_open())
        return false;
    return error("gles3 compositor initialise");
}
void rr_video_layers_begin(void)
{
    glBindFramebuffer(GL_FRAMEBUFFER, enabled ? target : 0);
}
void rr_video_layers_close(void)
{
    if (!scene)
        return;
    gpu_close();
    glDeleteFramebuffers(1, &target);
    glDeleteRenderbuffers(1, &depth);
    GLuint tex[] = {scene, lut_tex};
    glDeleteTextures(2, tex);
    glDeleteBuffers(1, &vbo);
    scene = 0;
}

#ifdef GLES3_VALIDATION
void gles3_read_scene(uint8_t *p)
{
    glBindFramebuffer(GL_FRAMEBUFFER, target);
    glReadPixels(0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, p);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
#endif
