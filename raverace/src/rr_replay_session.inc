/* Simulation-owner replay session. Existing F records remain the input format.
 * Recording starts at reset; no mid-race save-state or wall-clock input timing. */
#include "rr_input_stream.h"
static const char *input_record_path, *input_replay_path, *checkpoint_path;
static FILE *checkpoint_file, *replay_timing_file;
static const char *replay_timing_path;
static char replay_timing_buffer[16384];
static uint32_t replay_end, completed_replay_frame;
static bool replay_session, replay_session_ok = true, replay_checks = true;
static uint64_t replay_hash(const void *data, size_t n)
{
    const uint8_t *p = data;
    uint64_t h = UINT64_C(14695981039346656037);
    while (n--)
        h = (h ^ *p++) * UINT64_C(1099511628211);
    return h;
}
static bool replay_rom_hash(FILE *manifest, const char *dir, const char *name)
{
    if (!name)
        return true;
    char path[2048];
    if (snprintf(path, sizeof path, "%s/%s", dir, name) >= (int)sizeof path)
        return false;
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror(path);
        return false;
    }
    uint8_t buf[16384];
    size_t n, total = 0;
    uint64_t h = UINT64_C(14695981039346656037);
    while ((n = fread(buf, 1, sizeof buf, f))) {
        total += n;
        for (size_t i = 0; i < n; i++)
            h = (h ^ buf[i]) * UINT64_C(1099511628211);
    }
    bool ok = !ferror(f);
    fclose(f);
    return ok && fprintf(manifest, "ROM %s %zu %016" PRIx64 "\n", name, total, h) > 0;
}
static void replay_session_close(void)
{
    rr_input_stream_close();
    if (replay_timing_file) {
        if (ferror(replay_timing_file))
            replay_session_ok = false;
        if (fclose(replay_timing_file))
            replay_session_ok = false;
        replay_timing_file = NULL;
    }
    if (input_record_path && replay_session) {
        char path[2048];
        snprintf(path, sizeof path, "%s.end", input_record_path);
        FILE *f = fopen(path, "wx");
        if (!f)
            replay_session_ok = false;
        else {
            if (fprintf(f, "%u\n", completed_replay_frame) < 0)
                replay_session_ok = false;
            if (fclose(f))
                replay_session_ok = false;
        }
    }
    if (checkpoint_file) {
        if (ferror(checkpoint_file))
            replay_session_ok = false;
        if (fclose(checkpoint_file))
            replay_session_ok = false;
        checkpoint_file = NULL;
    }
    if (replay_session)
        fprintf(stderr,
                "[REPLAY] completed_frames=%u status=%s\n",
                completed_replay_frame,
                replay_session_ok && rr_input_stream_ok() ? "ok" : "ERROR");
    if (!replay_session_ok || !rr_input_stream_ok()) {
        fflush(stderr);
        _Exit(2);
    }
    replay_session = false;
}
static bool replay_session_open(const char *rom_dir)
{
    if (!input_record_path && !input_replay_path && !checkpoint_path && !replay_timing_path)
        return true;
    const char *base = input_record_path ? input_record_path : input_replay_path;
    if (base && (!*base || strlen(base) > 1900))
        return false;
    if (base) {
        FILE *m = tmpfile();
        if (!m)
            return false;
        fprintf(m,
                "RR_REPLAY 1\nGAME %s\nTIMING 25600000/427350\nINPUT_BOUNDARY before-vblank\nRESET "
                "translated-factory-seed\nPOLLS %u\nEEPROM %016" PRIx64 "\n",
                g_rr_game->name,
                polls_per_frame,
                replay_hash(g_rr.eeprom, sizeof g_rr.eeprom));
        bool ok = true;
        for (int i = 0; i < 4; i++)
            ok = ok && replay_rom_hash(m, rom_dir, g_rr_game->prg[i]);
        for (int i = 0; i < 8; i++)
            ok = ok && replay_rom_hash(m, rom_dir, g_rr_game->cg[i]);
        ok = ok && replay_rom_hash(m, rom_dir, g_rr_game->ccrl) && replay_rom_hash(m, rom_dir, g_rr_game->ccrh);
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < g_rr_game->pot_chips; j++)
                ok = ok && replay_rom_hash(m, rom_dir, g_rr_game->pot[i][j]);
        ok = ok && replay_rom_hash(m, rom_dir, g_rr_game->snd_data);
        for (int i = 0; i < 4; i++)
            ok = ok && replay_rom_hash(m, rom_dir, g_rr_game->wav[i].file);
        for (int i = 0; i < 3; i++)
            ok = ok && replay_rom_hash(m, rom_dir, g_rr_game->gamma[i]);
        /* EEPROM metadata describes the initial bytes, not a ROM-directory seed file. */
        if (g_rr_game->eeprom)
            ok = ok && fprintf(m,
                               "ROM %s %zu %016" PRIx64 "\n",
                               g_rr_game->eeprom,
                               sizeof g_rr.eeprom,
                               replay_hash(g_rr.eeprom, sizeof g_rr.eeprom)) > 0;
        char path[2048];
        if (snprintf(path, sizeof path, "%s.meta", base) >= (int)sizeof path)
            ok = false;
        FILE *saved = ok ? fopen(path, input_record_path ? "wx" : "rb") : NULL;
        if (!saved)
            ok = false;
        rewind(m);
        int c;
        if (saved) {
            while ((c = fgetc(m)) != EOF)
                if (input_record_path ? fputc(c, saved) == EOF : fgetc(saved) != c) {
                    ok = false;
                    break;
                }
            if (!input_record_path && fgetc(saved) != EOF)
                ok = false;
            if (ferror(m) || ferror(saved))
                ok = false;
            if (fclose(saved))
                ok = false;
        }
        fclose(m);
        snprintf(path, sizeof path, "%s.eeprom", base);
        saved = ok ? fopen(path, input_record_path ? "wx" : "rb") : NULL;
        if (!saved)
            ok = false;
        if (saved) {
            uint8_t seed[RR_EEPROM_SIZE];
            if (input_record_path)
                ok = fwrite(g_rr.eeprom, 1, sizeof g_rr.eeprom, saved) == sizeof g_rr.eeprom;
            else
                ok = fread(seed, 1, sizeof seed, saved) == sizeof seed && fgetc(saved) == EOF &&
                     !memcmp(seed, g_rr.eeprom, sizeof seed);
            if (fclose(saved))
                ok = false;
        }
        if (!ok) {
            fprintf(stderr, "[REPLAY] metadata/ROM/initial EEPROM mismatch or file creation failure\n");
            return false;
        }
        if (input_replay_path) {
            snprintf(path, sizeof path, "%s.end", base);
            saved = fopen(path, "r");
            unsigned n;
            char tail;
            ok = saved && fscanf(saved, "%u %c", &n, &tail) == 1 && n && n <= INT32_MAX;
            if (saved)
                fclose(saved);
            if (!ok) {
                fprintf(stderr, "[REPLAY] missing/invalid completed recording .end file\n");
                return false;
            }
            replay_end = n;
            if (max_frames > n)
                max_frames = n;
            if (!rr_input_replay_open(base))
                return false;
        } else if (!rr_input_record_start(base))
            return false;
    }
    if (checkpoint_path) {
        checkpoint_file = fopen(checkpoint_path, "wx");
        if (!checkpoint_file) {
            perror(checkpoint_path);
            return false;
        }
    } else if (input_record_path) {
        char path[2048];
        snprintf(path, sizeof path, "%s.states.jsonl", input_record_path);
        checkpoint_file = fopen(path, "wx");
        if (!checkpoint_file) {
            perror(path);
            return false;
        }
    }
    if (replay_timing_path) {
        replay_timing_file = fopen(replay_timing_path, "wx");
        if (!replay_timing_file) {
            perror(replay_timing_path);
            return false;
        }
        setvbuf(replay_timing_file, replay_timing_buffer, _IOFBF, sizeof replay_timing_buffer);
        fputs("frame,completed\n", replay_timing_file);
    }
    replay_session = true;
    atexit(replay_session_close);
    fprintf(stderr,
            "[REPLAY] inputs sampled at emulated boundaries; ROM hashes FNV1a64; checkpoints every 300 ticks\n");
    return true;
}
