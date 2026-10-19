/* Optional host diagnostics. No chip state reads. All GL/statistics storage is
 * owned by the presentation thread; only the selected mode crosses threads. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "rr_overlay.h"
#include "rr_sound.h"
#include "rr_font.h"
#include "eng_gl.h"
#ifdef RR_THREADED_PRESENTATION
#include "rr_presentation.h"
#endif
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdint.h>
#define WINDOW 300
#define WARMUP 120
#define MAX_VERTICES (6 * 2048)
static atomic_int mode;
static double stamps[WINDOW + 1], fps, low, milliseconds, hz, age_ms;
static unsigned count, cursor, seen, previous_sim, dropped, queue;
static double last_sample, previous_sim_time;
static unsigned long long total[5], idle[5], thread_ticks[2];
static double cpu[5], thread_cpu[2];
static long frequency = -1, gpu_frequency = -1, temperature = -1, gpu_temperature = -1, cpu_cooling = -1,
            gpu_cooling = -1;
static long rss, available, memory_total;
static bool cpu_valid[5], thread_valid[2];
static GLuint program, texture, buffer;
static GLint color;
static float vertices[MAX_VERTICES][4];
static unsigned used;
bool rr_overlay_mode(const char *value)
{
    int m = !strcmp(value, "off") ? 0 : !strcmp(value, "compact") ? 1 : !strcmp(value, "expanded") ? 2 : -1;
    if (m < 0)
        return false;
    atomic_store(&mode, m);
    return true;
}
void rr_overlay_toggle(void)
{
    atomic_store(&mode, (atomic_load(&mode) + 1) % 3);
}
static long number(const char *path)
{
    FILE *f = fopen(path, "r");
    long n = -1;
    if (f) {
        if (fscanf(f, "%ld", &n) != 1)
            n = -1;
        fclose(f);
    }
    return n;
}
static void process_memory(void)
{
    char line[256];
    FILE *f = fopen("/proc/self/status", "r");
    if (f) {
        while (fgets(line, sizeof line, f))
            if (sscanf(line, "VmRSS: %ld kB", &rss) == 1)
                break;
        fclose(f);
    }
    f = fopen("/proc/meminfo", "r");
    if (f) {
        while (fgets(line, sizeof line, f)) {
            if (sscanf(line, "MemAvailable: %ld kB", &available) == 1)
                continue;
            if (sscanf(line, "MemTotal: %ld kB", &memory_total) == 1)
                continue;
        }
        fclose(f);
    }
}
static void sample(double t, unsigned simulation_frame)
{
    double elapsed = t - last_sample;
    if (elapsed < 1)
        return;
    FILE *f = fopen("/proc/stat", "r");
    char line[512], name[16];
    if (f) {
        while (fgets(line, sizeof line, f)) {
            unsigned long long a, b, c, d, e, g, h, j;
            if (sscanf(line, "%15s %llu %llu %llu %llu %llu %llu %llu %llu", name, &a, &b, &c, &d, &e, &g, &h, &j) != 9)
                continue;
            int index = !strcmp(name, "cpu") ? 0 : !strncmp(name, "cpu", 3) ? atoi(name + 3) + 1 : -1;
            if (index < 0 || index > 4)
                continue;
            unsigned long long sum = a + b + c + d + e + g + h + j, quiet = d + e;
            if (total[index] && sum > total[index]) {
                cpu[index] = 100. * (1. - (double)(quiet - idle[index]) / (sum - total[index]));
                cpu_valid[index] = true;
            }
            total[index] = sum;
            idle[index] = quiet;
        }
        fclose(f);
    }
    long tids[2] = {getpid(), syscall(SYS_gettid)};
    for (int i = 0; i < 2; i++) {
        if (i == 1 && tids[0] == tids[1]) {
            thread_valid[1] = false;
            continue;
        }
        char path[96];
        snprintf(path, sizeof path, "/proc/self/task/%ld/stat", tids[i]);
        f = fopen(path, "r");
        if (!f)
            continue;
        if (fgets(line, sizeof line, f)) {
            char *p = strrchr(line, ')');
            unsigned long long ticks = 0;
            if (p) {
                char *save, *tok = strtok_r(p + 2, " ", &save);
                int field = 3;
                for (; tok; tok = strtok_r(NULL, " ", &save), field++)
                    if (field == 14 || field == 15)
                        ticks += strtoull(tok, NULL, 10);
                if (thread_ticks[i] && elapsed > 0) {
                    thread_cpu[i] = 100. * (ticks - thread_ticks[i]) / sysconf(_SC_CLK_TCK) / elapsed;
                    thread_valid[i] = true;
                }
                thread_ticks[i] = ticks;
            }
        }
        fclose(f);
    }
    if (previous_sim_time > 0)
        hz = (simulation_frame - previous_sim) / (t - previous_sim_time);
    previous_sim = simulation_frame;
    previous_sim_time = t;
    last_sample = t;
    frequency = number("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
    gpu_frequency = number("/sys/class/devfreq/gpu/cur_freq");
    temperature = gpu_temperature = -1;
    for (int i = 0; i < 8; i++) {
        char path[128], type[64];
        snprintf(path, sizeof path, "/sys/class/thermal/thermal_zone%d/type", i);
        f = fopen(path, "r");
        if (!f)
            continue;
        bool found = fgets(type, sizeof type, f) != NULL;
        fclose(f);
        if (found && (strstr(type, "cpu") || strstr(type, "soc") || strstr(type, "gpu"))) {
            snprintf(path, sizeof path, "/sys/class/thermal/thermal_zone%d/temp", i);
            if (strstr(type, "gpu"))
                gpu_temperature = number(path);
            else
                temperature = number(path);
        }
    }
    cpu_cooling = gpu_cooling = -1;
    for (int i = 0; i < 8; i++) {
        char path[128], type[64];
        snprintf(path, sizeof path, "/sys/class/thermal/cooling_device%d/type", i);
        f = fopen(path, "r");
        if (!f)
            continue;
        bool found = fgets(type, sizeof type, f) != NULL;
        fclose(f);
        if (found) {
            snprintf(path, sizeof path, "/sys/class/thermal/cooling_device%d/cur_state", i);
            if (strstr(type, "cpufreq"))
                cpu_cooling = number(path);
            else if (strstr(type, "devfreq"))
                gpu_cooling = number(path);
        }
    }
    process_memory();
    rr_audio_output_stats audio;
    rr_audio_output_status(&audio);
    fprintf(stderr,
            "[OVERLAY] fps=%.3f mean_ms=%.3f low1_fps=%.3f samples=%u sim_hz=%.3f dropped=%u queue=%u age_ms=%.3f "
            "cpu_percent=%.1f/%.1f/%.1f/%.1f/%.1f thread_percent=%.1f/%.1f rss_kib=%ld available_kib=%ld "
            "audio_underrun_samples=%u\n",
            fps,
            milliseconds,
            low,
            count ? count - 1 : 0,
            hz,
            dropped,
            queue,
            age_ms,
            cpu[0],
            cpu[1],
            cpu[2],
            cpu[3],
            cpu[4],
            thread_cpu[0],
            thread_cpu[1],
            rss,
            available,
            audio.underruns);
}
static int descending(const void *a, const void *b)
{
    double x = *(const double *)a, y = *(const double *)b;
    return (x < y) - (x > y);
}
void rr_overlay_present(uint32_t frame, double timestamp)
{
    if (!atomic_load(&mode)) {
        if (count || seen) {
            count = cursor = seen = 0;
            last_sample = previous_sim_time = 0;
            fps = low = milliseconds = hz = 0;
            memset(total, 0, sizeof total);
            memset(idle, 0, sizeof idle);
            memset(thread_ticks, 0, sizeof thread_ticks);
            memset(cpu_valid, 0, sizeof cpu_valid);
            memset(thread_valid, 0, sizeof thread_valid);
        }
        return;
    }
    double t = timestamp;
    unsigned sim_frame = frame;
#ifdef RR_THREADED_PRESENTATION
    if (rr_presentation_enabled())
        rr_presentation_overlay(&sim_frame, &dropped, &queue, &age_ms, t);
#endif
    if (seen++ < WARMUP) {
        last_sample = 0;
        return;
    }
    stamps[cursor] = t;
    cursor = (cursor + 1) % (WINDOW + 1);
    if (count < WINDOW + 1)
        count++;
    if (count > 1) {
        unsigned first = (cursor + WINDOW + 1 - count) % (WINDOW + 1);
        double span = t - stamps[first];
        milliseconds = span * 1000 / (count - 1);
        fps = (count - 1) / span;
    }
    if (t - last_sample >= 1) {
        if (count > 1) {
            double intervals[WINDOW];
            unsigned n = count - 1, first = (cursor + WINDOW + 1 - count) % (WINDOW + 1);
            for (unsigned i = 0; i < n; i++)
                intervals[i] = stamps[(first + i + 1) % (WINDOW + 1)] - stamps[(first + i) % (WINDOW + 1)];
            qsort(intervals, n, sizeof *intervals, descending);
            unsigned worst = (n + 99) / 100;
            double sum = 0;
            for (unsigned i = 0; i < worst; i++)
                sum += intervals[i];
            low = worst / sum;
        }
        sample(t, sim_frame);
    }
}
static GLuint compile(GLenum kind, const char *source)
{
    GLuint s = glCreateShader(kind);
    glShaderSource(s, 1, &source, NULL);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof log, NULL, log);
        fprintf(stderr, "[OVERLAY] shader: %s\n", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}
static bool open_gl(void)
{
    const char *vs =
        "attribute vec4 vertex;varying vec2 uv;void main(){gl_Position=vec4(vertex.xy,0.,1.);uv=vertex.zw;}";
    const char *fs = "precision mediump float;varying vec2 uv;uniform sampler2D font;uniform vec4 color;void "
                     "main(){gl_FragColor=vec4(color.rgb,color.a*texture2D(font,uv).a);}";
    GLuint v = compile(GL_VERTEX_SHADER, vs), f = compile(GL_FRAGMENT_SHADER, fs);
    if (!v || !f)
        return false;
    program = glCreateProgram();
    glAttachShader(program, v);
    glAttachShader(program, f);
    glBindAttribLocation(program, 0, "vertex");
    glLinkProgram(program);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok;
    glGetProgramiv(program, GL_LINK_STATUS, &ok);
    if (!ok) {
        fprintf(stderr, "[OVERLAY] link failed\n");
        return false;
    }
    color = glGetUniformLocation(program, "color");
    unsigned char atlas[128 * 128] = {0};
    for (int c = 0; c < 95; c++)
        for (int y = 0; y < 16; y++)
            for (int x = 0; x < 8; x++)
                atlas[((c / 16) * 16 + y) * 128 + (c % 16) * 8 + x] = (rr_font8x16[c][y] & (128 >> x)) ? 255 : 0;
    atlas[127 * 128 + 127] = 255;
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, 128, 128, 0, GL_ALPHA, GL_UNSIGNED_BYTE, atlas);
    glUseProgram(program);
    glUniform1i(glGetUniformLocation(program, "font"), 0);
    glGenBuffers(1, &buffer);
    return true;
}
static void quad(float x, float y, float w, float h, float u, float v, float uw, float vh)
{
    if (used + 6 > MAX_VERTICES)
        return;
    float a[4][4] = {{x / 320 - 1, 1 - y / 240, u, v},
                     {(x + w) / 320 - 1, 1 - y / 240, u + uw, v},
                     {(x + w) / 320 - 1, 1 - (y + h) / 240, u + uw, v + vh},
                     {x / 320 - 1, 1 - (y + h) / 240, u, v + vh}};
    const int order[] = {0, 1, 2, 0, 2, 3};
    for (int i = 0; i < 6; i++)
        memcpy(vertices[used++], a[order[i]], sizeof a[0]);
}
static void text(int row, const char *line)
{
    for (unsigned i = 0; line[i] && i < 74; i++) {
        unsigned c = (unsigned char)line[i];
        if (c < 32 || c > 126)
            c = '?';
        c -= 32;
        quad(10 + i * 8, 8 + row * 16, 8, 16, (c % 16) * 8 / 128.f, (c / 16) * 16 / 128.f, 8 / 128.f, 16 / 128.f);
    }
}
static void upload(void)
{
    glBindBuffer(GL_ARRAY_BUFFER, buffer);
    glBufferData(GL_ARRAY_BUFFER, used * sizeof vertices[0], vertices, GL_STREAM_DRAW);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glDisableVertexAttribArray(2);
}
void rr_overlay_draw(void)
{
    int m = atomic_load(&mode);
    if (!m)
        return;
    if (!program && !open_gl()) {
        atomic_store(&mode, 0);
        return;
    }
    char line[256];
    rr_audio_output_stats audio;
    rr_audio_output_status(&audio);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, 640, 480);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(program);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    used = 0;
    quad(5, 5, m == 2 ? 625 : 560, m == 2 ? 149 : 53, 127.5f / 128, 127.5f / 128, 0, 0);
    if (count < 2)
        text(0, "WARMUP: FIRST 120 PRESENTATIONS EXCLUDED");
    else {
        snprintf(line, sizeof line, "FPS %.1f  %.2fMS  1%% LOW %.1f [%u]", fps, milliseconds, low, count - 1);
        text(0, line);
    }
    snprintf(line, sizeof line, "SIM %.2fHZ DROP %u QUEUE %u AGE %.1fMS", hz, dropped, queue, age_ms);
    text(1, line);
    snprintf(line,
             sizeof line,
             "AUDIO %s BUFFER %.1fMS UNDERRUN %u",
             audio.active ? "ON" : "OFF",
             audio.queued / 48.,
             audio.underruns);
    text(2, line);
    if (m == 2) {
        if (!cpu_valid[0])
            text(3, "CPU N/A CORES N/A / N/A / N/A / N/A");
        else {
            snprintf(
                line, sizeof line, "CPU %.0f%%  CORES %.0f/%.0f/%.0f/%.0f%%", cpu[0], cpu[1], cpu[2], cpu[3], cpu[4]);
            text(3, line);
        }
        char simulation[24] = "N/A", presentation[24] = "N/A", cf[24] = "N/A", gf[24] = "N/A", ct[24] = "N/A",
             gt[24] = "N/A", cc[24] = "N/A", gc[24] = "N/A";
        if (thread_valid[0])
            snprintf(simulation, sizeof simulation, "%.0f%%", thread_cpu[0]);
        if (thread_valid[1])
            snprintf(presentation, sizeof presentation, "%.0f%%", thread_cpu[1]);
        snprintf(line, sizeof line, "THREAD CPU SIM %s PRESENT %s", simulation, presentation);
        text(4, line);
        if (frequency >= 0)
            snprintf(cf, sizeof cf, "%ldMHZ", frequency / 1000);
        if (gpu_frequency >= 0)
            snprintf(gf, sizeof gf, "%ldMHZ", gpu_frequency / 1000000);
        if (temperature >= 0)
            snprintf(ct, sizeof ct, "%ldC", temperature / 1000);
        if (gpu_temperature >= 0)
            snprintf(gt, sizeof gt, "%ldC", gpu_temperature / 1000);
        if (cpu_cooling >= 0)
            snprintf(cc, sizeof cc, "%ld", cpu_cooling);
        if (gpu_cooling >= 0)
            snprintf(gc, sizeof gc, "%ld", gpu_cooling);
        snprintf(line, sizeof line, "CPU FREQ %s GPU FREQ %s", cf, gf);
        text(5, line);
        snprintf(line, sizeof line, "GPU UTIL N/A TEMP %s COOLING CPU %s DEVFREQ %s", gt, cc, gc);
        text(6, line);
        if (!memory_total)
            text(7, "RSS N/A AVAILABLE N/A CPU TEMP N/A");
        else {
            snprintf(line,
                     sizeof line,
                     "RSS %ldM AVAILABLE %ld/%ldM CPU TEMP %s",
                     rss / 1024,
                     available / 1024,
                     memory_total / 1024,
                     ct);
            text(7, line);
        }
        text(8, "X / F3: OFF > COMPACT > EXPANDED");
    }
    upload();
    glUniform4f(color, 0, 0, 0, .8f);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glUniform4f(color, 1, 1, 1, 1);
    glDrawArrays(GL_TRIANGLES, 6, used - 6);
    glDisable(GL_BLEND);
}
void rr_overlay_close(void)
{
    if (program) {
        glDeleteProgram(program);
        glDeleteTextures(1, &texture);
        glDeleteBuffers(1, &buffer);
        program = texture = buffer = 0;
    }
}
