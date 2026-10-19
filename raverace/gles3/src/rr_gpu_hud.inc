/* Optional GLES2 packed character/tile compositor. State is selected by the
 * caller; caches compare against the last actual upload, never a skipped slot. */
#ifdef RR_GPU_HUD
#include "eng.h"
static GLuint gpu_program, raw_tex, palette_tex;
static bool fold_valid3;
static uint8_t fold_palette3[1024], fold_lut3[1024], fold_mix3[9];
static GLint gpu_samplers[4], gpu_scroll, gpu_shadow, gpu_mix[3];
static uint8_t uploaded_raw[0x20000], uploaded_palette[256][4];
static bool raw_valid, palette_valid;
static uint64_t gpu_bytes, gpu_calls, gpu_pictures, gpu_verified;
static uint64_t gpu_profile_bytes, gpu_profile_calls;
static uint64_t gpu_profile_glyph_bytes, gpu_profile_vertex_bytes, gpu_profile_palette_bytes, gpu_profile_lut_bytes;
static uint8_t *reference_pixels, *gpu_pixels;
#include "rr_gpu_hud_geometry.inc"
static bool gpu_open(void)
{
    const char *vs = "#version 100\nattribute vec2 a_position;varying highp vec2 uv;void "
                     "main(){gl_Position=vec4(a_position,0.,1.);uv=a_position*.5+.5;}\n";
    const char *fs = glyph_final_fs;
    GLuint v = shader(GL_VERTEX_SHADER, vs), f = shader(GL_FRAGMENT_SHADER, fs);
    if (!v || !f) {
        if (v)
            glDeleteShader(v);
        if (f)
            glDeleteShader(f);
        return false;
    }
    gpu_program = glCreateProgram();
    glAttachShader(gpu_program, v);
    glAttachShader(gpu_program, f);
    glBindAttribLocation(gpu_program, 0, "a_position");
    glLinkProgram(gpu_program);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok;
    glGetProgramiv(gpu_program, GL_LINK_STATUS, &ok);
    if (!ok) {
        fprintf(stderr, "[GPU-HUD] shader link failed\n");
        return false;
    }
    const char *names[] = {"scene_tex", "raw_tex", "palette_tex", "lut_tex"};
    for (int i = 0; i < 4; i++)
        gpu_samplers[i] = glGetUniformLocation(gpu_program, names[i]);
    gpu_scroll = glGetUniformLocation(gpu_program, "scroll");
    gpu_shadow = glGetUniformLocation(gpu_program, "shadow_on");
    const char *mixes[] = {"mix0", "mix1", "mix2"};
    for (int i = 0; i < 3; i++)
        gpu_mix[i] = glGetUniformLocation(gpu_program, mixes[i]);
    raw_tex = gpu_geometry() ? texture(W, H) : texture(256, 128);
    palette_tex = texture(256, 1);
    if (gpu_geometry() && !geometry_open())
        return false;
    fprintf(stderr,
            "[GPU-HUD] mode=%s GLES2; verify=%d\n",
            gpu_geometry() ? "glyph-quads" : "packed-tilemap",
            rr_video_layers_verify());
    return error("initialise packed HUD");
}
static void gpu_upload_rows(const uint8_t *source, int first, int rows)
{
    int end = first + rows;
    for (int row = first; row < end;) {
        if (raw_valid && !memcmp(uploaded_raw + row * 1024, source + (row - first) * 1024, 1024)) {
            row++;
            continue;
        }
        int begin = row++;
        while (row < end && (!raw_valid || memcmp(uploaded_raw + row * 1024, source + (row - first) * 1024, 1024)))
            row++;
        size_t bytes = (size_t)(row - begin) * 1024;
        glTexSubImage2D(
            GL_TEXTURE_2D, 0, 0, begin, 256, row - begin, GL_RGBA, GL_UNSIGNED_BYTE, source + (begin - first) * 1024);
        memcpy(uploaded_raw + begin * 1024, source + (begin - first) * 1024, bytes);
        gpu_bytes += bytes;
        gpu_calls++;
        if (rr_video_layers_profile_enabled()) {
            gpu_profile_bytes += bytes;
            gpu_profile_calls++;
        }
    }
}

bool rr_video_layers_compose_gpu(const rr_video_cpu_frame *frame)
{
    if (!enabled)
        return true;
    double cpu = rr_video_layers_cpu_time(), wall = rr_video_layers_profile_enabled() ? stamp(CLOCK_MONOTONIC) : 0;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, raw_tex);
    if (!geometry_prepare(frame))
        return false;
    if (!palette_valid || memcmp(uploaded_palette, frame->palette, sizeof uploaded_palette)) {
        glBindTexture(GL_TEXTURE_2D, palette_tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, frame->palette);
        memcpy(uploaded_palette, frame->palette, sizeof uploaded_palette);
        palette_valid = true;
        gpu_bytes += 1024;
        gpu_calls++;
        if (rr_video_layers_profile_enabled()) {
            gpu_profile_bytes += 1024;
            gpu_profile_calls++;
            gpu_profile_palette_bytes += 1024;
        }
    }
    if (!lut_valid || memcmp(last_lut, frame->lut, sizeof last_lut)) {
        glBindTexture(GL_TEXTURE_2D, lut_tex);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, frame->lut);
        memcpy(last_lut, frame->lut, sizeof last_lut);
        lut_valid = true;
        gpu_bytes += 1024;
        gpu_calls++;
        if (rr_video_layers_profile_enabled()) {
            gpu_profile_bytes += 1024;
            gpu_profile_calls++;
            gpu_profile_lut_bytes += 1024;
        }
    }
    if (rr_video_layers_profile_enabled()) {
        upload_cpu += rr_video_layers_cpu_time() - cpu;
        upload_wall += stamp(CLOCK_MONOTONIC) - wall;
        cpu = rr_video_layers_cpu_time();
        wall = stamp(CLOCK_MONOTONIC);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, p1b_output);
    glViewport(0, 0, W, H);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glUseProgram(gpu_program);
    const GLuint ts[] = {scene, raw_tex, palette_tex, lut_tex};
    for (int i = 0; i < 4; i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, ts[i]);
        glUniform1i(gpu_samplers[i], i);
    }
    glUniform2f(gpu_scroll, frame->scroll_x, frame->scroll_y);
    glUniform1i(gpu_shadow, hud_control3 == 1 ? 0 : frame->shadow_enabled);
    for (int i = 0; i < 3; i++)
        glUniform3f(gpu_mix[i], frame->multipliers[i][0], frame->multipliers[i][1], frame->multipliers[i][2]);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glActiveTexture(GL_TEXTURE0);
    if (rr_video_layers_profile_enabled()) {
        compose_cpu += rr_video_layers_cpu_time() - cpu;
        compose_wall += stamp(CLOCK_MONOTONIC) - wall;
        profile_frames++;
    }
    frames++;
    gpu_pictures++;
    return error("packed HUD compose");
}
static void gpu_close(void)
{
    if (!gpu_program)
        return;
    fprintf(stderr,
            "[GPU-HUD] pictures=%llu verified=%llu bytes=%llu calls=%llu profile_bytes=%llu profile_calls=%llu\n",
            (unsigned long long)gpu_pictures,
            (unsigned long long)gpu_verified,
            (unsigned long long)gpu_bytes,
            (unsigned long long)gpu_calls,
            (unsigned long long)gpu_profile_bytes,
            (unsigned long long)gpu_profile_calls);
    fprintf(stderr,
            "[GPU-HUD-TRANSFER] scope=profile glyph_bytes=%llu vertex_bytes=%llu palette_bytes=%llu lut_bytes=%llu\n",
            (unsigned long long)gpu_profile_glyph_bytes,
            (unsigned long long)gpu_profile_vertex_bytes,
            (unsigned long long)gpu_profile_palette_bytes,
            (unsigned long long)gpu_profile_lut_bytes);
    geometry_close();
    GLuint ts[] = {raw_tex, palette_tex};
    glDeleteTextures(2, ts);
    glDeleteProgram(gpu_program);
    gpu_program = 0;
    free(reference_pixels);
    free(gpu_pixels);
    reference_pixels = gpu_pixels = NULL;
}
#endif
