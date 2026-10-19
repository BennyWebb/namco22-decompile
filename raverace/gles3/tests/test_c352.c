/* Exact PCM/state transcript from the real shared mixer. No replacement mixer.
 * Build twice (fast path OFF/ON), then compare files on gles3 and ARM64. */
#include "c352.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static c352_t c;
static uint8_t rom[0x20000];
static FILE *file;
static unsigned operations;
static void u32(uint32_t v)
{
    uint8_t b[4] = {(uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v};
    assert(fwrite(b, 1, 4, file) == 4);
}
static void state(void)
{
    u32(operations++);
    for (unsigned j = 0; j < 32; j++) {
        c352_voice_t *v = &c.voice[j];
#define H(field) u32((uint32_t)v->field)
        H(pos);
        H(counter);
        H(sample);
        H(last_sample);
        H(vol_f);
        H(vol_r);
        for (int ch = 0; ch < 4; ch++)
            u32(v->curr_vol[ch]);
        H(freq);
        H(flags);
        H(wave_bank);
        H(wave_start);
        H(wave_end);
        H(wave_loop);
#undef H
    }
    for (unsigned i = 0; i < 256; i++)
        u32((uint32_t)c.mulawtab[i]);
    u32(c.random_state);
    u32(c.control);
    u32(c.rom_size);
    for (unsigned i = 0; i < 0x100; i++)
        u32(c352_read(&c, i));
    u32(c352_read(&c, 0x200));
    u32(c352_read(&c, 0x202));
    u32(c352_read(&c, 0x300));
}
static void write_reg(unsigned reg, uint16_t val, uint16_t mask)
{
    c352_write(&c, reg, val, mask);
    state();
}
static void generate(int n)
{
    int16_t pcm[4 * 257];
    assert(n <= 257);
    memset(pcm, 0x5a, sizeof pcm);
    c352_generate(&c, pcm, n);
    u32((uint32_t)n);
    for (int i = 0; i < (n > 0 ? n * 4 : 0); i++)
        u32((uint16_t)pcm[i]);
    for (int i = (n > 0 ? n * 4 : 0); i < 4 * 257; i++)
        assert((uint16_t)pcm[i] == 0x5a5a);
    state();
}
static void reset(void)
{
    c352_reset(&c);
    state();
}
static void voice(
    unsigned j, uint16_t flags, uint16_t vol, uint16_t freq, uint16_t bank, uint16_t start, uint16_t end, uint16_t loop)
{
    unsigned base = j * 8;
    write_reg(base, vol, 0xffff);
    write_reg(base + 1, (uint16_t)(vol ^ 0x7f32), 0xffff);
    write_reg(base + 2, freq, 0xffff);
    write_reg(base + 4, bank, 0xffff);
    write_reg(base + 5, start, 0xffff);
    write_reg(base + 6, end, 0xffff);
    write_reg(base + 7, loop, 0xffff);
    write_reg(base + 3, (uint16_t)(flags | C352_FLG_KEYON), 0xffff);
}
static void exec(void)
{
    write_reg(0x202, 0, 0xffff);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    file = fopen(argv[1], "wb");
    assert(file);
    for (unsigned i = 0; i < sizeof rom; i++)
        rom[i] = (uint8_t)(i * 73 + (i >> 7));
    c352_init(&c, rom, sizeof rom);
    state();
    generate(-1);
    generate(0);
    generate(1);
    generate(257); /* no voices */
    /* Inactive stale state, phase/volume/noise and pending commands remain frozen. */
    for (unsigned j = 0; j < 32; j++) {
        c.voice[j].pos = 100 + j;
        c.voice[j].counter = 0xabcd;
        c.voice[j].sample = -2345;
        c.voice[j].last_sample = 1234;
        memset(c.voice[j].curr_vol, 255, 4);
        c.voice[j].flags =
            C352_FLG_KEYON | C352_FLG_KEYOFF | C352_FLG_NOISE | C352_FLG_PHASEFL | C352_FLG_PHASEFR | C352_FLG_PHASERL;
    }
    c352_t before = c;
    int16_t silence[4 * 17];
    c352_generate(&c, silence, 17);
    assert(memcmp(&before, &c, sizeof c) == 0);
    for (unsigned i = 0; i < sizeof silence / sizeof silence[0]; i++)
        assert(silence[i] == 0);
    generate(17);
    exec();
    generate(2); /* both commands: on then off */
    reset();
    voice(0, 0, 0xff80, 0x7000, 0, 1, 31, 4);
    exec();
    for (int i = 0; i < 8; i++)
        generate(13);
    /* Ending this sample may still interpolate last_sample: no post-fetch skip. */
    reset();
    c.voice[0].flags = C352_FLG_BUSY;
    c.voice[0].freq = 2;
    c.voice[0].counter = 65535;
    c.voice[0].sample = 4096;
    c.voice[0].wave_end = 0;
    memset(c.voice[0].curr_vol, 255, 4);
    int16_t end_pcm[4];
    c352_generate(&c, end_pcm, 1);
    assert(!(c.voice[0].flags & C352_FLG_BUSY));
    assert(end_pcm[0] != 0);
    for (unsigned i = 0; i < 4; i++)
        u32((uint16_t)end_pcm[i]);
    state();
    generate(1);
    /* Loop, reverse, ping-pong, linked bank, mu-law, filter, noise, phase flags. */
    const uint16_t modes[] = {C352_FLG_LOOP,
                              C352_FLG_REVERSE,
                              C352_FLG_LOOP | C352_FLG_REVERSE,
                              C352_FLG_LINK | C352_FLG_LOOP,
                              C352_FLG_MULAW | C352_FLG_LOOP,
                              C352_FLG_FILTER | C352_FLG_LOOP,
                              C352_FLG_NOISE,
                              C352_FLG_PHASEFL | C352_FLG_PHASEFR | C352_FLG_PHASERL};
    for (unsigned m = 0; m < sizeof modes / sizeof modes[0]; m++) {
        reset();
        voice(3, modes[m], 0xfffe, 0xfedc, 0, 8, 16, 9);
        exec();
        generate(1);
        generate(256);
        generate(257);
    }
    reset(); /* 32 busy voices, shared noise PRNG ordering, overflowing mix */
    for (unsigned j = 0; j < 32; j++)
        voice(j, modes[j % 8], 0xffff, 0xffff, j & 1, 8, 16, 9);
    exec();
    for (int i = 0; i < 9; i++)
        generate(129);
    /* Active silence must still advance pos/counter and PRNG. */
    reset();
    voice(1, C352_FLG_NOISE, 0, 0x7000, 0, 1, 65535, 0);
    write_reg(9, 0, 0xffff);
    exec();
    uint16_t old_random = c.random_state;
    generate(97);
    assert(c.random_state != old_random);
    assert(c.voice[1].flags & C352_FLG_BUSY);
    reset();
    voice(1, C352_FLG_LOOP, 0, 0x7000, 0, 1, 20, 2);
    write_reg(9, 0, 0xffff);
    exec();
    generate(97);
    assert(c.voice[1].pos != 1);
    /* Volume ramp, zero pitch, register/exec writes at buffer boundaries. */
    write_reg(10, 0, 0xffff);
    generate(13);
    write_reg(8, 0xff00, 0xff00);
    generate(89);
    write_reg(10, 0x8123, 0xffff);
    generate(1);
    for (unsigned i = 0; i < 20; i++) {
        write_reg(11, C352_FLG_KEYOFF | C352_FLG_BUSY, 0xffff);
        generate(1);
        exec();
        generate(1);
        write_reg(11, C352_FLG_KEYON | C352_FLG_LOOP, 0xffff);
        exec();
        generate(2);
    }
    write_reg(0x202, 0, 0xff);
    generate(3); /* partial exec ignored */
    write_reg(0x200, 0xabcd, 0xffff);
    write_reg(0x200, 0x1200, 0xff00);
    generate(1);
    /* Out-of-ROM and 24-bit address masking. */
    reset();
    voice(0, 0, 0xffff, 0xffff, 0xff, 0xffff, 0, 0);
    exec();
    generate(16);
    /* Seeded arbitrary/masked register trace with variable buffer boundaries. */
    uint32_t seed = 0x12345678;
    for (unsigned i = 0; i < 3000; i++) {
        seed = seed * 1664525 + 1013904223;
        unsigned reg = (seed >> 16) & 0xff;
        write_reg(reg, (uint16_t)seed, (i % 3 == 0) ? 0xff : (i % 3 == 1) ? 0xff00 : 0xffff);
        if (i % 7 == 0)
            exec();
        if (i % 5 == 0)
            generate((int)(seed % 129));
    }
    assert(fclose(file) == 0);
    fprintf(
        stderr, "Real C352 PCM, every voice field/register, PRNG and table transcript: %u states passed\n", operations);
    return 0;
}
