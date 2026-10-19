/*
 * rr_mem.c -- see include/rr_mem.h.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rr_mem.h"
#include "rr_game.h"

rr_sys_t g_rr;

static rr_io_read_fn  s_io_r;
static rr_io_write_fn s_io_w;
static bool s_wram_plain;
void rr_set_io_hooks(rr_io_read_fn r, rr_io_write_fn w)
{ s_io_r = r; s_io_w = w; s_wram_plain = false; }
void rr_set_plain_wram_io_hooks(rr_io_read_fn r, rr_io_write_fn w)
{ rr_set_io_hooks(r,w); s_wram_plain = true; }

#ifdef RR_MEMORY_READ_COUNTERS
static struct { uint64_t widths[4], regions[5], hits, general_paths, callbacks; } read_stats;
static uint32_t read_from=UINT32_MAX, read_to, read_frame;
static bool count_reads;
#define READ_COUNT(field) do { if(count_reads) read_stats.field++; } while(0)
#define READ_IO(a,n,p) ((count_reads ? (++read_stats.callbacks,0) : 0),s_io_r(a,n,p))
static void count_read(vaddr_t a,int size)
{
    if(!count_reads) return;
    read_stats.widths[size==1?0:size==2?1:size==4?2:3]++;
    unsigned bucket=a<RR_ROM_SIZE?0:
        (a & 0xF7FE0000u)==0x10000000u?1:
        a>=0x70000000u && a<0x70000000u+RR_POLY_WORDS*4?2:
        (a>=0x20000000u && a<0x20000010u) ||
        (a>=0x20020000u && a<0x20020010u) ||
        (a>=0x50000000u && a<0x5000000cu)?3:4;
    read_stats.regions[bucket]++;
}
#else
#define READ_COUNT(field) ((void)0)
#define READ_IO(a,n,p) s_io_r(a,n,p)
#endif
void rr_memory_profile_range(uint32_t first,uint32_t last)
{
#ifdef RR_MEMORY_READ_COUNTERS
    read_from=first;read_to=last;memset(&read_stats,0,sizeof read_stats);
    count_reads=read_frame>=first && read_frame<=last;
#else
    (void)first;(void)last;
#endif
}
void rr_memory_profile_frame(uint32_t frame)
{
#ifdef RR_MEMORY_READ_COUNTERS
    read_frame=frame;count_reads=frame>=read_from && frame<=read_to;
#else
    (void)frame;
#endif
}
void rr_memory_profile_report(void)
{
#ifdef RR_SPLIT_MEMORY_READER
    fprintf(stderr,"[MEM-READER] implementation=split\n");
#elif defined(RR_FAST_MEMORY_READS)
    fprintf(stderr,"[MEM-READER] implementation=existing\n");
#else
    fprintf(stderr,"[MEM-READER] implementation=original\n");
#endif
#ifdef RR_FAST_MEMORY_READS
    const char *fast="on";
#else
    const char *fast="off";
#endif
#ifdef RR_MEMORY_READ_COUNTERS
    fprintf(stderr,"[MEM-READS] fast=%s counters=on from=%u to=%u byte=%llu word=%llu long=%llu other_width=%llu ROM=%llu WRAM=%llu POLY=%llu IO_bucket=%llu other_bucket=%llu hits=%llu general_paths=%llu callbacks=%llu\n",
        fast,read_from,read_to,(unsigned long long)read_stats.widths[0],(unsigned long long)read_stats.widths[1],
        (unsigned long long)read_stats.widths[2],(unsigned long long)read_stats.widths[3],
        (unsigned long long)read_stats.regions[0],(unsigned long long)read_stats.regions[1],
        (unsigned long long)read_stats.regions[2],(unsigned long long)read_stats.regions[3],(unsigned long long)read_stats.regions[4],
        (unsigned long long)read_stats.hits,(unsigned long long)read_stats.general_paths,(unsigned long long)read_stats.callbacks);
#else
    fprintf(stderr,"[MEM-READS] fast=%s counters=off\n",fast);
#endif
}

/* A big-endian byte region: returns the base and the byte offset, or NULL. */
static uint8_t *region(vaddr_t a, uint32_t *off, uint32_t *len, bool *ro)
{
    *ro = false;
    if (a < RR_ROM_SIZE) { *off = a; *len = RR_ROM_SIZE; *ro = true; return g_rr.rom; }
    uint32_t hi = a & 0xF8000000u;
    if (hi == 0x10000000u || hi == 0x18000000u) {            /* work RAM + mirror */
        uint32_t o = a & 0x07FFFFFFu;
        if (o < RR_WRAM_SIZE) { *off = o; *len = RR_WRAM_SIZE; return g_rr.wram; }
        return NULL;
    }
#define R(base, size, arr) \
    if (a >= (base) && a < (base) + (size)) { *off = a - (base); *len = (size); return (arr); }
    R(0x20010000u, RR_SCI_SIZE,    g_rr.sci)
    R(0x58000000u, RR_EEPROM_SIZE, g_rr.eeprom)
    R(0x60004000u, RR_SHARED_SIZE, g_rr.shared)
    R(0x90010000u, RR_CZRAM_SIZE,  g_rr.czram)
    R(0x90020000u, RR_MIXER_SIZE,  g_rr.mixer)
    R(0x90028000u, RR_PAL_SIZE,    g_rr.pal)
    R(0x9009E000u, RR_TEXT_SIZE,   g_rr.text)     /* before CGRAM: it overlaps its tail */
    R(0x90080000u, RR_CGRAM_SIZE,  g_rr.cgram)
    R(0x40000000u, 0x20,           g_rr.syscon)
    R(0x900A0000u, 0x10,           g_rr.tilemapattr)
    R(0x20020000u, 0x10,           g_rr.sci_reg)
#undef R
    return NULL;
}

static void complain(const char *what, vaddr_t a, int size)
{
    static uint32_t seen[256]; static int nseen;
    uint32_t page = a & ~0xFFFu;
    for (int i = 0; i < nseen; i++) if (seen[i] == page) return;
    if (nseen < 256) seen[nseen++] = page;
    fprintf(stderr, "[RRMEM] %s %d-byte access at 0x%08X\n", what, size, a);
}

/* Polygon RAM: 32-bit words, 24 bits connected, stored sign-extended the way
 * MAME's namcos22_dspram_w does (the master DSP port reads it that way). */
static bool poly_access(vaddr_t a, int size, bool wr, uint32_t *v)
{
    if (a < 0x70000000u || a >= 0x70000000u + RR_POLY_WORDS * 4) return false;
    uint32_t idx = (a - 0x70000000u) >> 2, sh = (3 - (a & 3)) * 8;
    uint32_t word = g_rr.poly[idx];
    if (!wr) {
        uint32_t full = word | 0xFF000000u;                  /* only d0-23 connected */
        if (size == 4) *v = full;
        else if (size == 2) *v = (full >> ((a & 2) ? 0 : 16)) & 0xFFFF;
        else *v = (full >> sh) & 0xFF;
        return true;
    }
    uint32_t nv;
    if (size == 4) nv = *v;
    else if (size == 2) { uint32_t s2 = (a & 2) ? 0 : 16; nv = (word & ~(0xFFFFu << s2)) | ((*v & 0xFFFF) << s2); }
    else nv = (word & ~(0xFFu << sh)) | ((*v & 0xFF) << sh);
    g_rr.poly[idx] = (uint32_t)((int32_t)(nv << 8) >> 8);    /* signed24 */
    return true;
}

extern int rd_journal_on, rd_read_log, rd_quiet, rd_io_written, rd_io_touched;
void rd_io_log(vaddr_t a, int size, uint32_t v);
extern int rd_ior_replay;
void rd_ior_log(vaddr_t a, int size, uint32_t v);
uint32_t rd_ior_serve(vaddr_t a, int size);
void rd_read_byte(vaddr_t a, uint8_t *p);
void rd_dict_byte(uint8_t v);
#ifdef RR_TRACE
void (*rr_read_probe)(vaddr_t, int);                  /* the trace oracle (engine/lift_env.c): sees every read */
#endif
#if defined(RR_SPLIT_MEMORY_READER) && defined(RR_FAST_MEMORY_READS)
/* GCC 13.3 at -O3 inlines the plain split and recreates the 112-byte frame.
 * Preserve this boundary, without marking ordinary ROM/device reads cold. */
static uint32_t rr_read_general_path(vaddr_t a, int size) __attribute__((noinline));
#endif
uint32_t rr_read(vaddr_t a, int size)
{
#ifdef RR_MEMORY_READ_COUNTERS
    count_read(a,size);
#endif
#ifdef RR_FAST_MEMORY_READS
    /* Precisely the two original 128-KiB WRAM windows. No extra aliases.
     * Unsupported/checker/custom-hook/boundary cases retain original ordering. */
    if((a & 0xF7FE0000u)==0x10000000u &&
       (size==1 || size==2 || size==4) &&
       (uint32_t)size<=RR_WRAM_SIZE-(a & 0x07FFFFFFu) &&
       (!s_io_r || s_wram_plain) && !rd_journal_on && !rd_read_log
#ifdef RR_TRACE
       && !rr_read_probe
#endif
       ) {
        const uint8_t *p=g_rr.wram+(a & 0x07FFFFFFu);
        READ_COUNT(hits);
        if(size==1) return *p;
        if(size==2) {
            uint16_t value;memcpy(&value,p,sizeof value);
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
            value=__builtin_bswap16(value);
#endif
            return value;
        }
        uint32_t value;memcpy(&value,p,sizeof value);
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        value=__builtin_bswap32(value);
#endif
        return value;
    }
#endif
#if defined(RR_SPLIT_MEMORY_READER) && defined(RR_FAST_MEMORY_READS)
    return rr_read_general_path(a, size);
}

/* Keep the original callback/checker/mapping sequence in one place. The common
 * WRAM entry needs none of its address-taken locals or callback saved state. */
static uint32_t rr_read_general_path(vaddr_t a, int size)
{
#endif
    READ_COUNT(general_paths);
    uint32_t v = 0;
#ifdef RR_TRACE
    if (rr_read_probe) rr_read_probe(a, size);
#endif
    if (rd_journal_on && s_io_r && !(a >= 0x70000000u && a < 0x70000000u + RR_POLY_WORDS * 4)) {
        uint32_t o_, l_; bool r_;
        if (!region(a, &o_, &l_, &r_)) {           /* an I/O read during a check (src/rd) */
            if (rd_ior_replay) return rd_ior_serve(a, size);   /* the readable run: the lifted run's value */
            if (READ_IO(a, size, &v)) {                          /* the lifted run: real, once, logged */
                if (rd_io_written) rd_io_touched = 1;           /* a captured write came first */
                rd_ior_log(a, size, v);
                return v;
            }
            rd_ior_log(a, size, 0);        /* unmapped (a fuzzed pointer): logged too, reads 0 below */
        }
    }
    if (s_io_r && READ_IO(a, size, &v)) return v;
    if (poly_access(a, size, false, &v)) return v;
    uint32_t off, len; bool ro;
    uint8_t *m = region(a, &off, &len, &ro);
    if (!m || off + (uint32_t)size > len) { g_rr.n_unmapped++; if (!rd_quiet) complain("unmapped read", a, size); return 0; }
    if (rd_read_log) for (int i = 0; i < size; i++) {
        if (!ro) rd_read_byte(a + (uint32_t)i, &m[off + i]);
        else rd_dict_byte(m[off + i]);                 /* ROM values: the fuzzer's dictionary */
    }
    for (int i = 0; i < size; i++) v = (v << 8) | m[off + i];
    return v;
}

#ifdef RR_TRACE
/* RR_WATCH=<addr>: every write covering that byte, with the writing PC (trace build only) */
extern uint32_t rr_trace_pc;
static void watch(vaddr_t a, int size, uint32_t v)
{
    static long w = -2;
    if (w == -2) { const char *e = getenv("RR_WATCH"); w = e ? strtol(e, NULL, 0) : -1; }
    if (w >= 0 && (uint32_t)w >= a && (uint32_t)w < a + (uint32_t)size)
        fprintf(stderr, "[WATCH] %08X <- %0*X (size %d) at PC %06X\n", (unsigned)a, size * 2, (unsigned)v, size, (unsigned)rr_trace_pc);
}
#endif
/* Readable-C checker journal (src/rd/rd_core.c): while on, every byte a write
 * changes is recorded with its old value so the write can be rolled back and
 * compared; an access the journal cannot undo (I/O hooks, polygon RAM) marks
 * the call unverifiable instead. */
int  rd_journal_on;
void rd_journal_byte(vaddr_t a, uint8_t *p);

void rr_write(vaddr_t a, int size, uint32_t v)
{
#ifdef RR_TRACE
    watch(a, size, v);
#endif
    if (rd_journal_on && s_io_w && !(a >= 0x70000000u && a < 0x70000000u + RR_POLY_WORDS * 4)) {
        uint32_t o_, l_; bool r_;
        /* syscon (0x40000000) and the C139 SCI regs (0x20020000) are backed by memory
         * but their writes have side effects (IRQ enables/acks, sound CPU hold/release,
         * DSP control; SCI: TX capture/completion, the RX ring pointer): an I/O write too */
        if (!region(a, &o_, &l_, &r_) || (a >= 0x40000000u && a < 0x40000020u)
            || (a >= 0x20020000u && a < 0x20020010u)) {
            rd_io_log(a, size, v); return;                      /* src/rd: captured, replayed later */
        }
    }
    if (s_io_w && s_io_w(a, size, v)) return;
    if (rd_journal_on && a >= 0x70000000u && a < 0x70000000u + RR_POLY_WORDS * 4) {   /* journal the word's 4 host bytes */
        uint32_t base = a & ~3u;
        uint8_t *w = (uint8_t *)&g_rr.poly[(base - 0x70000000u) >> 2];
        for (int i = 0; i < 4; i++) rd_journal_byte(base + (uint32_t)i, w + i);
    }
    if (poly_access(a, size, true, &v)) return;
    uint32_t off, len; bool ro;
    uint8_t *m = region(a, &off, &len, &ro);
    if (!m || off + (uint32_t)size > len) { g_rr.n_unmapped++; if (!rd_quiet) complain("unmapped write", a, size); return; }
    if (ro) { g_rr.n_romwrite++; complain("ROM write", a, size); return; }
    if (rd_journal_on) for (int i = 0; i < size; i++) rd_journal_byte(a + (uint32_t)i, &m[off + i]);
    for (int i = size - 1; i >= 0; i--) { m[off + i] = (uint8_t)v; v >>= 8; }
}

bool rr_load_program(const char *dir)
{
    /* MAME ROM_LOAD32_BYTE offsets: uub 0, umb 1, lmb 2, llb 3 */
    const char *const *lane = g_rr_game->prg;
    for (int l = 0; l < 4; l++) {
        char p[1024]; snprintf(p, sizeof p, "%s/%s", dir, lane[l]);
        FILE *f = fopen(p, "rb");
        if (!f) { fprintf(stderr, "cannot open %s\n", p); return false; }
        for (uint32_t i = 0; i < RR_ROM_SIZE / 4; i++) {
            int c = fgetc(f);
            if (c == EOF) { fclose(f); fprintf(stderr, "short %s\n", p); return false; }
            g_rr.rom[i * 4 + l] = (uint8_t)c;
        }
        fclose(f);
    }
    return true;
}
