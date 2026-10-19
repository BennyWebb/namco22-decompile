/* ROM-only pen/vertex compiler. The shipped packing plan contains coordinates,
 * never texels/vertices. Python verifies ROM/plan/output hashes atomically. */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#define REQUIRE(x)                                                                                                     \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            fprintf(stderr, "asset compiler failed line %d\n", __LINE__);                                              \
            exit(2);                                                                                                   \
        }                                                                                                              \
    } while (0)
static unsigned char *readfile(const char *dir, const char *name, size_t bytes)
{
    char path[4096];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "rb");
    REQUIRE(f);
    unsigned char *p = malloc(bytes);
    REQUIRE(p);
    REQUIRE(fread(p, 1, bytes, f) == bytes && fgetc(f) == EOF);
    fclose(f);
    return p;
}
static void emit(const char *dir, const char *name, const void *p, size_t n)
{
    char path[4096];
    snprintf(path, sizeof path, "%s/%s", dir, name);
    FILE *f = fopen(path, "wb");
    REQUIRE(f);
    REQUIRE(fwrite(p, 1, n, f) == n);
    REQUIRE(fclose(f) == 0);
}
static uint32_t get(FILE *f)
{
    uint32_t x;
    REQUIRE(fread(&x, 4, 1, f) == 1);
    return x;
}
static unsigned char *cg, *lo, *hi;
static int32_t *words;
static unsigned pen(unsigned bank, unsigned cm, unsigned u, unsigned v)
{
    u &= 4095;
    v = (v & 4095) | (bank * 4096);
    unsigned ix = ((v & 0xfff0) << 4) | ((u & 0xff0) >> 4);
    unsigned at = (hi[ix / 2] >> (ix % 2 ? 0 : 4)) & 15;
    unsigned tile = (lo[ix * 2] | lo[ix * 2 + 1] << 8) | ((at & 1) << 16);
    unsigned x = u & 15, y = v & 15;
    if (at & 4)
        x = 15 - x;
    if (at & 2)
        y = 15 - y;
    if (at & 8) {
        unsigned t = x;
        x = y;
        y = t;
    }
    unsigned off = tile * 256 + y * 16 + x;
    unsigned p = off < 0x1000000 ? cg[off] : 0;
    if (cm & 4)
        return (0xec + ((cm & 8) << 1) + ((p >> (2 * (~cm & 3))) & 3)) & 255;
    if (cm & 2)
        return (0xe0 + ((cm & 8) << 1) + ((p >> (4 * (~cm & 1))) & 15)) & 255;
    return p;
}
int main(int argc, char **argv)
{
    REQUIRE(argc == 4);
    const char *rom = argv[1], *out = argv[3];
    FILE *f = fopen(argv[2], "rb");
    REQUIRE(f);
    REQUIRE(get(f) == 0x31503247);
    unsigned nr = get(f), nm = get(f), height = get(f), np = get(f);
    REQUIRE(nr < 65536 && nm < 131072 && np <= 16 && height < 16384);
    unsigned dims[16][2];
    for (unsigned i = 0; i < np; i++) {
        dims[i][0] = get(f);
        dims[i][1] = get(f);
        REQUIRE(dims[i][0] <= 4096 && dims[i][1] <= 4096);
    }
    uint32_t (*rect)[9] = calloc(nr, sizeof *rect);
    REQUIRE(rect);
    for (unsigned i = 0; i < nr; i++)
        for (int j = 0; j < 9; j++)
            rect[i][j] = get(f);
    uint32_t (*mesh)[2] = calloc(nm, sizeof *mesh);
    REQUIRE(mesh);
    for (unsigned i = 0; i < nm; i++) {
        mesh[i][0] = get(f);
        mesh[i][1] = get(f);
    }
    REQUIRE(fgetc(f) == EOF);
    fclose(f);
    cg = malloc(0x1000000);
    REQUIRE(cg);
    const char *names[] = {
        "rv1cg0.1a", "rv1cg1.1c", "rv1cg2.1d", "rv1cg3.1e", "rv1cg4.1f", "rv1cg5.1j", "rv1cg6.1k", "rv1cg7.1n"};
    for (int i = 0; i < 8; i++) {
        void *p = readfile(rom, names[i], 0x200000);
        memcpy(cg + i * 0x200000, p, 0x200000);
        free(p);
    }
    lo = readfile(rom, "rv1ccrl.5a", 0x200000);
    hi = readfile(rom, "rv1ccrh.5c", 0x80000);
    for (unsigned page = 0; page < np; page++) {
        unsigned w = dims[page][0], h = dims[page][1];
        unsigned char *p = calloc(w, h);
        REQUIRE(p);
        for (unsigned i = 0; i < nr; i++) {
            uint32_t *r = rect[i];
            if (r[6] != page)
                continue;
            REQUIRE(r[7] + r[4] + 4 <= w && r[8] + r[5] + 4 <= h);
            for (unsigned y = 0; y < r[5] + 4; y++)
                for (unsigned x = 0; x < r[4] + 4; x++)
                    p[(r[8] + y) * w + r[7] + x] = pen(r[0], r[1], r[2] + x - 2, r[3] + y - 2);
        }
        char name[64];
        snprintf(name, sizeof name, "page-%u.r8ui", page);
        emit(out, name, p, (size_t)w * h);
        free(p);
        printf("[ASSETS] decoded page %u/%u\n", page + 1, np);
        fflush(stdout);
    }
    free(cg);
    free(lo);
    free(hi);
    words = calloc(0x200000, 4);
    REQUIRE(words);
    const char *suffix[] = {"l", "m", "u"};
    const char *ext[] = {"b", "c", "d"};
    for (unsigned plane = 0; plane < 3; plane++)
        for (unsigned part = 0; part < 4; part++) {
            char name[64];
            snprintf(name, sizeof name, "rv1pot%s%u.%u%s", suffix[plane], part, 5 - part, ext[plane]);
            unsigned char *p = readfile(rom, name, 0x80000);
            for (unsigned i = 0; i < 0x80000; i++)
                words[part * 0x80000 + i] |= (uint32_t)p[i] << (plane * 8);
            free(p);
        }
    for (unsigned i = 0; i < 0x200000; i++)
        words[i] = (int32_t)((uint32_t)words[i] << 8) >> 8;
    float *geo = calloc((size_t)height * 1024 * 4, sizeof(float));
    REQUIRE(geo);
    for (unsigned i = 0; i < nm; i++) {
        unsigned at = mesh[i][0], tex = mesh[i][1];
        REQUIRE(at + 20 <= 0x200000 && tex + 8 <= height * 1024);
        for (unsigned v = 0; v < 4; v++) {
            float *p = geo + (tex + v * 2) * 4;
            for (unsigned c = 0; c < 3; c++)
                p[c] = (float)words[at + 8 + v * 3 + c];
            p[4] = (float)(words[at + v * 2] & 4095);
            p[5] = (float)(words[at + v * 2 + 1] & 4095);
        }
    }
    emit(out, "meshes.f32", geo, (size_t)height * 1024 * 16);
    printf("[ASSETS] compiled %u meshes; complete\n", nm);
    free(geo);
    free(words);
    free(rect);
    free(mesh);
    return 0;
}
