/* Included only by rr_main.c's scheduler-test build. No alternate scheduler. */
#include <errno.h>
#include <inttypes.h>
#include <limits.h>

uint64_t rr_headless_instructions;
uint32_t rr_headless_pc;
extern uint64_t rr_headless_dsp_steps(void);
extern bool rr_headless_dsp_fault(void);
extern uint64_t rr_headless_sound_cycles(void);
extern bool rr_headless_sound_fault(void);

static uint32_t state_hash(const uint8_t *bytes, size_t n)
{
    uint32_t h = 2166136261u; /* FNV-1a, byte-defined on every host */
    for (size_t i = 0; i < n; i++) h = (h ^ bytes[i]) * 16777619u;
    return h;
}

static void rr_headless_summary(void)
{
    uint8_t poly[RR_POLY_WORDS * 4];
    for (size_t i = 0; i < RR_POLY_WORDS; i++) {
        uint32_t w = g_rr.poly[i];
        for (int b = 0; b < 4; b++) poly[i * 4 + b] = (uint8_t)(w >> (24 - b * 8));
    }
    printf("{\"schema\":1,\"frames\":%u,\"slices\":%u,\"instructions_68k\":%" PRIu64
           ",\"last_instruction_pc\":\"%08x\",\"sr\":\"%04x\",\"d\":[",
           frame, slice, rr_headless_instructions, rr_headless_pc, rr_get_sr());
    for (int i = 0; i < 8; i++) printf("%s\"%08x\"", i ? "," : "", (uint32_t)RG4(i * 4));
    printf("],\"a\":[");
    for (int i = 0; i < 8; i++) printf("%s\"%08x\"", i ? "," : "", (uint32_t)RG4(0x20 + i * 4));
    printf("],\"fnv1a32\":{\"register_bytes\":\"%08x\",\"wram\":\"%08x\","
           "\"shared\":\"%08x\",\"poly_be\":\"%08x\",\"text\":\"%08x\","
           "\"palette\":\"%08x\",\"eeprom\":\"%08x\"},"
           "\"traps\":%u,\"unmapped\":%u,\"romwrites\":%u,\"irqs\":[",
           state_hash(R, sizeof R), state_hash(g_rr.wram, sizeof g_rr.wram),
           state_hash(g_rr.shared, sizeof g_rr.shared), state_hash(poly, sizeof poly),
           state_hash(g_rr.text, sizeof g_rr.text), state_hash(g_rr.pal, sizeof g_rr.pal),
           state_hash(g_rr.eeprom, sizeof g_rr.eeprom), rr_n_traps, g_rr.n_unmapped, g_rr.n_romwrite);
    for (int i = 1; i <= 7; i++) printf("%s%u", i > 1 ? "," : "", rr_n_irq[i]);
    printf("],\"dsp_steps\":%" PRIu64 ",\"sound_cycles\":%" PRIu64
           ",\"dsp_fault\":%s,\"sound_fault\":%s}\n",
           rr_headless_dsp_steps(), rr_headless_sound_cycles(),
           rr_headless_dsp_fault() ? "true" : "false",
           rr_headless_sound_fault() ? "true" : "false");
    fflush(stdout);
}

static void headless_fatal(const char *why)
{
    fprintf(stderr, "[HEADLESS] fatal before requested frame limit: %s\n", why);
    rr_headless_summary();
}

static bool headless_number(const char *text, uint32_t max, uint32_t *value)
{
    if (!*text) return false;
    for (const char *p = text; *p; p++) if (*p < '0' || *p > '9') return false;
    char *end; errno = 0;
    unsigned long v = strtoul(text, &end, 10);
    if (errno || *end || v > max) return false;
    *value = (uint32_t)v;
    return true;
}

static int rr_headless_main(int argc, char **argv)
{
    const char *rom_dir = "extracted";
#ifdef RR_GLES2
    if (!rr_gles2_options(&argc, argv)) return 2;
#endif
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--help")) {
#ifdef RR_GLES2
            fprintf(stderr, "Usage: rr_gles2 [ROM_DIR] [--frames N] [--coin F] [--gas F] [--steer F:V] [--dump DIR]\n"
                            "                [--unpaced] [--capture FILE.ppm --capture-frame N]\n"
                            "Quit: controller START+SELECT (SDL START+BACK), Escape, or frame limit.\n");
#else
            fprintf(stderr, "Usage: rr_headless [ROM_DIR] [--frames N] [--coin F] [--gas F] [--steer F:V] [--dump DIR]\n");
#endif
            return 0;
        }
        if (!strcmp(argv[i], "--dump") && i + 1 < argc) { dump_dir = argv[++i]; continue; }
        uint32_t v;
        if ((!strcmp(argv[i], "--frames") || !strcmp(argv[i], "--coin") || !strcmp(argv[i], "--gas")) && i + 1 < argc) {
            const char *option = argv[i++];
            if (!headless_number(argv[i], INT32_MAX, &v) || v == 0) goto invalid;
            if (!strcmp(option, "--frames")) max_frames = v;
            else if (!strcmp(option, "--gas")) test_gas = (int32_t)v;
            else {
                if (n_test_coins == 8) goto invalid;
                test_coins[n_test_coins++] = (int32_t)v;
            }
        } else if (!strcmp(argv[i], "--steer") && i + 1 < argc) {
            unsigned at, wheel; char tail;
            if (n_steer == 8 || sscanf(argv[++i], "%u:%u%c", &at, &wheel, &tail) != 2 ||
                at == 0 || at > INT32_MAX || wheel > 4095) goto invalid;
            test_steer[n_steer].at = (int32_t)at;
            test_steer[n_steer++].value = (int32_t)wheel;
        } else if (argv[i][0] == '-') goto invalid;
        else rom_dir = argv[i];
        continue;
invalid:
        fprintf(stderr, "Invalid/missing argument near %s; use --help\n", argv[i]);
        return 2;
    }
    /* Isolate this test from desktop debug/network/config environment. */
    extern char **environ;
    for (char **e = environ; *e; ) {
        if (!strncmp(*e, "RR_", 3) || !strncmp(*e, "SND_", 4)) {
            char *name = strdup(*e); char *eq = strchr(name, '=');
            if (eq) *eq = 0;
            unsetenv(name); free(name); e = environ;
        } else e++;
    }
    setenv("RR_RD", "0", 1); /* translated baseline, no readable substitutions */
    /* Require the same EEPROM seed rather than silently starting with zeros. */
    char seed[1024];
    snprintf(seed, sizeof seed, "%s/%s", rom_dir, g_rr_game->eeprom);
    FILE *fp = fopen(seed, "rb");
    if (!fp) { fprintf(stderr, "[HEADLESS] missing EEPROM seed: %s\n", seed); return 2; }
    bool seed_ok = fseek(fp, 0, SEEK_END) == 0 && ftell(fp) == RR_EEPROM_SIZE;
    fclose(fp);
    if (!seed_ok) { fprintf(stderr, "[HEADLESS] invalid EEPROM seed: %s\n", seed); return 2; }
    if (!rr_load_program(rom_dir) || !rr_audio_init(rom_dir) ||
        !rr_sound_init(rom_dir) || !rr_hw_init(rom_dir)) return 2;
#ifdef RR_GLES2
    if (!rr_gl_init(rom_dir) || !rr_gles2_open()) return 2;
#endif
    memset(R, 0, sizeof R);
    RS4(REG_SP, rr_read(0, 4));
    set_sr(0x2700);
    rr_budget = polls_per_frame / SLICES;
    rr_call_push(0xFFFFFFFEu);
    extern void rd_init(void);
    rd_init();
    rr_fatal_hook = headless_fatal;
    fprintf(stderr, "[HEADLESS] translated baseline; stop after %u frames\n", max_frames);
    g_rr_game->entry();
    fprintf(stderr, "[HEADLESS] reset entry unexpectedly returned\n");
    return 1;
}
