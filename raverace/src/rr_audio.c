/*
 * rr_audio.c -- the engine's audio side: the C352 sample mixer (src/c352.c, a
 * port of MAME's c352.cpp), the wave ROM, and the output.
 *
 * Wave ROM layout is NOT file order (namcos22.cpp raverace):
 *   rv1wav0.10r @ 0x000000, rv1wav2.10n @ 0x100000,
 *   rv1wav1.10p @ 0x200000, rv1wav3.10l @ 0x300000.
 * Clock 24.576 MHz / 288 = 85333.3 samples/s, four outputs (front L/R, rear L/R;
 * this cabinet routes only 0/1 to the speakers). The video frame is 59.906 Hz, so
 * a frame is 1424.45 samples -- carried as a fraction so nothing drifts.
 *
 * RR_AUDIODUMP=<file.wav>: write the front pair as a 16-bit stereo WAV at the
 * chip's own rate, for comparing with MAME's -wavwrite.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rr_game.h"
#include "c352.h"
#include "rr_sound.h"
#if !defined(RR_HEADLESS) || defined(RR_GLES3)
#include <SDL.h>
#include <stdatomic.h>
#include <math.h>

/* ---- output (windowed runs only): 48 kHz stereo through SDL's callback ----
 * A lock-free single-producer (the emulation thread, per slice) / single-
 * consumer (SDL's audio thread) ring. The game is paced by the display, never
 * exactly the audio clock, so the resampler trims its ratio by up to +-0.5% to
 * hold the ring near TARGET -- no underrun crackle, no creeping latency. */
#define OUT_HZ 48000
#define RING 16384                  /* stereo frames, power of two */
#define TARGET (OUT_HZ * 60 / 1000) /* 60 ms */
static int16_t ring[RING * 2];
static atomic_uint r_head, r_tail; /* written by producer / consumer */
static SDL_AudioDeviceID dev;
static double resampler_ratio = 1, resampler_ratio_min = 1, resampler_ratio_max = 1;
double rr_audio_resampler_ratio(void)
{
    return resampler_ratio;
}
static double rs_pos; /* resampler: position in input samples */
static int16_t rs_prev[2];
static float g_volume = 1.0f;
/* OUTPUT GAIN. The C352's own output is quiet: measured against MAME's
 * -wavwrite over the same attract, ours is -40.1 dBFS RMS and MAME's -40.3,
 * race peaks -20.5 dBFS -- the cabinet's amplifier supplied the rest. So
 * the window/speaker output gets x6 (+15.6 dB: race peaks -3.5 dBFS, the limiter
 * touching 0.008% of samples), with a soft limiter above -6 dBFS so
 * a loud moment rounds off instead of clipping. The capture (RR_AUDIODUMP) is
 * the chip's own signal, before this. RR_OUTPUT_GAIN=<x> overrides (1 = raw). */
static double g_out_gain = 6.0;
static inline double soft_limit(double v)
{
    const double knee = 16384.0, room = 32767.0 - knee;
    double a = v < 0 ? -v : v;
    if (a <= knee)
        return v;
    a = knee + room * (1.0 - 1.0 / (1.0 + (a - knee) / room)); /* approaches 32767, never passes */
    return v < 0 ? -a : a;
}
static atomic_uint underruns, overruns, latency_resets;
static atomic_bool output_active;
static atomic_int primed;
static uint32_t per_sec_ur, cb_samples;
    /* RR_AUDIOLOG: underruns per second of output */ /* 0 until the ring first reaches TARGET: start-up silence is not
                                                         an underrun */

static void audio_cb(void *u, Uint8 *stream, int len)
{
    (void)u;
    int16_t *o = (int16_t *)stream;
    int n = len / 4;
    unsigned t = atomic_load_explicit(&r_tail, memory_order_relaxed);
    unsigned h = atomic_load_explicit(&r_head, memory_order_acquire);
    if ((int)(h - t) > 4 * TARGET) {
        t = h - TARGET;
        latency_resets++;
    } /* far behind: drop to the target once */
    if (!atomic_load(&primed)) {
        if ((int)(h - t) < TARGET) {
            memset(stream, 0, (size_t)len);
            return;
        }
        atomic_store(&primed, 1);
    }
    for (int i = 0; i < n; i++) {
        if (t == h) {
            o[2 * i] = o[2 * i + 1] = 0;
            underruns++;
            per_sec_ur++;
            continue;
        }
        o[2 * i] = ring[(t & (RING - 1)) * 2];
        o[2 * i + 1] = ring[(t & (RING - 1)) * 2 + 1];
        t++;
    }
    atomic_store_explicit(&r_tail, t, memory_order_release);
    cb_samples += (uint32_t)n;
    if (cb_samples >= OUT_HZ) {
        static int logon = -1;
        if (logon < 0)
            logon = getenv("RR_AUDIOLOG") != NULL;
        if (logon)
            fprintf(stderr,
                    "[AUDIO] second: %u underrun samples, ring %d, latency resets %u\n",
                    per_sec_ur,
                    (int)(h - t),
                    latency_resets);
        cb_samples -= OUT_HZ;
        per_sec_ur = 0;
    }
}

bool rr_audio_output_open(void)
{
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "[AUDIO] SDL audio: %s\n", SDL_GetError());
        return false;
    }
    SDL_AudioSpec want = {0}, have;
    want.freq = OUT_HZ;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.callback = audio_cb;
    want.samples = 512; /* RR_AUDIO_SAMPLES: 480 = exactly 10 ms, for SDL's disk driver (it sleeps whole ms) */
    if (getenv("RR_AUDIO_SAMPLES"))
        want.samples = (Uint16)atoi(getenv("RR_AUDIO_SAMPLES"));
    dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (!dev) {
        fprintf(stderr, "[AUDIO] no audio device: %s\n", SDL_GetError());
        return false;
    }
    if (getenv("RR_OUTPUT_GAIN")) {
        g_out_gain = atof(getenv("RR_OUTPUT_GAIN"));
        if (g_out_gain < 0)
            g_out_gain = 0;
    }
    atomic_store(&r_head, 0);
    atomic_store(&r_tail, 0);
    atomic_store(&primed, 0);
    atomic_store(&underruns, 0);
    atomic_store(&overruns, 0);
    atomic_store(&latency_resets, 0);
    rs_pos = 0;
    rs_prev[0] = rs_prev[1] = 0;
    atomic_store(&output_active, true);
    SDL_PauseAudioDevice(dev, 0);
    fprintf(stderr,
            "[AUDIO] driver=%s output %d Hz stereo, %d-sample buffer, gain x%.2f\n",
            SDL_GetCurrentAudioDriver(),
            have.freq,
            have.samples,
            g_out_gain);
    return true;
}
void rr_audio_output_status(rr_audio_output_stats *out)
{
    unsigned t = atomic_load_explicit(&r_tail, memory_order_acquire);
    unsigned h = atomic_load_explicit(&r_head, memory_order_acquire);
    unsigned queued = h - t; /* Both counters can advance during this observation. */
    *out = (rr_audio_output_stats){queued <= RING ? queued : 0,
                                   atomic_load(&underruns),
                                   atomic_load(&overruns),
                                   atomic_load(&latency_resets),
                                   atomic_load(&output_active)};
}
void rr_audio_set_volume(int percent)
{
    g_volume = (percent < 0 ? 0 : percent > 100 ? 100 : percent) / 100.0f;
}

static void push_out(const int16_t *in4, int n) /* in: n frames of the chip's 4 outputs, 85333 Hz */
{
    if (!dev)
        return;
    unsigned h = atomic_load_explicit(&r_head, memory_order_relaxed);
    unsigned t = atomic_load_explicit(&r_tail, memory_order_acquire);
    int fill = (int)(h - t);
    /* +-0.5% (inaudible) near the target; up to +-2% when far off, so a device or
     * display whose clock is well away from the board's still converges. */
    double err = (double)(TARGET - fill) / TARGET;
    double lim = (err > 1.0 || err < -1.0) ? 0.02 : 0.005;
    double adj = 1.0 + err * 0.005 * ((err > 1.0 || err < -1.0) ? 4 : 1);
    if (adj > 1.0 + lim)
        adj = 1.0 + lim;
    if (adj < 1.0 - lim)
        adj = 1.0 - lim;
    resampler_ratio = adj;
    if (adj < resampler_ratio_min)
        resampler_ratio_min = adj;
    if (adj > resampler_ratio_max)
        resampler_ratio_max = adj;
    const double step = 85333.333 / OUT_HZ / adj; /* input samples per output sample */
    while (rs_pos < n) {
        int i = (int)rs_pos;
        double f = rs_pos - i;
        for (int ch = 0; ch < 2; ch++) {
            double a = i == 0 ? rs_prev[ch] : in4[(i - 1) * 4 + ch], b = in4[i * 4 + ch];
            double v = soft_limit((a + (b - a) * f) * g_volume * g_out_gain);
            if (h - t < RING)
                ring[(h & (RING - 1)) * 2 + ch] = (int16_t)lrint(v);
        }
        if (h - t < RING)
            h++;
        else
            atomic_fetch_add(&overruns, 1);
        rs_pos += step;
    }
    rs_pos -= n;
    rs_prev[0] = in4[(n - 1) * 4];
    rs_prev[1] = in4[(n - 1) * 4 + 1];
    atomic_store_explicit(&r_head, h, memory_order_release);
}

#else
/* Keep the C352 clocks/status, discard only host output. */
static void push_out(const int16_t *samples, int n)
{
    (void)samples;
    (void)n;
}
void rr_audio_output_status(rr_audio_output_stats *out)
{
    memset(out, 0, sizeof *out);
}
#endif

static c352_t chip;
static uint8_t *wave;
static bool ready;
static FILE *dump;
static uint32_t dump_frames;
static uint64_t acc; /* samples owed, x (59906 * 16) */
extern uint32_t rr_frame;
static uint32_t checksum_from, activity_from;
static uint64_t checked_samples, checked_intervals;
static uint64_t pcm_hash, voice_hash;
#define FNV64_OFFSET UINT64_C(14695981039346656037)
static uint64_t hash_byte(uint64_t h, uint8_t b)
{
    return (h ^ b) * UINT64_C(1099511628211);
}
static uint64_t hash_u32(uint64_t h, uint32_t v)
{
    for (int b = 3; b >= 0; b--)
        h = hash_byte(h, (uint8_t)(v >> (b * 8)));
    return h;
}
static uint64_t hash_voices(uint64_t h)
{
    for (int j = 0; j < 32; j++) {
        const c352_voice_t *v = &chip.voice[j];
#define H(field) h = hash_u32(h, (uint32_t)v->field)
        H(pos);
        H(counter);
        H(sample);
        H(last_sample);
        H(vol_f);
        H(vol_r);
        for (int ch = 0; ch < 4; ch++)
            h = hash_u32(h, v->curr_vol[ch]);
        H(freq);
        H(flags);
        H(wave_bank);
        H(wave_start);
        H(wave_end);
        H(wave_loop);
#undef H
    }
    h = hash_u32(h, chip.random_state);
    h = hash_u32(h, chip.control);
    h = hash_u32(h, (uint32_t)(acc >> 32));
    return hash_u32(h, (uint32_t)acc);
}
bool rr_audio_diagnostics(uint32_t checksum_first, uint32_t activity_first)
{
    if (activity_first && !c352_profile_available()) {
        fprintf(stderr, "[C352] activity requires RR_C352_VOICE_COUNTERS=ON\n");
        return false;
    }
    checksum_from = checksum_first;
    activity_from = activity_first;
    checked_samples = checked_intervals = 0;
    pcm_hash = voice_hash = FNV64_OFFSET;
    c352_profile_reset();
    c352_profile_enable(false);
    return true;
}
static void activity_enable(void)
{
    if (activity_from)
        c352_profile_enable(rr_frame + 1 >= activity_from);
}
#define RATE_NUM 85333333ull                 /* samples/s x 1000 */
#define SLICE_DEN (59906ull * RR_SND_SLICES) /* slices/s x 1000 */

static void wav_header(FILE *f, uint32_t frames)
{
    uint32_t rate = 85333, bytes = frames * 4;
    uint8_t h[44] = {'R', 'I', 'F', 'F'};
    uint32_t v;
#define P32(o, x)                                                                                                      \
    do {                                                                                                               \
        v = (x);                                                                                                       \
        h[o] = v;                                                                                                      \
        h[o + 1] = v >> 8;                                                                                             \
        h[o + 2] = v >> 16;                                                                                            \
        h[o + 3] = v >> 24;                                                                                            \
    } while (0)
    P32(4, 36 + bytes);
    memcpy(h + 8, "WAVEfmt ", 8);
    P32(16, 16);
    h[20] = 1;
    h[22] = 2;
    P32(24, rate);
    P32(28, rate * 4);
    h[32] = 4;
    h[34] = 16;
    memcpy(h + 36, "data", 4);
    P32(40, bytes);
    fseek(f, 0, SEEK_SET);
    fwrite(h, 1, 44, f);
}

bool rr_audio_init(const char *dir)
{
    const rr_wave_t *w = g_rr_game->wav;
    wave = calloc(1, 0x400000);
    for (int i = 0; i < 4; i++) {
        char p[1024];
        snprintf(p, sizeof p, "%s/%s", dir, w[i].file);
        FILE *f = fopen(p, "rb");
        if (!f || fread(wave + w[i].at, 1, 0x100000, f) != 0x100000) {
            fprintf(stderr, "[AUDIO] cannot read %s -- silent\n", p);
            if (f)
                fclose(f);
            return false;
        }
        fclose(f);
    }
    c352_init(&chip, wave, 0x400000);
    c352_reset(&chip);
    const char *d = getenv("RR_AUDIODUMP");
    if (d && (dump = fopen(d, "wb"))) {
        wav_header(dump, 0);
        fprintf(stderr, "[AUDIO] dumping to %s\n", d);
    }
    ready = true;
    return true;
}

void rr_audio_c352_write(unsigned word, uint16_t v)
{
    if (ready) {
        activity_enable();
        c352_write(&chip, word, v, 0xffff);
    }
}
uint16_t rr_audio_c352_read(unsigned word)
{
    return ready ? c352_read(&chip, word) : 0;
}

void rr_audio_slice(void)
{
    if (!ready)
        return;
    activity_enable();
    acc += RATE_NUM;
    int n = (int)(acc / SLICE_DEN);
    acc -= (uint64_t)n * SLICE_DEN;
    static int16_t buf[4 * 256];
    while (n > 0) {
        int k = n > 256 ? 256 : n;
        c352_generate(&chip, buf, k);
        if (checksum_from && rr_frame + 1 >= checksum_from) {
            for (int i = 0; i < k * 4; i++) {
                uint16_t sample = (uint16_t)buf[i];
                pcm_hash = hash_byte(pcm_hash, (uint8_t)sample);
                pcm_hash = hash_byte(pcm_hash, (uint8_t)(sample >> 8));
            }
            voice_hash = hash_u32(voice_hash, (uint32_t)k);
            voice_hash = hash_voices(voice_hash);
            checked_samples += (unsigned)k;
            checked_intervals++;
        }
        push_out(buf, k);
        if (dump) {
            for (int i = 0; i < k; i++) {
                fwrite(&buf[i * 4], 2, 2, dump);
            }
            dump_frames += (uint32_t)k;
        }
        n -= k;
    }
}

void rr_audio_close(void)
{
    fprintf(stderr,
            "[C352] inactive_fastpath=%s counters=%s\n",
            c352_inactive_fastpath_enabled() ? "on" : "off",
            c352_profile_available() ? "on" : "off");
    if (checksum_from)
        fprintf(stderr,
                "[AUDIO-CHECK] from=%u samples=%llu intervals=%llu pcm_fnv1a64=%016llx voice_fnv1a64=%016llx "
                "final_fnv1a64=%016llx acc=%llu\n",
                checksum_from,
                (unsigned long long)checked_samples,
                (unsigned long long)checked_intervals,
                (unsigned long long)pcm_hash,
                (unsigned long long)voice_hash,
                (unsigned long long)hash_voices(FNV64_OFFSET),
                (unsigned long long)acc);
    if (activity_from) {
        c352_profile_t p;
        c352_profile_get(&p);
        fprintf(stderr,
                "[C352-ACTIVITY] from=%u intervals=%llu samples=%llu iterations=%llu active=%llu inactive=%llu "
                "mix_iterations=%llu zero_source_iterations=%llu active_zero_source=%llu activations=%llu "
                "deactivations=%llu keyon=%llu keyoff=%llu natural_ends=%llu\n",
                activity_from,
                (unsigned long long)p.intervals,
                (unsigned long long)p.samples,
                (unsigned long long)(p.samples * 32),
                (unsigned long long)p.active_iterations,
                (unsigned long long)(p.samples * 32 - p.active_iterations),
                (unsigned long long)(c352_inactive_fastpath_enabled() ? p.active_iterations : p.samples * 32),
                (unsigned long long)(p.samples * 32 - p.active_iterations + p.active_zero_source),
                (unsigned long long)p.active_zero_source,
                (unsigned long long)p.activations,
                (unsigned long long)p.deactivations,
                (unsigned long long)p.keyon,
                (unsigned long long)p.keyoff,
                (unsigned long long)p.natural_ends);
        fprintf(stderr, "[C352-HIST] entry_active=");
        for (int i = 0; i <= 32; i++)
            fprintf(stderr, "%s%llu", i ? "," : "", (unsigned long long)p.entry_active[i]);
        fprintf(stderr, " sample_active=");
        for (int i = 0; i <= 32; i++)
            fprintf(stderr, "%s%llu", i ? "," : "", (unsigned long long)p.sample_active[i]);
        fprintf(stderr, "\n");
    }
#if !defined(RR_HEADLESS) || defined(RR_GLES3)
    if (dev) {
        fprintf(stderr,
                "[AUDIO-RESAMPLER] resampler_ratio_min=%.9f max=%.9f final=%.9f\n",
                resampler_ratio_min,
                resampler_ratio_max,
                resampler_ratio);
        SDL_CloseAudioDevice(dev);
        atomic_store(&output_active, false);
        dev = 0;
        rr_audio_output_stats out;
        rr_audio_output_status(&out);
        fprintf(stderr,
                "[AUDIO-OUTPUT] underrun_samples=%u overrun_samples=%u latency_resets=%u queued_frames=%u\n",
                out.underruns,
                out.overruns,
                out.resets,
                out.queued);
    }
#endif
    if (dump) {
        wav_header(dump, dump_frames);
        fclose(dump);
        dump = NULL;
        fprintf(stderr, "[AUDIO] wrote %u samples (%.1f s)\n", dump_frames, dump_frames / 85333.0);
    }
}

void rr_audio_checkpoint(FILE *out, unsigned frame)
{
    fprintf(out,
            "{\"audio_frame\":%u,\"samples\":%llu,\"intervals\":%llu,\"pcm\":\"%016llx\",\"voices\":\"%016llx\","
            "\"acc\":%llu}\n",
            frame,
            (unsigned long long)checked_samples,
            (unsigned long long)checked_intervals,
            (unsigned long long)pcm_hash,
            (unsigned long long)voice_hash,
            (unsigned long long)acc);
}
