/* Whole-ROM compiler primitives. Decodes bounded rows; never retains texel dumps.
 * The original plan builder and this compiler share the exact pen semantics. */
#define main rr_asset_plan_main
#include "asset_builder.c"
#undef main
/* Stable opaque EVP ABI, also usable on host test systems without SDK headers. */
typedef struct evp_md_ctx_st EVP_MD_CTX;
typedef struct evp_md_st EVP_MD;
extern EVP_MD_CTX *EVP_MD_CTX_new(void);
extern void EVP_MD_CTX_free(EVP_MD_CTX *);
extern const EVP_MD *EVP_sha256(void);
extern int EVP_DigestInit_ex(EVP_MD_CTX *, const EVP_MD *, void *);
extern int EVP_DigestUpdate(EVP_MD_CTX *, const void *, size_t);
extern int EVP_DigestFinal_ex(EVP_MD_CTX *, unsigned char *, unsigned *);
#include <limits.h>

void rr_codec_init(const char *rom)
{
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
}
int32_t *rr_codec_words(void)
{
    return words;
}
static void row_pens(const int *r, int y, unsigned char *row)
{
    for (int x = 0; x < r[4] + 4; x++)
        row[x] = pen(r[0], r[1], r[2] + x - 2, r[3] + y - 2);
}
void rr_codec_digest(const int *r, unsigned char *digest)
{
    REQUIRE(r[4] > 0 && r[4] <= 4092 && r[5] > 0 && r[5] <= 4092);
    /* EVP avoids deprecated low-level SHA context APIs. */
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    REQUIRE(ctx && EVP_DigestInit_ex(ctx, EVP_sha256(), NULL));
    uint32_t dimensions[] = {(unsigned)r[4] + 4, (unsigned)r[5] + 4};
    REQUIRE(EVP_DigestUpdate(ctx, dimensions, sizeof dimensions));
    unsigned char row[4096];
    for (int y = 0; y < r[5] + 4; y++) {
        row_pens(r, y, row);
        REQUIRE(EVP_DigestUpdate(ctx, row, r[4] + 4));
    }
    unsigned length;
    REQUIRE(EVP_DigestFinal_ex(ctx, digest, &length) && length == 32);
    EVP_MD_CTX_free(ctx);
}
int rr_codec_equal(const int *a, const int *b)
{
    if (a[4] != b[4] || a[5] != b[5])
        return 0;
    unsigned char ar[4096], br[4096];
    for (int y = 0; y < a[5] + 4; y++) {
        row_pens(a, y, ar);
        row_pens(b, y, br);
        if (memcmp(ar, br, a[4] + 4))
            return 0;
    }
    return 1;
}
void rr_codec_place(const char *path, int width, int height, const int *r, int x, int y)
{
    REQUIRE(x >= 0 && y >= 0 && x + r[4] + 4 <= width && y + r[5] + 4 <= height);
    FILE *f = fopen(path, "r+b");
    REQUIRE(f);
    unsigned char row[4096];
    for (int i = 0; i < r[5] + 4; i++) {
        row_pens(r, i, row);
        REQUIRE(!fseek(f, (long)(y + i) * width + x, SEEK_SET));
        REQUIRE(fwrite(row, 1, r[4] + 4, f) == (size_t)r[4] + 4);
    }
    REQUIRE(!fclose(f));
}
void rr_codec_verify(const char *path, int width, int height, const int *r, int x, int y)
{
    REQUIRE(x >= 0 && y >= 0 && x + r[4] + 4 <= width && y + r[5] + 4 <= height);
    FILE *f = fopen(path, "rb");
    REQUIRE(f);
    unsigned char expected[4096], actual[4096];
    for (int i = 0; i < r[5] + 4; i++) {
        row_pens(r, i, expected);
        REQUIRE(!fseek(f, (long)(y + i) * width + x, SEEK_SET));
        REQUIRE(fread(actual, 1, r[4] + 4, f) == (size_t)r[4] + 4);
        REQUIRE(!memcmp(actual, expected, r[4] + 4));
    }
    REQUIRE(!fclose(f));
}
void rr_codec_meshes(const char *path, const uint32_t *meshes, unsigned count, unsigned height)
{
    FILE *f = fopen(path, "wb");
    REQUIRE(f);
    for (unsigned i = 0; i < count; i++) {
        unsigned at = meshes[i * 2], texel = meshes[i * 2 + 1];
        REQUIRE(at + 20 <= 0x200000 && texel == i * 8);
        float face[8][4] = {{0}};
        for (unsigned v = 0; v < 4; v++) {
            for (unsigned c = 0; c < 3; c++)
                face[v * 2][c] = (float)words[at + 8 + v * 3 + c];
            face[v * 2 + 1][0] = (float)(words[at + v * 2] & 4095);
            face[v * 2 + 1][1] = (float)(words[at + v * 2 + 1] & 4095);
        }
        REQUIRE(fwrite(face, sizeof face, 1, f) == 1);
    }
    unsigned char zero[16] = {0};
    for (unsigned i = count * 8; i < height * 1024; i++)
        REQUIRE(fwrite(zero, sizeof zero, 1, f) == 1);
    REQUIRE(!fclose(f));
}
/* Merge priority is Python heapq's (-saving, earlier_id, later_id), exactly.
 * Only 12 bytes per pending candidate; merged bounds are recomputed on pop. */
typedef struct {
    int save, a, b;
} Candidate;
static Candidate *heap;
static size_t hn, hc;
static int better(Candidate a, Candidate b)
{
    return a.save != b.save ? a.save > b.save : a.a != b.a ? a.a < b.a : a.b < b.b;
}
static void push(Candidate x)
{
    if (hn == hc) {
        hc = hc ? hc * 2 : 1024;
        heap = realloc(heap, hc * sizeof *heap);
        REQUIRE(heap);
    }
    size_t i = hn++;
    while (i && better(x, heap[(i - 1) / 2])) {
        heap[i] = heap[(i - 1) / 2];
        i = (i - 1) / 2;
    }
    heap[i] = x;
}
static Candidate pop(void)
{
    Candidate result = heap[0], x = heap[--hn];
    size_t i = 0;
    while (i * 2 + 1 < hn) {
        size_t c = i * 2 + 1;
        if (c + 1 < hn && better(heap[c + 1], heap[c]))
            c++;
        if (!better(heap[c], x))
            break;
        heap[i] = heap[c];
        i = c;
    }
    heap[i] = x;
    return result;
}
static int imin(int a, int b)
{
    return a < b ? a : b;
}
static int imax(int a, int b)
{
    return a > b ? a : b;
}
static void bounds(const int *a, const int *b, int *r)
{
    r[0] = a[0];
    r[1] = a[1];
    r[2] = imin(a[2], b[2]);
    r[3] = imin(a[3], b[3]);
    r[4] = imax(a[2] + a[4], b[2] + b[4]) - r[2];
    r[5] = imax(a[3] + a[5], b[3] + b[5]) - r[3];
}
static void candidates(int i, int (*rect)[6], const unsigned char *active)
{
    const int *a = rect[i];
    for (int j = 0; j < i; j++) {
        const int *b = rect[j];
        if (!active[j] || a[0] != b[0] || a[1] != b[1] || imin(a[2] + a[4], b[2] + b[4]) <= imax(a[2], b[2]) ||
            imin(a[3] + a[5], b[3] + b[5]) <= imax(a[3], b[3]))
            continue;
        int r[6];
        bounds(a, b, r);
        int save = (a[4] + 4) * (a[5] + 4) + (b[4] + 4) * (b[5] + 4) - (r[4] + 4) * (r[5] + 4);
        if (r[4] <= 4092 && r[5] <= 4092 && r[4] * r[5] < a[4] * a[5] + b[4] * b[5] && save > 0)
            push((Candidate){save, j, i});
    }
}
int rr_codec_merge(int (*rect)[6], int count, int *parents, int *savings)
{
    unsigned char *active = calloc(count * 2 + 1, 1);
    REQUIRE(active);
    for (int i = 0; i < count * 2; i++)
        parents[i] = i;
    for (int i = 0; i < count; i++) {
        active[i] = 1;
        candidates(i, rect, active);
    }
    int n = count, merges = 0;
    while (hn) {
        Candidate x = pop();
        if (!active[x.a] || !active[x.b])
            continue;
        bounds(rect[x.a], rect[x.b], rect[n]);
        active[x.a] = active[x.b] = 0;
        parents[x.a] = parents[x.b] = n;
        active[n] = 1;
        savings[merges++] = x.save;
        candidates(n++, rect, active);
    }
    free(active);
    free(heap);
    heap = NULL;
    hc = hn = 0;
    return merges;
}
typedef struct {
    int x, y, w, h;
} Rect;
typedef struct {
    Rect *free;
    int n, width, height;
} Page;
static const int *pack_sizes;
static const unsigned char *pack_keys;
static int item_compare(const void *a, const void *b)
{
    int i = *(const int *)a, j = *(const int *)b;
    int w = pack_sizes[i * 2], h = pack_sizes[i * 2 + 1];
    int v = pack_sizes[j * 2], t = pack_sizes[j * 2 + 1];
    if (imax(w, h) != imax(v, t))
        return imax(v, t) - imax(w, h);
    if (w * h != v * t)
        return v * t - w * h;
    return memcmp(pack_keys + i * 32, pack_keys + j * 32, 32);
}
static int rect_compare(const void *a, const void *b)
{
    const Rect *x = a, *y = b;
    if (x->x != y->x)
        return x->x - y->x;
    if (x->y != y->y)
        return x->y - y->y;
    if (x->w != y->w)
        return x->w - y->w;
    return x->h - y->h;
}
static int contains(Rect a, Rect b)
{
    return a.x <= b.x && a.y <= b.y && a.x + a.w >= b.x + b.w && a.y + a.h >= b.y + b.h;
}
int rr_codec_pack(int count, const int *sizes, const unsigned char *keys, int (*placements)[5], int (*dims)[2])
{
    Page pages[16] = {{0}};
    int np = 0;
    int *order = malloc(count * sizeof *order);
    REQUIRE(order);
    for (int i = 0; i < count; i++)
        order[i] = i;
    pack_sizes = sizes;
    pack_keys = keys;
    qsort(order, count, sizeof *order, item_compare);
    for (int at = 0; at < count; at++) {
        int i = order[at], w = sizes[i * 2], h = sizes[i * 2 + 1];
        REQUIRE(w > 0 && h > 0 && w <= 4096 && h <= 4096);
        int best[5] = {INT_MAX, INT_MAX, INT_MAX, INT_MAX, INT_MAX};
        Rect chosen = {0};
        int page = -1;
        for (int p = 0; p < np; p++)
            for (int j = 0; j < pages[p].n; j++) {
                Rect r = pages[p].free[j];
                if (w > r.w || h > r.h)
                    continue;
                int score[] = {imin(r.w - w, r.h - h), imax(r.w - w, r.h - h), p, r.y, r.x};
                int comparison = 0;
                for (int c = 0; c < 5; c++)
                    if (score[c] != best[c]) {
                        comparison = score[c] < best[c] ? -1 : 1;
                        break;
                    }
                if (comparison < 0) {
                    memcpy(best, score, sizeof best);
                    chosen = r;
                    page = p;
                }
            }
        if (page < 0) {
            REQUIRE(np < 16);
            page = np++;
            chosen = (Rect){0, 0, 4096, 4096};
            pages[page].free = malloc(sizeof(Rect));
            REQUIRE(pages[page].free);
            pages[page].free[0] = chosen;
            pages[page].n = 1;
        }
        Page *p = pages + page;
        int x = chosen.x, y = chosen.y;
        Rect *next = malloc((p->n * 4 + 1) * sizeof *next);
        REQUIRE(next);
        int nn = 0;
        for (int j = 0; j < p->n; j++) {
            Rect r = p->free[j];
            if (x >= r.x + r.w || x + w <= r.x || y >= r.y + r.h || y + h <= r.y) {
                next[nn++] = r;
                continue;
            }
            if (x > r.x)
                next[nn++] = (Rect){r.x, r.y, x - r.x, r.h};
            if (x + w < r.x + r.w)
                next[nn++] = (Rect){x + w, r.y, r.x + r.w - x - w, r.h};
            if (y > r.y)
                next[nn++] = (Rect){r.x, r.y, r.w, y - r.y};
            if (y + h < r.y + r.h)
                next[nn++] = (Rect){r.x, y + h, r.w, r.y + r.h - y - h};
        }
        free(p->free);
        qsort(next, nn, sizeof *next, rect_compare);
        int unique = 0;
        for (int j = 0; j < nn; j++)
            if (!j || rect_compare(next + j, next + j - 1))
                next[unique++] = next[j];
        p->free = malloc((unique + 1) * sizeof *p->free);
        REQUIRE(p->free);
        p->n = 0;
        for (int j = 0; j < unique; j++) {
            int enclosed = 0;
            for (int k = 0; k < unique; k++)
                if (j != k && contains(next[k], next[j])) {
                    enclosed = 1;
                    break;
                }
            if (!enclosed)
                p->free[p->n++] = next[j];
        }
        free(next);
        p->width = imax(p->width, x + w);
        p->height = imax(p->height, y + h);
        int result[] = {page, x, y, w, h};
        memcpy(placements[i], result, sizeof result);
    }
    for (int p = 0; p < np; p++) {
        dims[p][0] = (pages[p].width + 3) / 4 * 4;
        dims[p][1] = (pages[p].height + 3) / 4 * 4;
        free(pages[p].free);
    }
    free(order);
    return np;
}
