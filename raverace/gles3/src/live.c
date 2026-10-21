#ifdef GLES3_VALIDATION
extern unsigned char *SHA256(const unsigned char *, unsigned long, unsigned char *);
#endif

#include "renderer_api.h"
#include "rr_gles3.h"

static void live_fail(const char *, int, int);
#define _GNU_SOURCE 1
/* Revised OBJECT: resident original meshes, compact commands and bounded prefetch.
 * No simulator, ROM decoder or audio linked. Repository MIT licence. */
#include <SDL2/SDL.h>
#include <GLES3/gl3.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/resource.h>
#include <pthread.h>

#define RR_GLES3 1
#define RR_GPU_HUD 1
#define RR_GPU_HUD_DEFAULT 1
#include "depth_lut_support.inc"
#include "rr_video_layers.h"
#include "rr_gl.h"
extern unsigned g_eng_frame;

#define RENDER_CHECK(x)                                                                                                \
    do {                                                                                                               \
        if (!(x)) {                                                                                                    \
            fprintf(stderr, "packet failure line=%d %s SDL=%s\n", __LINE__, #x, SDL_GetError());                       \
            exit(2);                                                                                                   \
        }                                                                                                              \
    } while (0)
#define MAX_QUADS 4096
#define MAX_INSTANCES 512
#define MAX_MATERIALS 65535
#define PACKET_CAPACITY 524288
#define GPU_COPY_COUNT 3
#define FONT_BYTES 122880
#define STYLE_BYTES 2073
typedef struct {
    uint32_t magic, version, tick, n, visible, groups, instances, dynamic, font, style, palette, reserved;
    float bg[3];
} RenderPacketHeader;
typedef struct {
    float data[16];
    int32_t box[4];
} RenderInstance;
typedef struct {
    int32_t mesh;
    uint16_t instance, material;
    float depth;
    uint16_t brightness[4], source_instance, order, flags, reserved;
    uint32_t zsort;
} RenderQuad;
typedef struct {
    uint32_t first, count, page, mode;
    int32_t box[4];
} DrawGroup;
typedef struct {
    float uv[4];
    uint32_t page, pad[3];
} RenderMaterial;
typedef struct {
    float state[4], brightness[4];
} DrawAttribute;
_Static_assert(sizeof(RenderPacketHeader) == 60 && sizeof(RenderInstance) == 80 && sizeof(RenderQuad) == 32 &&
                   sizeof(DrawGroup) == 32 && sizeof(RenderMaterial) == 32,
               "packet ABI");
typedef struct {
    uint8_t bytes[PACKET_CAPACITY];
    size_t size;
    int state;
    unsigned tick;
    double read_ms, inflate_ms, worker_cpu_ms;
} RenderPacket;
static GLuint frame_instances[3], frame_dynamic[3], frame_attributes[3];
static GLsync frame_fences[3];
static unsigned frame_gpu_index;
static const rr_video_cpu_frame *frame_hud;
static const rr_presentation *gles3_drawing;
static bool frame_diagnostic;
static double capture_upload_wall, capture_upload_cpu;
static unsigned capture_groups;
static unsigned material_groups[MAX_MATERIALS];
static GLuint palette_textures[3];
static uint64_t uploaded_palette_revision[3][128];
static bool palette_valid[3][128];
/* CPU allocator owns immutable ROM pens. Slot copies sever cache lifetime from GL. */
#include "dynamic_types.h"
static DynamicCacheEntry dynamic_cache_entries[DYNAMIC_CACHE_ENTRIES];
static uint16_t dynamic_cache_cells[384 * 384];
static uint8_t dynamic_cache_page[DYNAMIC_CACHE_BYTES];
static uint64_t dynamic_cache_generation, dynamic_cache_serial;
static void dynamic_cache_mark(unsigned id, bool fill)
{
    DynamicCacheEntry *e = dynamic_cache_entries + id;
    for (unsigned y = e->y / 8; y < (e->y + e->h + 7) / 8; y++)
        for (unsigned x = e->x / 8; x < (e->x + e->w + 7) / 8; x++)
            dynamic_cache_cells[y * 384 + x] = fill ? id + 1 : 0;
}
static bool dynamic_cache_place(unsigned w, unsigned h, unsigned *x, unsigned *y)
{
    unsigned cw = (w + 7) / 8, ch = (h + 7) / 8;
    for (unsigned yy = 0; yy + ch <= 384; yy++)
        for (unsigned xx = 0; xx + cw <= 384; xx++) {
            bool empty = true;
            for (unsigned j = 0; j < ch && empty; j++)
                for (unsigned i = 0; i < cw; i++)
                    if (dynamic_cache_cells[(yy + j) * 384 + xx + i]) {
                        empty = false;
                        break;
                    }
            if (empty) {
                *x = xx * 8;
                *y = yy * 8;
                return true;
            }
        }
    return false;
}
static bool dynamic_cache_pinned(const DynamicCacheSlot *s, unsigned i)
{
    for (unsigned j = 0; j < s->uploads; j++)
        if (s->upload[j].entry == i)
            return true;
    return false;
}
static unsigned dynamic_cache_lru(const DynamicCacheSlot *s)
{
    unsigned best = DYNAMIC_CACHE_ENTRIES;
    for (unsigned i = 0; i < DYNAMIC_CACHE_ENTRIES; i++)
        if (dynamic_cache_entries[i].occupied && !dynamic_cache_pinned(s, i) &&
            (best == DYNAMIC_CACHE_ENTRIES || dynamic_cache_entries[i].last < dynamic_cache_entries[best].last))
            best = i;
    return best;
}
/* Returns an existing or freshly allocated footprint. Palette is deliberately absent. */
static int dynamic_cache_reserve(DynamicCacheSlot *s, const int key[6], bool *decode)
{
    if (key[0] < 0 || key[0] > 15 || key[1] < 0 || key[1] > 15 || key[2] < 0 || key[2] > 4095 || key[3] < 0 ||
        key[3] > 4095 || key[4] < 1 || key[5] < 1 || key[4] > DYNAMIC_CACHE_SIDE - 4 || key[5] > DYNAMIC_CACHE_SIDE - 4)
        return -1;
    for (unsigned i = 0; i < DYNAMIC_CACHE_ENTRIES; i++)
        if (dynamic_cache_entries[i].occupied && !memcmp(dynamic_cache_entries[i].key, key, 24)) {
            s->hits++;
            dynamic_cache_entries[i].last = ++dynamic_cache_serial;
            *decode = false;
            return i;
        }
    s->misses++;
    unsigned x, y, id;
    for (;;) {
        for (id = 0; id < DYNAMIC_CACHE_ENTRIES && dynamic_cache_entries[id].occupied; id++)
            ;
        if (id < DYNAMIC_CACHE_ENTRIES && dynamic_cache_place(key[4] + 4, key[5] + 4, &x, &y))
            break;
        unsigned old = dynamic_cache_lru(s);
        if (old == DYNAMIC_CACHE_ENTRIES)
            return -1;
        dynamic_cache_mark(old, false);
        dynamic_cache_entries[old].occupied = false;
        s->evictions++;
    }
    DynamicCacheEntry *e = dynamic_cache_entries + id;
    memcpy(e->key, key, 24);
    e->x = x;
    e->y = y;
    e->w = key[4] + 4;
    e->h = key[5] + 4;
    e->generation = ++dynamic_cache_generation;
    e->last = ++dynamic_cache_serial;
    e->occupied = true;
    dynamic_cache_mark(id, true);
    *decode = true;
    return id;
}

#include "staging.h"
static DynamicCacheSlot *dynamic_cache_slot;
static const DynamicCacheSlot *dynamic_cache_drawing;
static unsigned dynamic_cache_page_index;
static GLuint dynamic_cache_pages[3], dynamic_cache_uv[3];
#ifdef GLES3_VALIDATION
static GLuint dynamic_cache_mapping[3];
#endif

static uint64_t dynamic_cache_resident[3][DYNAMIC_CACHE_ENTRIES];
static int dynamic_cache_last_recipe[7];
static bool dynamic_cache_key_valid;
extern uint8_t gles3_texture_pen(int, int, int);
static unsigned dynamic_cache_group(unsigned);
static unsigned dynamic_cache_material_page(unsigned);
static void dynamic_cache_upload(const rr_presentation *, unsigned);
static GLuint page_textures[16], mesh_texture, instance_texture, dynamic_mesh_texture, material_texture,
    shader_programs[2], vertex_array, attribute_buffer;
static unsigned page_count, mesh_width, mesh_height, material_count;
static int widths[16], heights[16];
static RenderMaterial material_data[MAX_MATERIALS];

static float instance_data[MAX_INSTANCES * 16];
static DrawAttribute attribute_data[MAX_QUADS];
#ifdef GLES3_VALIDATION
static uint8_t capture_pixels[640 * 480 * 4];
#endif

static size_t static_bytes;
static bool stage_sync;
static double clock_seconds(clockid_t kind)
{
    struct timespec t;
    clock_gettime(kind, &t);
    return t.tv_sec + t.tv_nsec / 1e9;
}
static FILE *open_asset(const char *dir, const char *name)
{
    char path[4096];
    RENDER_CHECK(snprintf(path, sizeof path, "%s/%s", dir, name) > 0);
    FILE *f = fopen(path, "rb");
    RENDER_CHECK(f);
    return f;
}
static void read_exact(FILE *f, void *data, size_t size)
{
    RENDER_CHECK(fread(data, 1, size, f) == size);
}
static void *read_asset(const char *dir, const char *name, size_t size)
{
    FILE *f = open_asset(dir, name);
    void *p = malloc(size ? size : 1);
    RENDER_CHECK(p);
    read_exact(f, p, size);
    RENDER_CHECK(fgetc(f) == EOF && !ferror(f));
    fclose(f);
    return p;
}
static GLuint create_texture(unsigned w, unsigned h, GLenum format, const void *data)
{
    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, GL_RGBA, format == GL_RGBA32F ? GL_FLOAT : GL_UNSIGNED_BYTE, data);
    RENDER_CHECK(glGetError() == GL_NO_ERROR);
    return t;
}
static GLuint compile_shader(const char *dir, const char *name, GLenum kind)
{
    FILE *f = open_asset(dir, name);
    RENDER_CHECK(!fseek(f, 0, SEEK_END));
    long n = ftell(f);
    RENDER_CHECK(n > 0 && n < 65536);
    rewind(f);
    char *s = calloc(n + 1, 1);
    RENDER_CHECK(s);
    read_exact(f, s, n);
    fclose(f);
    GLuint shader_ = glCreateShader(kind);
    glShaderSource(shader_, 1, (const char **)&s, NULL);
    glCompileShader(shader_);
    free(s);
    GLint ok;
    glGetShaderiv(shader_, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[8192];
        glGetShaderInfoLog(shader_, sizeof log, NULL, log);
        fprintf(stderr, "%s\n", log);
    }
    RENDER_CHECK(ok);
    return shader_;
}
#ifdef GLES3_VALIDATION
static bool provenance_debug;
static GLuint provenance_fbo, provenance_target, provenance_proof, provenance_texel, provenance_depth,
    provenance_coordinates[16], provenance_mapping;
static uint32_t provenance_pixels[640 * 480 * 4];
static uint8_t provenance_proof_pixels[640 * 480 * 4], provenance_texel_pixels[640 * 480 * 4];
extern void rr_validation_provenance(unsigned, const void *, const void *, const void *);
/* Nearest filtering is separable. Match each source axis's physical size and
 * sampler type/precision, with one 1D-in-2D coordinate texture per axis. */
static GLuint capture_ygrids[16];
static GLuint capture_axis(unsigned n, bool vertical)
{
    uint8_t bytes[4096 * 4];
    for (unsigned x = 0; x < n; x++) {
        bytes[x * 4] = x;
        bytes[x * 4 + 1] = x >> 8;
        bytes[x * 4 + 2] = 0;
        bytes[x * 4 + 3] = 0;
    }
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA8UI, vertical ? 1 : n, vertical ? n : 1, 0, GL_RGBA_INTEGER, GL_UNSIGNED_BYTE, bytes);
    return texture;
}
static void provenance_init(const char *dir)
{
    if (!provenance_debug)
        return;
    for (unsigned p = 0; p < page_count; p++) {
        provenance_coordinates[p] = capture_axis(widths[p], false);
        capture_ygrids[p] = capture_axis(heights[p], true);
    }
    unsigned mh = ((material_count + DYNAMIC_CACHE_MATERIALS + 1023) / 1024) * 2;
    void *m = read_asset(dir, "provenance-mapping.i32", 1024 * mh * 16);
    glGenTextures(1, &provenance_mapping);
    glBindTexture(GL_TEXTURE_2D, provenance_mapping);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32I, 1024, mh, 0, GL_RGBA_INTEGER, GL_INT, m);
    free(m);
    glGenFramebuffers(1, &provenance_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, provenance_fbo);
    glGenTextures(1, &provenance_target);
    glBindTexture(GL_TEXTURE_2D, provenance_target);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, 640, 480, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, provenance_target, 0);
    provenance_proof = create_texture(640, 480, GL_RGBA8, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, provenance_proof, 0);
    provenance_texel = create_texture(640, 480, GL_RGBA8, NULL);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, provenance_texel, 0);
    glGenRenderbuffers(1, &provenance_depth);
    glBindRenderbuffer(GL_RENDERBUFFER, provenance_depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, 640, 480);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, provenance_depth);
    GLenum buffers[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
    glDrawBuffers(3, buffers);
    RENDER_CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    RENDER_CHECK(glGetError() == GL_NO_ERROR);
}
static void provenance_begin(const RenderPacketHeader *h)
{
    if (!provenance_debug)
        return;
    glBindFramebuffer(GL_FRAMEBUFFER, provenance_fbo);
    GLenum bufs[] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2};
    glDrawBuffers(3, bufs);
    GLuint zero[4] = {0};
    float bg[] = {h->bg[0], h->bg[1], h->bg[2], 0}, z = 1, clear[4] = {0};
    glClearBufferuiv(GL_COLOR, 0, zero);
    glClearBufferfv(GL_COLOR, 1, bg);
    glClearBufferfv(GL_COLOR, 2, clear);
    glClearBufferfv(GL_DEPTH, 0, &z);
}
static void provenance_capture(unsigned tick)
{
    glBindFramebuffer(GL_READ_FRAMEBUFFER, provenance_fbo);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, 640, 480, GL_RGBA_INTEGER, GL_UNSIGNED_INT, provenance_pixels);
    glReadBuffer(GL_COLOR_ATTACHMENT1);
    glReadPixels(0, 0, 640, 480, GL_RGBA, GL_UNSIGNED_BYTE, provenance_proof_pixels);
    glReadBuffer(GL_COLOR_ATTACHMENT2);
    glReadPixels(0, 0, 640, 480, GL_RGBA, GL_UNSIGNED_BYTE, provenance_texel_pixels);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    rr_validation_provenance(tick, provenance_pixels, provenance_proof_pixels, provenance_texel_pixels);
    RENDER_CHECK(glGetError() == GL_NO_ERROR);
}
#endif
static void load_render_assets(const char *dir, const char *unused)
{
    (void)unused;
#ifdef GLES3_VALIDATION
    provenance_debug = getenv("ENG_GLES3_PROVENANCE") != NULL;
#endif

    FILE *f = open_asset(dir, "gles3.meta");
    char magic[8];
    RENDER_CHECK(fscanf(f, "%7s %u %u %u %u", magic, &page_count, &mesh_width, &mesh_height, &material_count) == 5);
    RENDER_CHECK(!strcmp(magic, "GLES3") && page_count > 0 && page_count < 16 && mesh_width == 1024 &&
                 mesh_height > 0 && mesh_height <= 8192 && material_count > 0 && material_count <= MAX_MATERIALS);
    fclose(f);
    f = open_asset(dir, "atlas.meta");
    unsigned pages;
    RENDER_CHECK(fscanf(f, "%u", &pages) == 1 && pages == page_count);
    for (unsigned i = 0; i < pages; i++)
        RENDER_CHECK(fscanf(f, "%d %d", widths + i, heights + i) == 2);
    fclose(f);
    for (unsigned i = 0; i < pages; i++) {
        RENDER_CHECK(widths[i] > 0 && widths[i] <= 4096 && heights[i] > 0 && heights[i] <= 4096);
        size_t n = (size_t)widths[i] * heights[i];
        static_bytes += n;
        char name[64];
        snprintf(name, sizeof name, "page-%u.r8ui", i);
        void *p = read_asset(dir, name, n);
        glGenTextures(1, page_textures + i);
        glBindTexture(GL_TEXTURE_2D, page_textures[i]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, widths[i], heights[i], 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, p);
        free(p);
        RENDER_CHECK(glGetError() == GL_NO_ERROR);
    }
    size_t n = (size_t)mesh_width * mesh_height * 16;
    void *p = read_asset(dir, "meshes.f32", n);
    mesh_texture = create_texture(mesh_width, mesh_height, GL_RGBA32F, p);
    free(p);
    static_bytes += n;
    p = read_asset(dir, "materials.bin", material_count * sizeof(RenderMaterial));
    memcpy(material_data, p, material_count * sizeof(RenderMaterial));
    free(p);
    p = read_asset(dir, "groups.bin", material_count * 4);
    memcpy(material_groups, p, material_count * 4);
    free(p);
    for (unsigned k = 0; k < 3; k++)
        palette_textures[k] = create_texture(256, 128, GL_RGBA8, NULL);
    unsigned mh = (material_count + DYNAMIC_CACHE_MATERIALS + 1023) / 1024;
    float *uv = calloc(1024 * mh * 4, 4);
    RENDER_CHECK(uv);
    for (unsigned i = 0; i < material_count; i++) {
        RENDER_CHECK(material_data[i].page < pages);
        memcpy(uv + i * 4, material_data[i].uv, 16);
    }
    for (unsigned k = 0; k < 3; k++)
        dynamic_cache_uv[k] = create_texture(1024, mh, GL_RGBA32F, uv);
    material_texture = dynamic_cache_uv[0];
    free(uv);
    instance_texture = create_texture(4, MAX_INSTANCES, GL_RGBA32F, NULL);
    dynamic_mesh_texture = create_texture(4096, 1, GL_RGBA32F, NULL);
    const char *names[] = {"world.vert", "world_near.vert"};
    for (unsigned i = 0; i < 2; i++) {
        const char *vertex_name = names[i];
        const char *fragment_name = "index_texture_lowp.frag";
#ifdef GLES3_VALIDATION
#ifdef GLES3_VALIDATION
        if (provenance_debug) {
            vertex_name = i ? "provenance_world_near.vert" : "provenance_world.vert";
            fragment_name = "provenance.frag";
        }
#endif

#endif
        GLuint v = compile_shader(dir, vertex_name, GL_VERTEX_SHADER);
        GLuint fs = compile_shader(dir, fragment_name, GL_FRAGMENT_SHADER);
        shader_programs[i] = glCreateProgram();
        glAttachShader(shader_programs[i], v);
        glAttachShader(shader_programs[i], fs);
        glLinkProgram(shader_programs[i]);
        GLint ok;
        glGetProgramiv(shader_programs[i], GL_LINK_STATUS, &ok);
        RENDER_CHECK(ok);
        glDeleteShader(v);
        glDeleteShader(fs);
        glUseProgram(shader_programs[i]);
        const char *samplers[] = {"image", "meshes", "instances", "dynamic_mesh", "materials", "palette"};
        for (unsigned k = 0; k < 6; k++)
            glUniform1i(glGetUniformLocation(shader_programs[i], samplers[k]), k);
#ifdef GLES3_VALIDATION
        if (provenance_debug) {
            glUniform1i(glGetUniformLocation(shader_programs[i], "coordinate_image"), 6);
            glUniform1i(glGetUniformLocation(shader_programs[i], "coordinate_y"), 8);
            glUniform1i(glGetUniformLocation(shader_programs[i], "source_mapping"), 7);
        }
#endif
    }
    glGenVertexArrays(1, &vertex_array);
    glGenBuffers(1, &attribute_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, attribute_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof attribute_data, NULL, GL_STREAM_DRAW);
    glBindVertexArray(vertex_array);
    for (unsigned i = 0; i < 2; i++) {
        glEnableVertexAttribArray(i);
        glVertexAttribDivisor(i, 1);
    }
    glBindVertexArray(0);

    frame_instances[0] = instance_texture;
    frame_dynamic[0] = dynamic_mesh_texture;
    frame_attributes[0] = attribute_buffer;
    for (int i = 1; i < 3; i++) {
        frame_instances[i] = create_texture(4, MAX_INSTANCES, GL_RGBA32F, NULL);
        frame_dynamic[i] = create_texture(4096, 1, GL_RGBA32F, NULL);
        glGenBuffers(1, &frame_attributes[i]);
        glBindBuffer(GL_ARRAY_BUFFER, frame_attributes[i]);
        glBufferData(GL_ARRAY_BUFFER, sizeof attribute_data, NULL, GL_STREAM_DRAW);
    }
    dynamic_cache_page_index = page_count++;
    RENDER_CHECK(page_count <= 16);
    widths[dynamic_cache_page_index] = heights[dynamic_cache_page_index] = DYNAMIC_CACHE_SIDE;
    for (unsigned k = 0; k < 3; k++) {
        glGenTextures(1, dynamic_cache_pages + k);
        glBindTexture(GL_TEXTURE_2D, dynamic_cache_pages[k]);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_R8UI,
                     DYNAMIC_CACHE_SIDE,
                     DYNAMIC_CACHE_SIDE,
                     0,
                     GL_RED_INTEGER,
                     GL_UNSIGNED_BYTE,
                     NULL);
    }
    page_textures[dynamic_cache_page_index] = dynamic_cache_pages[0];
#ifdef GLES3_VALIDATION
    provenance_init(dir);
#endif

#ifdef GLES3_VALIDATION
    if (provenance_debug) {
        dynamic_cache_mapping[0] = provenance_mapping;
        unsigned mh = ((material_count + DYNAMIC_CACHE_MATERIALS + 1023) / 1024) * 2;
        void *m = read_asset(dir, "provenance-mapping.i32", 1024 * mh * 16);
        for (unsigned k = 1; k < 3; k++) {
            glGenTextures(1, dynamic_cache_mapping + k);
            glBindTexture(GL_TEXTURE_2D, dynamic_cache_mapping[k]);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32I, 1024, mh, 0, GL_RGBA_INTEGER, GL_INT, m);
        }
        free(m);
    }
#endif

    glFinish();
    fprintf(stderr,
            "[GLES3-LOAD] pages=%u static_bytes=%zu dynamic_bytes=%u copies=3 index_only=1\n",
            page_count - 1,
            static_bytes,
            3 * DYNAMIC_CACHE_BYTES);
}
#ifdef GLES3_VALIDATION
extern void rr_validation_image(unsigned, const char *, const void *, size_t);
static void capture_image(const char *out, unsigned tick, const char *suffix)
{
    (void)out;
    glReadPixels(0, 0, 640, 480, GL_RGBA, GL_UNSIGNED_BYTE, capture_pixels);
    rr_validation_image(tick, suffix, capture_pixels, 640 * 480 * 4);
}
#endif
static uint16_t front_indices[MAX_QUADS];
static double last_end;
static GLuint readback_fbo, readback_color;
static void draw_render_packet(const RenderPacket *s, int pass, const char *out, double start, double waited)
{
    (void)out; /* Image output is compiled only in validation. */
    double cpu = clock_seconds(CLOCK_THREAD_CPUTIME_ID);
    RENDER_CHECK(s->size >= sizeof(RenderPacketHeader));
    const RenderPacketHeader *h = (const RenderPacketHeader *)s->bytes;
    RENDER_CHECK(h->magic == 0x33504e52 && h->version == 3 && h->tick == s->tick && h->n <= MAX_QUADS &&
                 h->visible <= h->n && h->groups <= MAX_QUADS && h->instances <= MAX_INSTANCES && h->dynamic <= 4096);
    size_t offset = sizeof *h;
    const RenderInstance *ins = (const RenderInstance *)(s->bytes + offset);
    offset += h->instances * sizeof *ins;
    const RenderQuad *q = (const RenderQuad *)(s->bytes + offset);
    offset += h->n * sizeof *q;
    const uint16_t *visible = (const uint16_t *)(s->bytes + offset);
    offset += (h->visible * 2 + 3) & ~3u;
    const DrawGroup *groups = (const DrawGroup *)(s->bytes + offset);
    offset += h->groups * sizeof *groups;
    const float *dynamic = (const float *)(s->bytes + offset);
    offset += h->dynamic * 16;
    offset += 8192;
    RENDER_CHECK(offset == s->size);
    if (gles3_front) {
        memcpy(front_indices, visible, h->visible * 2);
        for (unsigned group_index = 0; group_index < h->groups; group_index++) {
            const DrawGroup *group = groups + group_index;
            if (group->mode)
                continue;
            RENDER_CHECK(group->first + group->count <= h->visible);
            for (unsigned instance_index = 0; instance_index < group->count; instance_index++) {
                unsigned original = visible[group->first + instance_index];
                RENDER_CHECK(original < h->n);
                if (instance_index) {
                    unsigned before = visible[group->first + instance_index - 1];
                    RENDER_CHECK(q[before].depth - q[original].depth > .0003f);
                }
                front_indices[group->first + group->count - 1 - instance_index] = original;
            }
        }
        visible = front_indices;
    }
    unsigned culled = 0;
    for (unsigned i = 0; i < h->n; i++) {
        RENDER_CHECK(q[i].instance < h->instances && q[i].material < material_count + DYNAMIC_CACHE_MATERIALS);
        culled += (q[i].flags >> 8) != 0;
    }
    RENDER_CHECK(culled + h->visible == h->n);
    for (unsigned i = 0; i < h->instances; i++)
        memcpy(instance_data + i * 16, ins[i].data, 64);
    for (unsigned i = 0; i < h->visible; i++) {
        RENDER_CHECK(visible[i] < h->n);
        const RenderQuad *r = q + visible[i];
        RENDER_CHECK(!(r->flags >> 8));
        DrawAttribute *a = attribute_data + i;
        a->state[0] = r->mesh;
        a->state[1] = r->instance;
        a->state[2] = r->depth;
        a->state[3] = ((r->material + 1) * 128 + dynamic_cache_group(r->material)) * (r->flags & 1 ? -1.f : 1.f);
        for (unsigned j = 0; j < 4; j++)
            a->brightness[j] = r->brightness[j] / 64.f;
    }
    double prepared = clock_seconds(CLOCK_MONOTONIC);
    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, instance_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 4, h->instances, GL_RGBA, GL_FLOAT, instance_data);
    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, dynamic_mesh_texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, h->dynamic, 1, GL_RGBA, GL_FLOAT, dynamic);
    glBindBuffer(GL_ARRAY_BUFFER, attribute_buffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, h->visible * sizeof(DrawAttribute), attribute_data);
    double uploaded = clock_seconds(CLOCK_MONOTONIC);
    rr_video_layers_begin();
    glViewport(0, 0, 640, 480);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glClearColor(h->bg[0], h->bg[1], h->bg[2], 0);
    glClearDepthf(1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
#ifdef GLES3_VALIDATION
    provenance_begin(h);
#endif

    glBindVertexArray(vertex_array);
    GLuint textures[] = {0, mesh_texture, instance_texture, dynamic_mesh_texture, material_texture};
    for (unsigned i = 1; i < 5; i++) {
        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(GL_TEXTURE_2D, textures[i]);
    }
    glActiveTexture(GL_TEXTURE0);
    glEnable(GL_SCISSOR_TEST);
    unsigned accounted = 0, vertices = 0;
    for (unsigned i = 0; i < h->groups; i++) {
        const DrawGroup *g = groups + i;
        RENDER_CHECK(g->first == accounted && g->count > 0 && g->first + g->count <= h->visible &&
                     g->page < page_count && g->mode <= 3 && g->box[2] >= 0 && g->box[3] >= 0);
        accounted += g->count;
        unsigned near = g->mode & 1;
        glUseProgram(shader_programs[near]);
        glBindTexture(GL_TEXTURE_2D, page_textures[g->page]);
#ifdef GLES3_VALIDATION
        if (provenance_debug) {
            glActiveTexture(GL_TEXTURE6);
            glBindTexture(GL_TEXTURE_2D, provenance_coordinates[g->page]);
            glActiveTexture(GL_TEXTURE8);
            glBindTexture(GL_TEXTURE_2D, capture_ygrids[g->page]);
            glActiveTexture(GL_TEXTURE7);
            glBindTexture(GL_TEXTURE_2D, provenance_mapping);
            glActiveTexture(GL_TEXTURE0);
        }
#endif

        glScissor(g->box[0], g->box[1], g->box[2], g->box[3]);
        glBindBuffer(GL_ARRAY_BUFFER, attribute_buffer);
        for (unsigned k = 0; k < 2; k++)
            glVertexAttribPointer(k,
                                  4,
                                  GL_FLOAT,
                                  GL_FALSE,
                                  sizeof(DrawAttribute),
                                  (void *)(uintptr_t)(g->first * sizeof(DrawAttribute) + k * 16));
        glDrawArraysInstanced(GL_TRIANGLES, 0, near ? 12 : 6, g->count);
        vertices += (near ? 12 : 6) * g->count;
    }
    RENDER_CHECK(accounted == h->visible);
    double world_submit = clock_seconds(CLOCK_MONOTONIC);
    if (stage_sync)
        glFinish();
    double world_done = clock_seconds(CLOCK_MONOTONIC);
#ifdef GLES3_VALIDATION
    bool capture = strcmp(out, "-") && pass == 0 &&
                   (getenv("ENG_GLES3_SEQUENCE") || h->tick == 1800 || h->tick == 1965 || h->tick == 2500 ||
                    h->tick == 2643 || h->tick == 3000 || h->tick == 3546 || h->tick == 3600 || h->tick == 4200 ||
                    h->tick == 4800 || h->tick == 5400 || h->tick == 5639 || h->tick == 5655 || h->tick == 5867 ||
                    h->tick == 5874 || h->tick == 5878 || h->tick == 5889 || h->tick == 5921 || h->tick == 6006);
#ifdef GLES3_VALIDATION
    /* Exact, bounded image samples for local replay regressions. */
    const char *samples = getenv("ENG_GLES3_CAPTURE_TICKS");
    if (samples) {
        capture = false;
        while (*samples) {
            char *end;
            unsigned t = strtoul(samples, &end, 10);
            if (end == samples)
                break;
            if (t == h->tick)
                capture = strcmp(out, "-") && pass == 0;
            samples = *end == ',' ? end + 1 : end;
        }
    }
    if (capture && provenance_debug) {
        static FILE *f;
        const char *p = getenv("ENG_GLES3_PALETTE_REFERENCE");
        if (p) {
            if (!f)
                f = fopen(p, "wx");
            RENDER_CHECK(f);
            uint32_t tick = h->tick;
            RENDER_CHECK(fwrite(&tick, 4, 1, f) == 1 && fwrite(gles3_drawing->pal, 1, RR_PAL_SIZE, f) == RR_PAL_SIZE);
        }
    }
#endif
    if (capture) {
        if (provenance_debug)
            provenance_capture(h->tick);
        else
            capture_image(out, h->tick, "world");
    }
#endif
    glBindVertexArray(0);
    RENDER_CHECK(rr_gl_video_submit(frame_hud));
    double hud_submit = clock_seconds(CLOCK_MONOTONIC);
    if (frame_diagnostic)
        glFinish();
    double end = clock_seconds(CLOCK_MONOTONIC);
    double interval = last_end ? end - last_end : end - start;
    if (pass >= 0)
        last_end = end;
    end_cpu = clock_seconds(CLOCK_THREAD_CPUTIME_ID);
    getrusage(RUSAGE_THREAD, &end_usage);
    double gc = clock_seconds(CLOCK_MONOTONIC), gcc = clock_seconds(CLOCK_THREAD_CPUTIME_ID);
    RENDER_CHECK(glGetError() == GL_NO_ERROR);
    check_wall = clock_seconds(CLOCK_MONOTONIC) - gc;
    check_cpu = clock_seconds(CLOCK_THREAD_CPUTIME_ID) - gcc;
    trace_wall = trace_cpu = 0;
    if (frame_diagnostic) {
        printf("%s pass=%d tick=%u primitives=%u visible=%u culled=%u draws=%u vertices=%u instances=%u "
               "dynamic_bytes=%zu wait_ms=%.6f read_ms=%.6f inflate_ms=%.6f worker_cpu_ms=%.6f prepare_ms=%.6f "
               "upload_ms=%.6f world_submit_ms=%.6f world_sync_ms=%.6f hud_submit_ms=%.6f finish_ms=%.6f total_ms=%.6f "
               "cpu_ms=%.6f interval_ms=%.6f\n",
               pass < 0 ? "WARM" : "FRAME",
               pass,
               h->tick,
               h->n,
               h->visible,
               culled,
               h->groups,
               vertices,
               h->instances,
               h->visible * sizeof(DrawAttribute) + h->instances * 64 + h->dynamic * 16,
               waited,
               s->read_ms,
               s->inflate_ms,
               s->worker_cpu_ms,
               (prepared - start) * 1000 - waited,
               (uploaded - prepared) * 1000,
               (world_submit - uploaded) * 1000,
               (world_done - world_submit) * 1000,
               (hud_submit - world_done) * 1000,
               (end - hud_submit) * 1000,
               (end - start) * 1000,
               (clock_seconds(CLOCK_THREAD_CPUTIME_ID) - cpu) * 1000,
               interval * 1000);
    }

#ifdef GLES3_VALIDATION
    if (capture && !provenance_debug)
        capture_image(out, h->tick, "composed");
#endif
    if (pass >= 0 && frame_diagnostic)
        printf("CAPTURE_FRAME tick=%u upload_ms=%.6f upload_cpu_ms=%.6f groups=%u bytes=%u render_ms=%.6f fence=%d\n",
               h->tick,
               capture_upload_wall,
               capture_upload_cpu,
               capture_groups,
               capture_groups * 1024,
               (end - start) * 1000 + capture_upload_wall,
               frame_diagnostic);
}
/* Diagnostic only, compiled against private geometry source. MIT. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "rr_mem.h"
#include "rr_dsp.h"
#include "rr_scene.h"
static unsigned object_packet, object_instance;
static int object_brightness[4];
/*
 * geo_hw.c — the SS22 geometry stage, ported from a validated oracle.
 *
 * WHY THIS EXISTS
 * ---------------
 * renderer_3d.c projects with an invented OpenGL camera (a hardcoded 90
 * degree FOV). The hardware has no field-of-view angle at all: the DSP
 * transforms vertices with a Q15 matrix and projects with an integer
 * divide against a focal length ("zoom"). Comparing the decompile's
 * attract screen against MAME's shows the two share nothing — MAME draws
 * a sky dome, clouds and the flyer; the decompile draws one enormous
 * mis-scaled quad. No amount of frustum tuning fixes that, because the
 * whole transform stage is missing rather than mistuned.
 *
 * THE ORACLE
 * ----------
 * This is a direct port of tools/pc_geo_fixed.py from the Prop Cycle
 * MiSTer project, which is gated byte-exact against MAME's own output on
 * four captured frames (sim: `make geo-run`, frames 1500/2400/3600/4800).
 * Every arithmetic step below mirrors it deliberately, including the
 * truncating divide and the 1/16-pixel screen coordinates. Where this
 * file and that Python disagree, the Python is right.
 *
 * Reference for the projection, namcos22_v.cpp (~line 296):
 *     cx = 320 + vx;  cy = 240 + vy
 *     screen_x = cx + (X * zoom) / Z
 *     screen_y = cy - (Y * zoom) / Z
 *
 * NOT PORTED YET (deliberate, and each is a visible gap):
 *   - lighting / per-vertex bri beyond the flat and gouraud cases
 *   - point-RAM objects (code 0x5)
 *   - the 0x8008 / 0x800A matrix sections, which the master-DSP HLE
 *     (tools/pc_master_model.py) also lists as unmodelled
 */

#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "eng.h"
#include "geo_hw.h"
#include "geo_projection_profile.inc"
#ifdef GLES3_VALIDATION
int g_objlog_on; /* ENG_OBJLOG: set by slave_list.c while it logs an object; here because every game links geo_hw.c (a
                    weak extern fails to link on Windows) */
#endif

/* ---- point data ----------------------------------------------------------
 * The board's point ROM (sign-extended 24-bit at load time, which matches the
 * oracle's pt.read()) and, on System 22, point RAM past it (eng.h). */
static inline int32_t pt_read(uint32_t addr)
{
#ifdef RR_GEOMETRY_PROJECTION_PROFILE
    if (GP_ACTIVE)
        gp_reads[addr >= g_eng_pointrom_n]++;
#endif
    return eng_point_read(addr);
}

/* Truncating signed division. C already truncates toward zero, which is
 * what the oracle's sdiv() reimplements because Python's // floors. */
static inline int32_t sdiv(int64_t a, int64_t b)
{
    if (b == 0)
        return 0;
    return (int32_t)(a / b);
}

/* Floor division / arithmetic shift, matching Python's // and >>, which
 * floor for negatives where C truncates. */
static inline int64_t fdiv64(int64_t a, int64_t b)
{
    int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0)))
        q--;
    return q;
}
static inline int64_t fshr64(int64_t v, int n)
{
    return v >> n;
}

/* The oracle is Python, whose ints are unbounded; ours are not. On
 * near-degenerate clip edges the lerp parameter t gets very large and
 * (b-a)*t overflows int64 -- silently, producing a mid-range screen
 * coordinate where the oracle produces a huge one that then saturates.
 * tools/frame_gate.py caught this as sx16/sy16 deltas on exactly the
 * partially-clipped quads, with the oracle at INT32_MAX/MIN and us at
 * arbitrary values. Doing the wide steps in __int128 restores parity.
 *
 * Shifts must FLOOR (Python >>) and the divide must TRUNCATE (the oracle
 * defines sdiv with C semantics); __int128 gives both gles3ly. */
static inline int64_t mulshr128(int64_t a, int64_t b, int n)
{
    return (int64_t)(((__int128)a * (__int128)b) >> n);
}

/* base16 +/- ((v * mant) << 4 >> shift) / z, saturated to int32. */
static inline int32_t
project_saturated_general(int64_t base16, int64_t v, int32_t mant, int shift, int64_t z, int negate)
{
#ifdef RR_GEOMETRY_PROJECTION_PROFILE
    if (gp_trace && g_eng_frame == 1500) {
        if (gp_records_n < 32768)
            gp_records[gp_records_n++] = (gp_record){base16, v, z, mant, shift, negate, 0};
        else
            gp_records_overflow++;
    }
#endif
    /* Shift through unsigned: v*mant is routinely negative and shifting a
     * negative value left is undefined behaviour (UBSan flags it). The
     * two's-complement bit pattern is identical, so the result is
     * unchanged -- this is a correctness-of-language fix, not a maths one. */
    __int128 num = (__int128)((unsigned __int128)((__int128)v * (__int128)mant) << 4);
    num >>= shift;                          /* arithmetic: floors, like Python */
    __int128 q = z ? num / (__int128)z : 0; /* truncating, like sdiv() */
    __int128 r = (__int128)base16 + (negate ? -q : q);
    if (r > (__int128)INT32_MAX)
        return INT32_MAX;
    if (r < (__int128)INT32_MIN)
        return INT32_MIN;
    return (int32_t)r;
}

static _Thread_local bool use_exact_projection;
#include "projection.h"
static inline int32_t proj_sat(int64_t b, int64_t v, int32_t m, int s, int64_t z, int n)
{
    return use_exact_projection ? project_saturated_exact(b, v, m, s, z, n)
                                : project_saturated_general(b, v, m, s, z, n);
}

static inline int32_t sat32(int64_t v)
{
    if (v > (int64_t)INT32_MAX)
        return INT32_MAX;
    if (v < (int64_t)INT32_MIN)
        return INT32_MIN;
    return (int32_t)v;
}

static inline int32_t sext(uint32_t v, int bits)
{
    /* MASK FIRST. Without the mask this sign-extends the low `bits` but
     * lets everything above them through, so sext(polyshift,18) leaked the
     * ap field (0x1C0000) straight into zsort -- caught by tools/geo_gate.py
     * as a zsort delta of exactly 0x1C0000 while every vertex matched. */
    uint32_t m = 1u << (bits - 1);
    v &= (1u << bits) - 1u;
    return (int32_t)((v ^ m) - m);
}

/* ---- view state -------------------------------------------------------- */
static geo_view g_view;

/* ---- per-object lighting (GeoFixed.register_normals_fixed) --------------
 * Normals packets inside an object publish per-vertex lit values that take
 * precedence over both the gouraud and flat brightness fields. Leaving them
 * unmodelled showed up in tools/frame_gate.py as b_f mismatches on exactly
 * the objects that carry them (76 vertices at frame 1500). */
#define LIT_MAX 512
static int32_t g_lit[LIT_MAX];
static int g_lit_n, g_lit_idx;

static void register_normals(uint32_t addr)
{
    const int32_t (*v)[3] = g_view.viewq;
    const int32_t (*m)[3] = g_view.m;
    int32_t cl[3];
    for (int c = 0; c < 3; c++)
        cl[c] = (int32_t)(((int64_t)g_view.light[0] * v[0][c] + (int64_t)g_view.light[1] * v[1][c] +
                           (int64_t)g_view.light[2] * v[2][c]) >>
                          15);
    for (int i = 0; i < 4; i++) {
        int32_t n[3], nt[3];
        for (int k = 0; k < 3; k++)
            n[k] = sext((uint32_t)pt_read(addr + i * 3 + k) & 0xffff, 16);
        for (int c = 0; c < 3; c++)
            nt[c] = (int32_t)(((int64_t)n[0] * m[0][c] + (int64_t)n[1] * m[1][c] + (int64_t)n[2] * m[2][c]) >> 15);
        int64_t dot = (int64_t)nt[0] * cl[0] + (int64_t)nt[1] * cl[1] + (int64_t)nt[2] * cl[2];
        if (dot < 0)
            dot = 0;
        if (g_lit_n < LIT_MAX)
            g_lit[g_lit_n++] = g_view.ambient + (int32_t)(((int64_t)g_view.power * dot) >> 30);
    }
}

/* Reject census. A frame that draws nothing has many possible causes and
 * the screen cannot tell them apart; these can. */
geo_stats g_geo_stats;
unsigned g_eng_frame;
int g_bbox_cur = -1;
/* the object code being walked (pickers, probes) */ /* the game's frame count, for diagnostics only */

int g_zord_ap = 0, g_zord_os = 0; /* last view's priority fields, for PROPCYCL_ZORD */

void geo_hw_set_view(const geo_view *v)
{
    g_zord_ap = v->absolute_priority;
    g_zord_os = v->objectshift;
    g_view = *v;
}

void geo_hw_zoom_from_dspfloat(uint32_t word, int32_t *mant, int *shift)
{
    *mant = sext(word & 0xffff, 16);
    int exp = (word >> 16) & 0x3f;
    int s = 0x2e - exp;
    *shift = s < 0 ? 0 : s;
}

/* ---- one quad ----------------------------------------------------------
 * Direct port of GeoFixed.quad_fixed(). `addr` is the PACKET DATA base,
 * not the raw object address — getting that base wrong is exactly the
 * defect TEXTURE_PIPELINE_BUGS.md records as Bug 2, and it is why the
 * existing renderer draws the wrong geometry.
 */
static void quad_fixed(
    int32_t color, uint32_t addr, int32_t polyshift, int32_t flags, int32_t packetformat, geo_quad_cb cb, void *user)
{
#ifdef RR_GEOMETRY_PROJECTION_PROFILE
    int gp_active = GP_ACTIVE;
    double gp_start = gp_active ? gp_clock() : 0;
#endif
    const int32_t (*m)[3] = g_view.m;
    const int32_t *t = g_view.t;

    int32_t vx[4], vy[4], vz[4];
    for (int i = 0; i < 4; i++) {
        int32_t x = pt_read(0x8 + i * 3 + addr);
        int32_t y = pt_read(0x9 + i * 3 + addr);
        int32_t z = pt_read(0xa + i * 3 + addr);
        vx[i] = (int32_t)(((int64_t)x * m[0][0] + (int64_t)y * m[1][0] + (int64_t)z * m[2][0]) >> 15) + t[0];
        vy[i] = (int32_t)(((int64_t)x * m[0][1] + (int64_t)y * m[1][1] + (int64_t)z * m[2][1]) >> 15) + t[1];
        vz[i] = (int32_t)(((int64_t)x * m[0][2] + (int64_t)y * m[1][2] + (int64_t)z * m[2][2]) >> 15) + t[2];
    }

#ifdef RR_GEOMETRY_PROJECTION_PROFILE
    if (gp_active) {
        gp_transform += gp_clock() - gp_start;
        gp_transforms++;
    }
#endif
    int32_t zmax = vz[0], zmin = vz[0];
    for (int i = 1; i < 4; i++) {
        if (vz[i] > zmax)
            zmax = vz[i];
        if (vz[i] < zmin)
            zmin = vz[i];
    }
    if (zmax < 0) {
        g_geo_stats.rej_behind++;
        return;
    } /* wholly behind */

    /* backface cull, two cross products, verbatim from the oracle */
    if (flags & 0x20) {
        int64_t c1 = (int64_t)vx[2] * ((int64_t)vz[0] * vy[1] - (int64_t)vy[0] * vz[1]) +
                     (int64_t)vy[2] * ((int64_t)vx[0] * vz[1] - (int64_t)vz[0] * vx[1]) +
                     (int64_t)vz[2] * ((int64_t)vy[0] * vx[1] - (int64_t)vx[0] * vy[1]);
        int64_t c2 = (int64_t)vx[0] * ((int64_t)vz[2] * vy[3] - (int64_t)vy[2] * vz[3]) +
                     (int64_t)vy[0] * ((int64_t)vx[2] * vz[3] - (int64_t)vz[2] * vx[3]) +
                     (int64_t)vz[0] * ((int64_t)vy[2] * vx[3] - (int64_t)vx[2] * vy[3]);
        if ((g_view.cullflip && c1 <= 0 && c2 <= 0) || (!g_view.cullflip && c1 >= 0 && c2 >= 0)) {
            g_geo_stats.rej_cull++;
#ifdef GLES3_VALIDATION
            /* PROPCYCL_CULLDUMP=<code>: name every back-face-culled quad of one
             * object, with its projected corners -- a hole INSIDE one model is
             * either a quad culled here or one lost to the near clip. Diagnostic
             * only; the oracle path is unchanged. */
            {
                static int cd = -2;
                extern int g_bbox_cur;
                if (cd == -2) {
                    const char *e = getenv("PROPCYCL_CULLDUMP");
                    cd = e ? atoi(e) : -1;
                }
                if (cd >= 0 && g_bbox_cur == cd) {
                    fprintf(
                        stderr, "[CULL] f%u code %d c1=%lld c2=%lld", g_eng_frame, cd, (long long)c1, (long long)c2);
                    for (int i = 0; i < 4; i++)
                        if (vz[i] > 0)
                            fprintf(stderr,
                                    " (%.3f,%.3f,%d)",
                                    proj_sat((int64_t)(320 + g_view.vx) * 16,
                                             vx[i],
                                             g_view.zoom_mant,
                                             g_view.zoom_shift,
                                             vz[i],
                                             0) /
                                        16.0,
                                    proj_sat((int64_t)(240 + g_view.vy) * 16,
                                             vy[i],
                                             g_view.zoom_mant,
                                             g_view.zoom_shift,
                                             vz[i],
                                             1) /
                                        16.0,
                                    vz[i]);
                    fprintf(stderr, "\n");
                }
            }
#endif

            return;
        }
    }

    if (zmin < 0)
        zmin = 0;
    int32_t zalg = flags & 0x300;
    int32_t zsort = (zalg == 0x000) ? zmin : (zalg == 0x100) ? zmax : ((zmin + zmax) >> 1);
    if (zsort > 0x1fffff)
        zsort = 0x1fffff;
    int ap = g_view.absolute_priority & 7;
    if (polyshift & 0x200000)
        zsort = polyshift & 0x1fffff;
    else {
        zsort += sext(polyshift, 18);
        ap += (polyshift & 0x1c0000) >> 18;
    }
    if (g_view.objectshift & 0x200000)
        zsort = g_view.objectshift & 0x1fffff;
    else {
        zsort += sext(g_view.objectshift, 18);
        ap += (g_view.objectshift & 0x1c0000) >> 18;
    }
    if (zsort < 0)
        zsort = 0;
    if (zsort > 0x1fffff)
        zsort = 0x1fffff;
    zsort |= (ap & 7) << 21;

    /* projection: 1/16-pixel screen coordinates */
    const int32_t mant = g_view.zoom_mant;
    const int shift = g_view.zoom_shift;
    const int32_t cx = 320 + g_view.vx;
    const int32_t cy = 240 + g_view.vy;

    geo_quad q;
    memset(&q, 0, sizeof q);
    q.color = (uint32_t)color;
    q.zsort = zsort;
    q.behind = 0;

    uint32_t uv_raw[8];
    object_packet = addr;
    for (int k = 0; k < 8; k++)
        uv_raw[k] = (uint32_t)pt_read(k + addr) & 0xffffff;

    /* Per-vertex brightness, resolved ONCE per quad: the lit path advances
     * a shared index, so evaluating it twice (raw pass + clip pass) would
     * consume the table at double rate. */
    int briv[4];
    for (int i = 0; i < 4; i++) {
        if (g_lit_n > 0) {
            int idx = g_lit_idx++;
            if (g_lit_n > 4)
                idx >>= 2;
            briv[i] = g_lit[((idx % g_lit_n) + g_lit_n) % g_lit_n];
        } else if (packetformat & 0x40) {
            briv[i] = (int)((uv_raw[i] >> 16) & 0xff); /* gouraud */
        } else {
            briv[i] = (int)((color >> 16) & 0xff); /* flat */
        }
    }
    memcpy(object_brightness, briv, sizeof briv);
    q.cmode = (uv_raw[0] >> 12) & 0xf; /* oracle: same fields */
    q.texbank = (uv_raw[1] >> 12) & 0xf;
    q.cz_type = (flags >> 10) & 3; /* pc_geo_fixed.py line 290 */
    q.flags_raw = flags & 0xffffff;
    q.cz_adjust = g_view.cz_adjust;
    q.objectflags = g_view.objectflags;
    if (g_view.have_clip) {
        /* pc_raster_model.py: clip = (cx+vl, cx-vr-1, cy+vu, cy-vd-1),
         * then int() truncation. C casts truncate the same way. */
        q.clip[0] = (int32_t)((float)cx + g_view.cl);
        q.clip[1] = (int32_t)((float)cx - g_view.cr - 1.0f);
        q.clip[2] = (int32_t)((float)cy + g_view.cu);
        q.clip[3] = (int32_t)((float)cy - g_view.cd - 1.0f);
    } else {
        q.clip[0] = 0;
        q.clip[1] = 639;
        q.clip[2] = 0;
        q.clip[3] = 479;
    }
    { /* cz_value = min(0x1fffff, zmax) >> 8, computed from the PRE-clamp
       * zmax (the oracle uses the raw zmax, not the zsort-clamped one). */
        int32_t zc = zmax;
        if (zc > 0x1fffff)
            zc = 0x1fffff;
        q.cz_value = zc >> 8;
    }

#ifdef RR_GEOMETRY_PROJECTION_PROFILE
    if (gp_active)
        gp_start = gp_clock();
#endif
    /* raw per-vertex projection -- parity with the oracle's scr16 */
    for (int i = 0; i < 4; i++) {
        if (vz[i] <= 0) {
            q.v[i].valid = 0;
            q.behind = 1;
            continue;
        }
#ifdef RR_GEOMETRY_PROJECTION_PROFILE
        if (gp_active)
            gp_raw_calls += 2;
#endif
        q.v[i].sx16 = proj_sat((int64_t)cx * 16, vx[i], mant, shift, vz[i], 0);
        q.v[i].sy16 = proj_sat((int64_t)cy * 16, vy[i], mant, shift, vz[i], 1);
        q.v[i].z = vz[i];
        q.v[i].u = uv_raw[2 * i] & 0xfff;
        q.v[i].v = uv_raw[2 * i + 1] & 0xfff;
        q.v[i].bri = briv[i];
        q.v[i].valid = 1;
    }

#ifdef RR_GEOMETRY_PROJECTION_PROFILE
    if (gp_active) {
        gp_raw += gp_clock() - gp_start;
        gp_quads++;
        gp_reusable += !q.behind;
    }
#endif
    { /* whole-quad UV box, for the live texture cache (see geo_hw.h) */
        int u0 = 0xfff, u1 = 0, v0 = 0xfff, v1 = 0;
        for (int i = 0; i < 4; i++) {
            int uu = uv_raw[2 * i] & 0xfff, vv = uv_raw[2 * i + 1] & 0xfff;
            if (uu < u0)
                u0 = uu;
            if (uu > u1)
                u1 = uu;
            if (vv < v0)
                v0 = vv;
            if (vv > v1)
                v1 = vv;
        }
        q.uvbox[0] = (uint16_t)u0;
        q.uvbox[1] = (uint16_t)u1;
        q.uvbox[2] = (uint16_t)v0;
        q.uvbox[3] = (uint16_t)v1;
    }

#ifdef RR_GEOMETRY_PROJECTION_PROFILE
    if (gp_active)
        gp_start = gp_clock();
#endif
    double pre_x[6], pre_y[6]; /* the near-clipped polygon, before projection */
    int n_pre = 0;
    /* ---- near-plane clip, then project ---------------------------------
     * Port of the oracle's clip lerp (UF=16 attribute fraction bits,
     * TF=24 parameter bits). Dropping partially-clipped quads whole is what
     * tore black bands through the sky; interpolating them is what the
     * hardware does.
     *
     * Python's // and >> floor; C truncates toward zero. fdiv/fshr below
     * keep the arithmetic identical for negative operands. */
    {
        const int UF = 16, TF = 24;
        int64_t px_[4], py_[4], pz_[4], pu_[4], pv_[4], pb_[4];
        for (int i = 0; i < 4; i++) {
            px_[i] = vx[i];
            py_[i] = vy[i];
            pz_[i] = vz[i];
            pu_[i] = (int64_t)(uv_raw[2 * i] & 0xfff) << UF;
            pv_[i] = (int64_t)(uv_raw[2 * i + 1] & 0xfff) << UF;
            pb_[i] = (int64_t)briv[i] << UF;
        }
        int64_t cx_[6], cy_[6], cz_[6], cu_[6], cv_[6], cb_[6];
        int n = 0;
        for (int i = 0; i < 4 && n < 6; i++) {
            int j = (i + 1) & 3;
            int ain = pz_[i] >= 1, bin = pz_[j] >= 1;
            if (ain) {
                cx_[n] = px_[i];
                cy_[n] = py_[i];
                cz_[n] = pz_[i];
                cu_[n] = pu_[i];
                cv_[n] = pv_[i];
                cb_[n] = pb_[i];
                n++;
            }
            if (ain != bin && n < 6) {
                int64_t den = pz_[j] - pz_[i];
                /* named lerp_t, not t: `t` up-scope is the VIEW TRANSLATION
                 * vector, and shadowing it here invites a later edit to
                 * reach for the wrong one. */
                /* (1 - pz) is negative whenever the vertex is in front of
                 * the near plane, so shift through unsigned (see proj_sat). */
                int64_t lerp_t = den ? fdiv64((int64_t)((uint64_t)(1 - pz_[i]) << TF), den) : 0;
                cx_[n] = px_[i] + mulshr128(px_[j] - px_[i], lerp_t, TF);
                cy_[n] = py_[i] + mulshr128(py_[j] - py_[i], lerp_t, TF);
                cu_[n] = pu_[i] + mulshr128(pu_[j] - pu_[i], lerp_t, TF);
                cv_[n] = pv_[i] + mulshr128(pv_[j] - pv_[i], lerp_t, TF);
                cb_[n] = pb_[i] + mulshr128(pb_[j] - pb_[i], lerp_t, TF);
                cz_[n] = 1; /* on the plane by definition */
                n++;
            }
        }
        q.nrv = (n >= 3) ? n : 0;
        n_pre = n;
        for (int i = 0; i < n && i < 6; i++) {
            pre_x[i] = (double)cx_[i];
            pre_y[i] = (double)cy_[i];
        }
        for (int i = 0; i < n && i < 6; i++) {
#ifdef RR_GEOMETRY_PROJECTION_PROFILE
            if (gp_active) {
#ifndef RR_GEOMETRY_REUSE_PROJECTION
                gp_near_calls += 2;
#else
                if (q.behind)
                    gp_near_calls += 2;
#endif
            }
#endif
#ifdef RR_GEOMETRY_REUSE_PROJECTION
            /* No raw vertex behind the eye means every integer Z >= 1.
             * The near clip therefore emits exactly the four original vertices
             * in order, with identical X/Y/Z and projection parameters. Reuse
             * only screen coordinates; all clipped attributes/bookkeeping stay.
             * Partial clips retain the wide-arithmetic oracle path below. */
            if (!q.behind) {
                q.rv[i].sx16 = q.v[i].sx16;
                q.rv[i].sy16 = q.v[i].sy16;
            } else
#endif
            {
                q.rv[i].sx16 = proj_sat((int64_t)cx * 16, cx_[i], mant, shift, cz_[i], 0);
                q.rv[i].sy16 = proj_sat((int64_t)cy * 16, cy_[i], mant, shift, cz_[i], 1);
            }
            q.rv[i].z = (int32_t)cz_[i];
            q.rv[i].u = (uint32_t)(cu_[i] >> UF) & 0xfff;
            q.rv[i].v = (uint32_t)(cv_[i] >> UF) & 0xfff;
            q.rv[i].bri = (int)(cb_[i] >> UF);
            q.rv[i].uf = (int32_t)cu_[i];
            q.rv[i].vf = (int32_t)cv_[i];
            q.rv[i].bf = (int32_t)cb_[i];
            q.rv[i].valid = 1;
        }
    }
#ifdef RR_GEOMETRY_PROJECTION_PROFILE
    if (gp_active)
        gp_near += gp_clock() - gp_start;
#endif
    /* ---- guard-band clip (register row 187) ------------------------------
     * A vertex put ON the near plane (z = 1) projects to billions of 1/16 px,
     * and proj_sat clamps x and y to +-2^31 INDEPENDENTLY -- which bends the
     * polygon's edges. On SOLITAR the cloud-floor sheet (code 112) has quads
     * with one vertex in front of the eye and three behind; banking turned
     * their clamped corners into a flat wedge across the sky. MAME rasterises
     * in float and clips to the screen, so the shape survives. Here the
     * near-clipped polygon is clipped in 3D against four planes a guard band
     * outside the screen (exact: every attribute is linear in 3D), and that
     * polygon is what gets DRAWN. rv is untouched, so the gates still compare
     * the oracle's numbers. */
    q.ndv = 0;
    if (q.nrv >= 3) {
        const int64_t G16 = 4096 * 16; /* 4096 px either side of centre */
        int need = 0;
        for (int i = 0; i < q.nrv; i++) {
            int64_t dx = (int64_t)q.rv[i].sx16 - (int64_t)cx * 16;
            int64_t dy = (int64_t)q.rv[i].sy16 - (int64_t)cy * 16;
            if (dx > G16 || dx < -G16 || dy > G16 || dy < -G16) {
                need = 1;
                break;
            }
        }
        if (need) {
            /* screen offset = X * K / Z with K = mant * 16 / 2^shift */
            double K = (double)mant * 16.0 / ldexp(1.0, shift);
            double P[16][6], Q[16][6];
            int np = 0;
            {
                const int UF = 16;
                /* rebuild the near-clipped polygon in float from rv's inputs:
                 * re-derive X, Y from the projected coords would lose them, so
                 * use z and the exact inverse of the projection. */
                for (int i = 0; i < q.nrv; i++) {
                    double Z = (double)q.rv[i].z;
                    double dx = ((double)q.rv[i].sx16 - (double)cx * 16.0);
                    double dy = -((double)q.rv[i].sy16 - (double)cy * 16.0);
                    P[np][0] = dx * Z / K;
                    P[np][1] = dy * Z / K;
                    P[np][2] = Z;
                    P[np][3] = q.rv[i].uf;
                    P[np][4] = q.rv[i].vf;
                    P[np][5] = q.rv[i].bf;
                    np++;
                }
                (void)UF;
            }
            /* saturated vertices lose their true X/Y above; recover them from
             * the pre-projection polygon instead when available */
            if (np == n_pre) {
                for (int i = 0; i < np; i++) {
                    P[i][0] = pre_x[i];
                    P[i][1] = pre_y[i];
                }
            }
            for (int pl = 0; pl < 4 && np >= 3; pl++) {
                int nq = 0;
                for (int i = 0; i < np; i++) {
                    double *a = P[i], *b = P[(i + 1) % np];
                    double da, db, ga = G16 * a[2], gb = G16 * b[2];
                    switch (pl) {
                    case 0:
                        da = a[0] * K - ga;
                        db = b[0] * K - gb;
                        break;
                    case 1:
                        da = -a[0] * K - ga;
                        db = -b[0] * K - gb;
                        break;
                    case 2:
                        da = a[1] * K - ga;
                        db = b[1] * K - gb;
                        break;
                    default:
                        da = -a[1] * K - ga;
                        db = -b[1] * K - gb;
                        break;
                    }
                    if (da <= 0 && nq < 16) {
                        memcpy(Q[nq++], a, sizeof Q[0]);
                    }
                    if ((da <= 0) != (db <= 0) && nq < 16) {
                        double t = da / (da - db);
                        for (int c = 0; c < 6; c++)
                            Q[nq][c] = a[c] + (b[c] - a[c]) * t;
                        nq++;
                    }
                }
                memcpy(P, Q, sizeof(double) * 6 * nq);
                np = nq;
            }
            if (np >= 3 && np <= 10) {
                for (int i = 0; i < np; i++) {
                    geo_vert *d = &q.dv[i];
                    double Z = P[i][2] < 1.0 ? 1.0 : P[i][2];
                    d->sx16 = (int32_t)llround((double)cx * 16.0 + P[i][0] * K / Z);
                    d->sy16 = (int32_t)llround((double)cy * 16.0 - P[i][1] * K / Z);
                    d->z = (int32_t)llround(Z);
                    d->uf = (int32_t)llround(P[i][3]);
                    d->vf = (int32_t)llround(P[i][4]);
                    d->bf = (int32_t)llround(P[i][5]);
                    d->u = (uint32_t)(d->uf >> 16) & 0xfff;
                    d->v = (uint32_t)(d->vf >> 16) & 0xfff;
                    d->bri = d->bf >> 16;
                    d->valid = 1;
                }
                q.ndv = np;
            } else if (np < 3) {
                q.ndv = -1; /* entirely outside the guard band */
            }
        }
    }
    if (q.behind)
        g_geo_stats.part_clip++;
    g_geo_stats.emitted++;
    if (g_geo_stats.zmin_seen == 0 || zmin < g_geo_stats.zmin_seen)
        g_geo_stats.zmin_seen = zmin;
    if (zmax > g_geo_stats.zmax_seen)
        g_geo_stats.zmax_seen = zmax;
#ifdef GLES3_VALIDATION
    { /* ENG_OBJLOG: slave_list.c sets g_objlog_on (defined here: every game links geo_hw.c, Cyber Sled no slave list)
       */
        if (g_objlog_on) {
            int x0 = 1 << 30, x1 = -(1 << 30), y0 = 1 << 30, y1 = -(1 << 30);
            for (int k = 0; k < q.nrv; k++) {
                const int x = q.rv[k].sx16 / 16, y = q.rv[k].sy16 / 16;
                if (x < x0)
                    x0 = x;
                if (x > x1)
                    x1 = x;
                if (y < y0)
                    y0 = y;
                if (y > y1)
                    y1 = y;
            }
            fprintf(stderr,
                    "[OBJ]   quad zsort %06X color %06X nrv %d  x %d..%d y %d..%d\n",
                    (unsigned)q.zsort,
                    (unsigned)q.color,
                    q.nrv,
                    x0,
                    x1,
                    y0,
                    y1);
        }
    }
#endif

    cb(&q, user);
}

/* ---- packet walk (GeoFixed.blit_quads_fixed) --------------------------- */
static void blit_quads(uint32_t addr, uint32_t length, geo_quad_cb cb, void *user)
{
    uint32_t finish = addr + length;
    while (addr < finish) {
        int32_t packetlength = pt_read(addr);
        addr += 1;
        int32_t packetformat = pt_read(addr + 0);
        if (packetlength == 0x17) {
            quad_fixed(pt_read(addr + 2), addr + 3, 0, pt_read(addr + 1), packetformat, cb, user);
        } else if (packetlength == 0x18) {
            quad_fixed(pt_read(addr + 2), addr + 4, pt_read(addr + 3), pt_read(addr + 1), packetformat, cb, user);
        } else if (packetlength == 0x10) {
            g_lit_n = 0;
            g_lit_idx = 0;
            register_normals(addr + 4);
        } else if (packetlength == 0x0d) {
            register_normals(addr + 1);
        } else {
            live_fail("unsupported point-ROM packet format", g_bbox_cur, packetlength);
        }
        addr += (uint32_t)packetlength;
        if ((packetformat & 0x800000) && addr != finish)
            return;
    }
}

/* ---- object walk (GeoFixed.blit_polyobject_fixed) ---------------------- */
void geo_hw_object(int32_t code, geo_quad_cb cb, void *user)
{
    /* Code 0x5 is a POINT-RAM object (System 22: the list starts at point RAM
     * 0xF00000, and a negative entry other than -1 is an address with its top
     * bits set). Only boards that supply point RAM have them. */
    const int pointram = (code == 0x5);
    if (pointram)
        live_fail("unsupported point-RAM object", code, -1);
    g_lit_n = 0;
    g_lit_idx = 0; /* lit_fx is per object */
    uint32_t list_addr = pointram ? 0xf00000u : (uint32_t)pt_read(code);
    for (int guard = 0; guard < 4096; guard++) {
        int32_t object_addr = pt_read(list_addr);
        list_addr += 1;
        if (object_addr < 0) { /* -1 terminator (non-pointram) */
            if (object_addr == -1 || !pointram)
                break;
            object_addr &= 0x00ffffff;
        }
        uint32_t chunklength = (uint32_t)pt_read(object_addr);
        object_addr += 1;
        if (chunklength > 0x100)
            live_fail("point-ROM chunk capacity", code, chunklength);
        blit_quads((uint32_t)object_addr, chunklength, cb, user);
    }
}

/*
 * slave_list.c -- the SLAVE DSP's job: walk the master DSP's display list in
 * polygon RAM and hand every primitive to the geometry stage (geo_hw.c).
 *
 * One walker for both boards (namcos22_v.cpp simulate_slavedsp). Record
 * layout, from tools/pc_geo_fixed.py (byte-exact vs MAME):
 *   len 0x15  bb000  viewport: zoom = dspfloat(src[6]),
 *                     view[row][col] = q15(src[0x0c + col*3 + row])
 *   len 0x10  233002  object shift / cz adjust (System 22: + object flags)
 *   len 0x0a  300000  view transform: view[row][col] = q15(src[1+col*3+row])
 *   len 0x0d  200002  primitive (code 0x5 or >= 0x45):
 *                     m[row][col] = q15(src[1+col*3+row])
 *                     t = sext24(src[0xa..0xc])
 *                     combined = (m . view) >> 15, t' = (t . view) >> 15
 * Moved here from src/framedump.c unchanged, apart from the board settings.
 */
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "eng.h"
#include "geo_hw.h"
#include "slave_list.h"

#ifdef GLES3_VALIDATION
extern int g_objlog_on; /* ENG_OBJLOG: geo_hw.c prints the quads while it is set (defined in geo_hw.c) */
#endif

static int32_t sext_n(uint32_t v, int bits)
{
    uint32_t m = 1u << (bits - 1);
    v &= (1u << bits) - 1u;
    return (int32_t)((v ^ m) - m);
}

static int32_t q15v(uint32_t v)
{
    return sext_n(v & 0xffff, 16);
}

/* namcos22_v.cpp:699 dspfloat, as a float (the clip window needs the real
 * value, not the mantissa/shift pair the projection uses). */
static float dspfloatf(uint32_t v)
{
    float mant = (float)sext_n(v & 0xffff, 16);
    int exp = (int)((v >> 16) & 0x3f);
    while (exp < 0x2e) {
        mant /= 2.0f;
        exp++;
    }
    return mant;
}

/* GeoFixed._reflectq: mirror the view by negating a COLUMN of the matrix. */
static void reflectq(int32_t m[3][3], int reflection)
{
    if (reflection & 0x10)
        for (int r = 0; r < 3; r++)
            m[r][0] = -m[r][0];
    if (reflection & 0x20)
        for (int r = 0; r < 3; r++)
            m[r][1] = -m[r][1];
}

int eng_walk_list(eng_word_fn pw, const eng_list_cfg *cfg, geo_quad_cb cb, void *user)
{
    /* ---- camera state, persistent across records ---- */
    int32_t viewq[3][3] = {{0x7FFF, 0, 0}, {0, 0x7FFF, 0}, {0, 0, 0x7FFF}};
    int have_view = 0; /* oracle: `if not hasattr(self,"viewq")` */
    int32_t zoom_mant = 0x7FFF;
    int zoom_shift = 15;
    int32_t vx = 0, vy = 0;
    int32_t absolute_priority = 0, objectshift = 0, cz_adjust = 0;
    int reflection = 0, cullflip = 0;
    int32_t amb_fx = 0, pow_fx = 0, light_fx[3] = {0, 0, 0};
    float cl = 0, cr = 0, cu = 0, cd = 0;
    int have_clip = 0;

    /* Fixed list head (simulate_slavedsp): Super 22 starts at 0x304, System 22
     * at 0x2FF. Deriving it from a pointer word and skipping an FFFE prologue
     * (what this once did) is not what the hardware does. */
    int src = cfg->head;
    int objectflags = 0;

    int guard = 0, prims = 0;
    while (guard++ < 4096) {
        uint32_t code = pw(src) & 0xFFFF;
        uint32_t len = pw(src + 1) & 0xFFFF;
        const int p = src + 2; /* payload base */

        if (len == 0x15) { /* bb000: viewport */
            absolute_priority = (int32_t)((pw(p + 3) >> 16) & 0xffff);
            vx = sext_n((pw(p + 5) >> 16) & 0xffff, 12);
            vy = sext_n(pw(p + 5) & 0xffff, 12);
            uint32_t z = pw(p + 6);
            zoom_mant = sext_n(z & 0xffff, 16);
            {
                int e = (int)((z >> 16) & 0x3f);
                int sh = 0x2e - e;
                zoom_shift = sh < 0 ? 0 : sh;
            }
            amb_fx = (int32_t)((pw(p + 1) >> 16) & 0xffff);
            pow_fx = (int32_t)(pw(p + 1) & 0xffff);
            for (int k = 0; k < 3; k++)
                light_fx[k] = q15v(pw(p + 2 + k));
            { /* scene clip window, and the reflection swaps that go with it */
                float zf = dspfloatf(z);
                cl = dspfloatf(pw(p + 8)) * zf - 0.5f; /* vl = src[8] */
                cr = dspfloatf(pw(p + 7)) * zf - 0.5f; /* vr = src[7] */
                cu = dspfloatf(pw(p + 9)) * zf - 0.5f;
                cd = dspfloatf(pw(p + 10)) * zf - 0.5f;
            }
            reflection = (int)((pw(p + 2) >> 16) & 0x30);
            cullflip = (reflection == 0x10 || reflection == 0x20);
            if (reflection & 0x10) {
                float t2 = cl;
                cl = cr;
                cr = t2;
            }
            if (reflection & 0x20) {
                float t2 = cu;
                cu = cd;
                cd = t2;
            }
            have_clip = 1;
            for (int col = 0; col < 3; col++)
                for (int row = 0; row < 3; row++)
                    viewq[row][col] = q15v(pw(p + 0x0c + col * 3 + row));
            reflectq(viewq, reflection);
            have_view = 1;
            cz_adjust = objectshift = 0; /* viewport resets the shift group */
            objectflags = 0;
        } else if (len == 0x10) { /* 233002: object shift */
            cz_adjust = (int32_t)(pw(p + 1) & 0xffffff);
            objectshift = (int32_t)(pw(p + 2) & 0xffffff);
            if (cfg->s22_objectflags)
                objectflags = (int)((pw(p + 3) >> 21) & 7);
        } else if (len == 0x0a) { /* 300000: view xform */
            for (int col = 0; col < 3; col++)
                for (int row = 0; row < 3; row++)
                    viewq[row][col] = q15v(pw(p + 1 + col * 3 + row));
            reflectq(viewq, reflection);
            have_view = 1;
        } else if (len == 0x0d) { /* 200002: primitive */
            if ((code == 0x5 || code >= 0x45) && have_view) {
                int32_t m[3][3], t[3];
                for (int col = 0; col < 3; col++)
                    for (int row = 0; row < 3; row++)
                        m[row][col] = q15v(pw(p + 1 + col * 3 + row));
                for (int k = 0; k < 3; k++)
                    t[k] = sext_n(pw(p + 0x0a + k), 24);

                geo_view gv;
                memset(&gv, 0, sizeof gv);
                for (int r = 0; r < 3; r++)
                    for (int c = 0; c < 3; c++)
                        gv.m[r][c] = (int32_t)(((int64_t)m[r][0] * viewq[0][c] + (int64_t)m[r][1] * viewq[1][c] +
                                                (int64_t)m[r][2] * viewq[2][c]) >>
                                               15);
                for (int c = 0; c < 3; c++)
                    gv.t[c] = (int32_t)(((int64_t)t[0] * viewq[0][c] + (int64_t)t[1] * viewq[1][c] +
                                         (int64_t)t[2] * viewq[2][c]) >>
                                        15);
                gv.zoom_mant = zoom_mant;
                gv.zoom_shift = zoom_shift;
                gv.vx = vx;
                gv.vy = vy;
                gv.objectshift = objectshift;
                gv.cz_adjust = cz_adjust;
                gv.cl = cl;
                gv.cr = cr;
                gv.cu = cu;
                gv.cd = cd;
                gv.have_clip = have_clip;
                gv.absolute_priority = absolute_priority;
                gv.cullflip = cullflip;
                memcpy(gv.viewq, viewq, sizeof viewq);
                gv.light[0] = light_fx[0];
                gv.light[1] = light_fx[1];
                gv.light[2] = light_fx[2];
                gv.ambient = amb_fx;
                gv.power = pow_fx;
                gv.objectflags = objectflags;
                geo_hw_set_view(&gv);
                g_bbox_cur = (int)code;
                object_instance = src;
#ifdef GLES3_VALIDATION
                {
                    static int olog = -2;
                    if (olog == -2) {
                        const char *e = getenv("ENG_OBJLOG");
                        olog = e ? (int)strtol(e, NULL, 0) : -1;
                    }
                    if (olog == (int)code ||
                        olog ==
                            0) { /* ENG_OBJLOG=<code>: that model's record and every quad it gives; 0 = every model */
                        fprintf(stderr,
                                "[OBJ] code %d t %d %d %d  ap %d objshift %06X cz %06X\n",
                                (int)code,
                                gv.t[0],
                                gv.t[1],
                                gv.t[2],
                                absolute_priority,
                                (unsigned)objectshift & 0xFFFFFF,
                                (unsigned)cz_adjust & 0xFFFFFF);
                        g_objlog_on = 1;
                    }
                }
#endif

                geo_hw_object((int32_t)code, cb, user);
#ifdef GLES3_VALIDATION
                g_objlog_on = 0;
#endif

                g_bbox_cur = -1;
                objectflags &= ~2; /* blit_polyobject: per object */
                prims++;
            }
        } else {
            if (len != 0 && len != 0xffff)
                live_fail("unsupported display-list format", code, len);
            break; /* cleared/uninitialised list terminator */
        }

        /* The next record must follow IMMEDIATELY; anything else ends the
         * list (oracle: `if nxt != index + length + 2: break`). */
        int nxt = (int)(pw(src + len + 3) & 0x7FFF);
        if (nxt != src + (int)len + 4)
            break;
        src = nxt;
    }
    if (cfg->out_zoom_mant) {
        *cfg->out_zoom_mant = zoom_mant;
        *cfg->out_zoom_shift = zoom_shift;
        *cfg->out_vx = vx;
        *cfg->out_vy = vy;
    }
    return prims;
}

extern int32_t *g_pointrom;
extern uint32_t g_pointrom_words;
/* Fixed little-endian protocol record. Metadata layout is documented separately. */
typedef struct {
    int32_t meta[36], local[24], oracle[28];
} ObjectQuad;
_Static_assert(sizeof(ObjectQuad) == 352, "capture ABI");
static ObjectQuad records[32768];
static unsigned count;
static const rr_presentation *frame_source;
static uint32_t pw(int i)
{
    return frame_source->poly[i & 0x7fff];
}
static void emit(const geo_quad *q, void *unused)
{
    (void)unused;
    if (count >= 32768)
        live_fail("geometry capture capacity", -1, -1);
    ObjectQuad *r = records + count;
    memset(r, 0, sizeof *r);
    int32_t m[] = {g_bbox_cur,
                   (int32_t)object_packet,
                   q->zsort,
                   (int32_t)q->color,
                   q->flags_raw,
                   q->texbank,
                   q->cmode,
                   q->objectflags,
                   q->cz_adjust,
                   q->cz_type,
                   q->cz_value,
                   (int32_t)count};
    memcpy(r->meta, m, sizeof m);
    memcpy(r->meta + 12, g_view.m, 9 * 4);
    memcpy(r->meta + 21, g_view.t, 3 * 4);
    r->meta[24] = g_view.zoom_mant;
    r->meta[25] = g_view.zoom_shift;
    r->meta[26] = 320 + g_view.vx;
    r->meta[27] = 240 + g_view.vy;
    memcpy(r->meta + 28, q->clip, 4 * 4);
    r->meta[32] = q->nrv;
    r->meta[33] = q->ndv;
    r->meta[34] = q->behind;
    r->meta[35] = object_instance;
    for (int i = 0; i < 4; i++) {
        for (int c = 0; c < 3; c++)
            r->local[i * 6 + c] = pt_read(object_packet + 8 + i * 3 + c);
        r->local[i * 6 + 3] = pt_read(object_packet + i * 2) & 4095;
        r->local[i * 6 + 4] = pt_read(object_packet + i * 2 + 1) & 4095;
        r->local[i * 6 + 5] = object_brightness[i];
        const geo_vert *v = q->v + i;
        int32_t a[] = {v->sx16, v->sy16, v->z, (int32_t)v->u, (int32_t)v->v, v->bri, v->valid};
        memcpy(r->oracle + i * 7, a, sizeof a);
    }
    count++;
}

#define RENDER_CHECK_VALID(x)                                                                                          \
    do {                                                                                                               \
        if (!(x))                                                                                                      \
            live_fail("unsupported command layout", -1, -1);                                                           \
    } while (0)
/* Live adapter for the proven OBJECT command contract; original simulation untouched.
 * Included after private OBJECT backend and original geometry/list implementation. */
#include "rr_mem.h"
#include "rr_dsp.h"
#include "rr_scene.h"

#include "rr_gles3.h"
typedef struct {
    int32_t code, packet, texel;
} LiveMesh;
typedef struct {
    int32_t recipe[7], material;
} LiveMaterial;
typedef struct {
    RenderQuad q;
    float transform[16];
    int32_t box[4];
    unsigned page, protected_, reason;
    int32_t source_key;
} LiveCommand;
static LiveMesh *live_meshes;
static LiveMaterial *live_materials;
static uint32_t live_nm, live_nt, live_frame;
static LiveCommand live_cmd[MAX_QUADS];
static RenderPacket *frame_output;
#define live_slot (*frame_output)
static bool live_prepared, live_loaded;

static double live_compile_ms;

static void live_fail(const char *why, int code, int packet)
{
    const rr_presentation *s = frame_source;
    gles3_unsupported(live_frame,
                      s ? s->state_main : 0,
                      s ? s->state_sub : 0,
                      code,
                      packet,
                      dynamic_cache_key_valid ? dynamic_cache_last_recipe : NULL,
                      why);
}
static int live_recipe_compare(const int32_t *a, const int32_t *b)
{
    for (int i = 0; i < 7; i++)
        if (a[i] != b[i])
            return a[i] < b[i] ? -1 : 1;
    return 0;
}
static uint8_t palette_shadow[128][768];
static uint64_t palette_generations[128];
static bool palette_initialized[128];
static void update_palette_versions(void)
{
    for (unsigned g = 0; g < 128; g++) {
        bool changed = !palette_initialized[g];
        for (unsigned c = 0; c < 3; c++)
            changed |= memcmp(palette_shadow[g] + c * 256, frame_source->pal + g * 256 + c * 32768, 256) != 0;
        if (changed) {
            for (unsigned c = 0; c < 3; c++)
                memcpy(palette_shadow[g] + c * 256, frame_source->pal + g * 256 + c * 32768, 256);
            if (++palette_generations[g] == 0)
                palette_generations[g] = 1;
            palette_initialized[g] = true;
        }
    }
}

static int state_uncached(const int32_t *recipe)
{
    unsigned lo = 0, hi = live_nt;
    while (lo < hi) {
        unsigned mid = lo + (hi - lo) / 2;
        if (live_recipe_compare(live_materials[mid].recipe, recipe) < 0)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo < live_nt && !live_recipe_compare(live_materials[lo].recipe, recipe) ? live_materials[lo].material : -1;
}

/* Whole-catalogue x128 sizing; compact hash cells refer to exact immutable pool keys.
 * The bounded key pool and generation/probe limits cap work independently of play length. */
#include "material_cache.h"
static int live_mesh(int code, int packet)
{
    unsigned lo = 0, hi = live_nm;
    while (lo < hi) {
        unsigned m = lo + (hi - lo) / 2;
        LiveMesh *v = live_meshes + m;
        if (v->code < code || (v->code == code && v->packet < packet))
            lo = m + 1;
        else
            hi = m;
    }
    return lo < live_nm && live_meshes[lo].code == code && live_meshes[lo].packet == packet ? live_meshes[lo].texel
                                                                                            : -1;
}
static int live_painter(const void *a, const void *b)
{
    const LiveCommand *x = a, *y = b;
    if (x->q.zsort != y->q.zsort)
        return x->q.zsort > y->q.zsort ? -1 : 1;
    return (y->q.order > x->q.order) - (y->q.order < x->q.order);
}
static int live_batch(const void *a, const void *b)
{
    const LiveCommand *x = a, *y = b;
    if (x->protected_ != y->protected_)
        return x->protected_ < y->protected_ ? -1 : 1;
    if (x->page != y->page)
        return x->page < y->page ? -1 : 1;
    for (int i = 0; i < 4; i++)
        if (x->box[i] != y->box[i])
            return x->box[i] < y->box[i] ? -1 : 1;
    return x->q.depth > y->q.depth ? -1 : x->q.depth < y->q.depth ? 1 : 0;
}
static int live_visible(const void *a, const void *b)
{
    unsigned x = *(const uint16_t *)a, y = *(const uint16_t *)b;
    unsigned mx = (live_cmd[x].q.flags >> 1) & 3, my = (live_cmd[y].q.flags >> 1) & 3;
    unsigned nx = mx & 1, ny = my & 1, px = mx >> 1, py = my >> 1;
    if (nx != ny)
        return nx < ny ? -1 : 1;
    if (px != py)
        return px < py ? -1 : 1;
    int c = live_batch(live_cmd + x, live_cmd + y);
    return c ? c : (x > y) - (x < y);
}
/* Same conservative float32 projection-error enclosure as compact_race.py. */
#ifdef GLES3_VALIDATION
static unsigned cull_reference(const ObjectQuad *r)
{
    double low[4][3], high[4][3], zoom = ldexp((double)r->meta[24], -r->meta[25]);
    for (int v = 0; v < 4; v++)
        for (int c = 0; c < 3; c++) {
            int64_t dot = 0;
            double absolute = 0;
            for (int k = 0; k < 3; k++) {
                int64_t term = (int64_t)r->local[v * 6 + k] * r->meta[12 + k * 3 + c];
                dot += term;
                absolute += fabs((double)term);
            }
            double tr = r->meta[21 + c], error = absolute / (32768. * 1048576.) + fabs(tr) / 1048576. + 2;
            double xyz = (double)(dot >> 15) + tr;
            low[v][c] = xyz - error;
            high[v][c] = xyz + error;
        }
    for (int v = 0; v < 4; v++)
        if (low[v][2] <= 1)
            return 0;
    bool outside[4] = {true, true, true, true};
    for (int v = 0; v < 4; v++) {
        double xl = 1e100, xh = -1e100, yl = 1e100, yh = -1e100;
        for (int a = 0; a < 2; a++)
            for (int b = 0; b < 2; b++) {
                double z = b ? high[v][2] : low[v][2], x = (a ? high[v][0] : low[v][0]) / z * zoom,
                       y = (a ? high[v][1] : low[v][1]) / z * zoom;
                xl = fmin(xl, x);
                xh = fmax(xh, x);
                yl = fmin(yl, y);
                yh = fmax(yh, y);
            }
        outside[0] &= r->meta[26] + xh < r->meta[28] - 4;
        outside[1] &= r->meta[26] + xl > r->meta[29] + 4;
        outside[2] &= r->meta[27] - yl < r->meta[30] - 4;
        outside[3] &= r->meta[27] - yh > r->meta[31] + 4;
    }
    for (unsigned i = 0; i < 4; i++)
        if (outside[i])
            return i + 1;
    return 0;
}
#endif

#include "culling.h"
static unsigned live_cull(const ObjectQuad *r)
{
#ifdef GLES3_VALIDATION
    unsigned a = cull_reference(r), b = cull_projected_bounds(r);
    if (a != b) {
        fprintf(stderr, "GLES3 cull mismatch %u %u\n", a, b);
        abort();
    }
    return b;
#else
    return cull_projected_bounds(r);
#endif
}
static uint8_t dynamic_cache_pen(int bank, int cm, int u, int v)
{
    unsigned p = gles3_texture_pen(u, v, bank);
    if (cm & 4)
        return (0xec + ((cm & 8) << 1) + ((p >> (2 * (~cm & 3))) & 3)) & 255;
    if (cm & 2)
        return (0xe0 + ((cm & 8) << 1) + ((p >> (4 * (~cm & 1))) & 15)) & 255;
    return p;
}
static unsigned dynamic_cache_group(unsigned material)
{
    return material < material_count ? material_groups[material]
                                     : dynamic_cache_drawing->material[material - material_count].group;
}
static unsigned dynamic_cache_material_page(unsigned m)
{
    return m < material_count ? material_data[m].page : dynamic_cache_page_index;
}
static int dynamic_cache_material(const int *r)
{
    memcpy(dynamic_cache_last_recipe, r, 28);
    dynamic_cache_key_valid = true;
    for (unsigned i = 0; i < dynamic_cache_slot->materials; i++)
        if (!memcmp(dynamic_cache_slot->material[i].recipe, r, 28)) {
            dynamic_cache_slot->hits++;
            return material_count + i;
        }
    if (r[3] < 0 || r[3] > 4095 || r[4] < 0 || r[4] > 4095 || r[5] < 1 || r[6] < 1 || r[5] > 1020 || r[6] > 1020)
        live_fail("dynamic footprint dimensions/capacity", -1, -1);
    int key[] = {r[0], r[2], (r[3] / 256) * 256, (r[4] / 256) * 256, 0, 0};
    key[4] = ((r[3] + r[5] + 255) / 256) * 256 - key[2];
    key[5] = ((r[4] + r[6] + 255) / 256) * 256 - key[3];
#ifdef GLES3_VALIDATION
    if (getenv("ENG_GLES3_MATERIAL_KEYS"))
        fprintf(stderr,
                "[MATERIAL_CACHE-KEY] tick=%u key=%d,%d,%d,%d,%d,%d\n",
                live_frame,
                key[0],
                key[1],
                key[2],
                key[3],
                key[4],
                key[5]);
#endif
    bool decode = false;
    int id = dynamic_cache_reserve(dynamic_cache_slot, key, &decode);
    if (id < 0)
        live_fail("dynamic footprint dimensions/capacity", -1, -1);
    DynamicCacheEntry *e = dynamic_cache_entries + id;
    if (decode) {
        for (unsigned y = 0; y < e->h; y++)
            for (unsigned x = 0; x < e->w; x++)
                dynamic_cache_page[(e->y + y) * DYNAMIC_CACHE_SIDE + e->x + x] =
                    dynamic_cache_pen(key[0], key[1], key[2] - 2 + x, key[3] - 2 + y);
    }
    unsigned u;
    for (u = 0; u < dynamic_cache_slot->uploads; u++)
        if (dynamic_cache_slot->upload[u].entry == (unsigned)id)
            break;
    if (u == dynamic_cache_slot->uploads) {
        if (u >= DYNAMIC_CACHE_MATERIALS || dynamic_cache_slot->bytes + e->w * e->h > DYNAMIC_CACHE_BYTES)
            live_fail("dynamic slot capacity", -1, -1);
        DynamicCacheUpload next = {.entry = id,
                                   .x = e->x,
                                   .y = e->y,
                                   .w = e->w,
                                   .h = e->h,
                                   .offset = dynamic_cache_slot->bytes,
                                   .generation = e->generation};
        memcpy(next.key, key, 24);
        preparation_stage(dynamic_cache_slot, u, &next, dynamic_cache_page);
        dynamic_cache_slot->bytes += e->w * e->h;
        dynamic_cache_slot->uploads++;
#ifdef GLES3_VALIDATION
        if (decode) {
            unsigned char hash[32];
            SHA256(dynamic_cache_slot->pens + dynamic_cache_slot->upload[u].offset, e->w * e->h, hash);
            fprintf(stderr,
                    "[DYNAMIC_CACHE-DECODE] tick=%u key=%d,%d,%d,%d,%d,%d bytes=%u sha256=",
                    live_frame,
                    key[0],
                    key[1],
                    key[2],
                    key[3],
                    key[4],
                    key[5],
                    e->w * e->h);
            for (unsigned j = 0; j < 32; j++)
                fprintf(stderr, "%02x", hash[j]);
            fprintf(stderr, "\n");
        }
#endif
    }
    unsigned m = dynamic_cache_slot->materials++;
    if (m >= DYNAMIC_CACHE_MATERIALS)
        live_fail("dynamic material capacity", -1, -1);
    DynamicCacheMaterial *v = dynamic_cache_slot->material + m,
                         value = {.uv = {1.f / DYNAMIC_CACHE_SIDE,
                                         1.f / DYNAMIC_CACHE_SIDE,
                                         (e->x + 2.5 - e->key[2]) / (double)DYNAMIC_CACHE_SIDE,
                                         (e->y + 2.5 - e->key[3]) / (double)DYNAMIC_CACHE_SIDE},
                                  .group = r[1],
                                  .upload = u};
    memcpy(value.recipe, r, 28);
    if (memcmp(v, &value, sizeof value)) {
        *v = value;
        dynamic_cache_slot->metadata_bytes += sizeof value;
    }
    return material_count + m;
}
static float material_uv[3][DYNAMIC_CACHE_MATERIALS][4];
static bool material_uv_valid[3][DYNAMIC_CACHE_MATERIALS];
static void dynamic_cache_upload(const rr_presentation *s, unsigned copy)
{
    unsigned uv_bytes = 0;
    dynamic_cache_drawing = s->dynamic_cache_dynamic;
    RENDER_CHECK(dynamic_cache_drawing);
    page_textures[dynamic_cache_page_index] = dynamic_cache_pages[copy];
    material_texture = dynamic_cache_uv[copy];
#ifdef GLES3_VALIDATION
    if (provenance_debug)
        provenance_mapping = dynamic_cache_mapping[copy];
#endif

    unsigned bytes = 0;
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, dynamic_cache_pages[copy]);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (unsigned j = 0; j < dynamic_cache_drawing->uploads; j++) {
        const DynamicCacheUpload *p = dynamic_cache_drawing->upload + j;
        if (dynamic_cache_resident[copy][p->entry] != p->generation) {
            glTexSubImage2D(GL_TEXTURE_2D,
                            0,
                            p->x,
                            p->y,
                            p->w,
                            p->h,
                            GL_RED_INTEGER,
                            GL_UNSIGNED_BYTE,
                            dynamic_cache_drawing->pens + p->offset);
            dynamic_cache_resident[copy][p->entry] = p->generation;
            bytes += p->w * p->h;
        }
    }
    for (unsigned j = 0; j < dynamic_cache_drawing->materials; j++) {
        const DynamicCacheMaterial *m = dynamic_cache_drawing->material + j;
        unsigned id = material_count + j;
        if (!material_uv_valid[copy][j] || memcmp(material_uv[copy][j], m->uv, 16)) {
            glBindTexture(GL_TEXTURE_2D, dynamic_cache_uv[copy]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, id % 1024, id / 1024, 1, 1, GL_RGBA, GL_FLOAT, m->uv);
            memcpy(material_uv[copy][j], m->uv, 16);
            material_uv_valid[copy][j] = true;
            uv_bytes += 16;
        }
#ifdef GLES3_VALIDATION
        if (provenance_debug) {
            const DynamicCacheUpload *p = dynamic_cache_drawing->upload + m->upload;
            int map[] = {m->recipe[0],
                         m->recipe[3],
                         m->recipe[4],
                         p->x + 2 + m->recipe[3] - p->key[2],
                         p->y + 2 + m->recipe[4] - p->key[3],
                         m->recipe[5],
                         m->recipe[6],
                         m->recipe[1]};
            unsigned at = id * 2;
            glBindTexture(GL_TEXTURE_2D, dynamic_cache_mapping[copy]);
            glTexSubImage2D(GL_TEXTURE_2D, 0, at % 1024, at / 1024, 2, 1, GL_RGBA_INTEGER, GL_INT, map);
        }
#endif
    }
    if (rr_gles3_full_telemetry())
        fprintf(stderr, "[MATERIAL-UV-UPLOAD] tick=%u bytes=%u\n", s->frame, uv_bytes);
    RENDER_CHECK(glGetError() == GL_NO_ERROR);
    glActiveTexture(GL_TEXTURE0);
    if (rr_gles3_full_telemetry()) {
        fprintf(stderr,
                "[DYNAMIC_CACHE-UPLOAD] tick=%u copy=%u bytes=%u materials=%u hits=%u misses=%u evictions=%u\n",
                s->frame,
                copy,
                bytes,
                dynamic_cache_drawing->materials,
                dynamic_cache_drawing->hits,
                dynamic_cache_drawing->misses,
                dynamic_cache_drawing->evictions);
    }
}

#ifdef GLES3_VALIDATION
static unsigned record_splits[4], record_protected_visible;
#endif
static void live_compile(void)
{
#ifdef GLES3_VALIDATION
    memset(record_splits, 0, sizeof record_splits);
    record_protected_visible = 0;
#endif
    double start = clock_seconds(CLOCK_MONOTONIC);
    unsigned direct = frame_source->direct_count, n = count + direct;
    if (n > MAX_QUADS || direct > 512)
        live_fail("command/dynamic capacity", -1, -1);
    update_palette_versions();
    float dynamic[4096][4];
    memset(dynamic, 0, sizeof dynamic);
    for (unsigned i = 0; i < count; i++) {
        const ObjectQuad *r = records + i;
        LiveCommand *c = live_cmd + direct + i;
        memset(c, 0, sizeof *c);
        if (r->meta[0] == 5 || r->meta[7])
            live_fail("unsupported point-RAM/solid format", r->meta[0], r->meta[1]);
        int mesh = live_mesh(r->meta[0], r->meta[1]);
        if (mesh < 0)
            live_fail("mesh absent from ROM catalogue", r->meta[0], r->meta[1]);
        int u0 = 4096, u1 = 0, v0 = 4096, v1 = 0;
        for (int j = 0; j < 4; j++) {
            int u = r->local[j * 6 + 3], v = r->local[j * 6 + 4];
            u0 = u < u0 ? u : u0;
            u1 = u > u1 ? u : u1;
            v0 = v < v0 ? v : v0;
            v1 = v > v1 ? v : v1;
            RENDER_CHECK_VALID(r->local[j * 6 + 5] >= 0 && r->local[j * 6 + 5] <= 65535);
            c->q.brightness[j] = r->local[j * 6 + 5];
        }
        int32_t recipe[] = {r->meta[5], (r->meta[3] >> 8) & 127, r->meta[6], u0, v0, u1 - u0 + 1, v1 - v0 + 1};
        memcpy(dynamic_cache_last_recipe, recipe, 28);
        dynamic_cache_key_valid = true;
        int material = live_material(recipe);
        if (material < 0)
            live_fail("material absent from ROM catalogue", r->meta[0], r->meta[1]);
        c->page = dynamic_cache_material_page(material);
        c->q.mesh = mesh;
        c->q.material = material;
        c->q.source_instance = r->meta[35];
        c->source_key = r->meta[35];
        c->q.order = direct + i;
        c->q.zsort = r->meta[2] & 0xffffff;
        for (int j = 0; j < 3; j++) {
            for (int k = 0; k < 3; k++)
                c->transform[j * 4 + k] = r->meta[12 + k * 3 + j];
            c->transform[j * 4 + 3] = r->meta[21 + j];
        }
        double zoom = ldexp((double)r->meta[24], -r->meta[25]);
        c->transform[12] = r->meta[26] / 320. - 1;
        c->transform[13] = 1 - r->meta[27] / 240.;
        c->transform[14] = zoom / 320;
        c->transform[15] = zoom / 240;
        int l = r->meta[28] > 0 ? r->meta[28] : 0, rr = r->meta[29] < 639 ? r->meta[29] : 639,
            t = r->meta[30] > 0 ? r->meta[30] : 0, b = r->meta[31] < 479 ? r->meta[31] : 479;
        c->box[0] = l;
        c->box[1] = 479 - b;
        c->box[2] = rr - l + 1;
        c->box[3] = b - t + 1;
        bool valid = true;
        for (int j = 0; j < 4; j++)
            valid &= r->oracle[j * 7 + 6] != 0;
        if (valid)
            for (int j = 0; j < 2; j++) {
                int a = j ? 2 : 1, b = j ? 3 : 2;
                int64_t ax = (int64_t)r->oracle[a * 7] - r->oracle[0],
                        ay = (int64_t)r->oracle[a * 7 + 1] - r->oracle[1],
                        bx = (int64_t)r->oracle[b * 7] - r->oracle[0],
                        by = (int64_t)r->oracle[b * 7 + 1] - r->oracle[1];
                c->protected_ |= ax * by - ay * bx == 0;
            }
        c->reason = live_cull(r);
        if (c->box[2] <= 0 || c->box[3] <= 0)
            c->reason = 5;
        c->q.flags = ((r->meta[6] & 7) == 1) | ((r->meta[34] != 0) << 1) | (c->protected_ << 2) | (c->reason << 8);
    }
    for (unsigned i = 0; i < direct; i++) {
        const uint16_t *w = frame_source->direct[i];
        LiveCommand *c = live_cmd + i;
        memset(c, 0, sizeof *c);
        int u0 = 4096, u1 = 0, v0 = 4096, v1 = 0;
        for (unsigned j = 0; j < 4; j++) {
            const uint16_t *s = w + 4 + j * 6;
            int u = s[0] & 4095, v = s[1] & 4095;
            u0 = u < u0 ? u : u0;
            u1 = u > u1 ? u : u1;
            v0 = v < v0 ? v : v0;
            v1 = v > v1 ? v : v1;
            double ooz = s[5] ? ldexp((double)s[5], -(46 - (s[4] & 63) > 0 ? 46 - (s[4] & 63) : 0)) : 65536.;
            int64_t z = (int64_t)(1. / ooz);
            if (z < 1)
                z = 1;
            if (z > 2000000000)
                z = 2000000000;
            dynamic[i * 8 + j * 2][0] = (float)((int64_t)(int16_t)s[2] * z);
            dynamic[i * 8 + j * 2][1] = (float)(-(int64_t)(int16_t)s[3] * z);
            dynamic[i * 8 + j * 2][2] = z;
            dynamic[i * 8 + j * 2 + 1][0] = u;
            dynamic[i * 8 + j * 2 + 1][1] = v;
            c->q.brightness[j] = s[4] >> 8;
        }
        int32_t recipe[] = {w[5] >> 12, (w[2] >> 8) & 127, w[4] >> 12, u0, v0, u1 - u0 + 1, v1 - v0 + 1};
        memcpy(dynamic_cache_last_recipe, recipe, 28);
        dynamic_cache_key_valid = true;
        int material = live_material(recipe);
        if (material < 0)
            material = dynamic_cache_material(recipe);
        c->q.material = material;
        c->page = dynamic_cache_material_page(material);
        c->q.mesh = -1 - i * 8;
        c->q.source_instance = 65535;
        c->source_key = -1 - (int)i;
        c->q.order = i;
        c->q.zsort = ((w[1] & 4095) << 12) | (w[0] & 4095);
        c->q.flags = ((w[4] >> 12) & 7) == 1;
        c->transform[0] = c->transform[5] = c->transform[10] = 32768;
        c->transform[14] = 1.f / 320;
        c->transform[15] = 1.f / 240;
        c->box[2] = 640;
        c->box[3] = 480;
    }

#ifdef GLES3_VALIDATION
    extern void rr_validation_semantic(unsigned, const void *, size_t);
    static LiveCommand semantic_commands[MAX_QUADS];
    memcpy(semantic_commands, live_cmd, n * sizeof *live_cmd);
    for (unsigned k = 0; k < n; k++)
        semantic_commands[k].page = 0;
    if (live_frame >=
            (getenv("ENG_GLES3_RECEIPTS_FROM") ? strtoul(getenv("ENG_GLES3_RECEIPTS_FROM"), NULL, 10) : 1800) &&
        getenv("ENG_GLES3_SEMANTIC"))
        rr_validation_semantic(live_frame, semantic_commands, n * sizeof *live_cmd);

#endif
    qsort(live_cmd, n, sizeof *live_cmd, live_painter);
    for (unsigned i = 0; i < n; i++)
        live_cmd[i].q.depth = .9 - 1.8 * (i + 1) / (n + 1.);
    qsort(live_cmd, n, sizeof *live_cmd, live_batch);
    RenderInstance ins[MAX_INSTANCES];
    int32_t ins_source[MAX_INSTANCES];
    unsigned ni = 0;
    RenderQuad quads[MAX_QUADS];
    uint16_t visible[MAX_QUADS];
    unsigned nv = 0;
    DrawGroup groups[MAX_QUADS];
    unsigned ng = 0;
    for (unsigned i = 0; i < n; i++) {
        LiveCommand *c = live_cmd + i;
        unsigned k = 0;
        for (; k < ni; k++) {
            if (ins_source[k] == c->source_key && !memcmp(ins[k].data, c->transform, 64) &&
                !memcmp(ins[k].box, c->box, 16))
                break;
        }
        if (k == ni) {
            if (ni >= MAX_INSTANCES)
                live_fail("instance capacity", -1, -1);
            ins_source[ni] = c->source_key;
            memcpy(ins[ni].data, c->transform, 64);
            memcpy(ins[ni].box, c->box, 16);
            ni++;
        }
        c->q.instance = k;
        quads[i] = c->q;
        if (!c->reason)
            visible[nv++] = i;
    }
    qsort(visible, nv, 2, live_visible);
    for (unsigned i = 0; i < nv; i++) {
        LiveCommand *c = live_cmd + visible[i];
        unsigned mode = (c->q.flags >> 1) & 3;
        DrawGroup *g = ng ? groups + ng - 1
                          : NULL; /* Adjacent instances share program/layout through mode. Keep protection in
                                   * sorting, depth and mode (including the no-reversal rule); only join draws. */
        if (g && g->page == c->page && g->mode == mode && !memcmp(g->box, c->box, 16))
            g->count++;
        else {
#ifdef GLES3_VALIDATION
            if (g) { /* Protection is no longer a draw boundary. */
                record_splits[1] += g->page != c->page;
                record_splits[2] += memcmp(g->box, c->box, 16) != 0;
                record_splits[3] += g->mode != mode;
            }
#endif
            g = groups + ng++;
            g->first = i;
            g->count = 1;
            g->page = c->page;
            g->mode = mode;
            memcpy(g->box, c->box, 16);
        }
#ifdef GLES3_VALIDATION
        record_protected_visible += !!(mode & 2);
#endif
    }
#ifdef GLES3_VALIDATION
    if (provenance_debug &&
        (live_frame == 1800 || live_frame == 1965 || live_frame == 2500 || live_frame == 2643 || live_frame == 3000 ||
         live_frame == 3546 || live_frame == 3600 || live_frame == 4200 || live_frame == 4800 || live_frame == 5400 ||
         live_frame == 5639 || live_frame == 5655 || live_frame == 5867 || live_frame == 5874 || live_frame == 5878 ||
         live_frame == 5889 || live_frame == 5921 || live_frame == 6006)) {
        for (unsigned j = 0; j < dynamic_cache_slot->materials; j++) {
            int *r = dynamic_cache_slot->material[j].recipe;
            fprintf(stderr,
                    "[GLES3-MATERIAL] tick=%u id=%u recipe=%d,%d,%d,%d,%d,%d,%d\n",
                    live_frame,
                    material_count + j,
                    r[0],
                    r[1],
                    r[2],
                    r[3],
                    r[4],
                    r[5],
                    r[6]);
        }
    }
#endif
    RenderPacketHeader h = {.magic = 0x33504e52,
                            .version = 3,
                            .tick = live_frame,
                            .n = n,
                            .visible = nv,
                            .groups = ng,
                            .instances = ni,
                            .dynamic = direct * 8};
    unsigned bgbase = (frame_source->mixer[4] << 8) & 0x7f00;
    for (unsigned c = 0; c < 3; c++)
        h.bg[c] = frame_source->pal[bgbase + 255 + c * 32768] / 255.f;
    size_t at = 0;
#define LIVE_COPY(p, n)                                                                                                \
    do {                                                                                                               \
        size_t bytes_ = (n);                                                                                           \
        RENDER_CHECK_VALID(at + bytes_ <= PACKET_CAPACITY);                                                            \
        memcpy(live_slot.bytes + at, (p), bytes_);                                                                     \
        at += bytes_;                                                                                                  \
    } while (0)
    LIVE_COPY(&h, sizeof h);
    LIVE_COPY(ins, ni * sizeof *ins);
    LIVE_COPY(quads, n * sizeof *quads);
    LIVE_COPY(visible, nv * 2);
    if (at & 3) {
        memset(live_slot.bytes + at, 0, 4 - (at & 3));
        at = (at + 3) & ~3u;
    }
    LIVE_COPY(groups, ng * sizeof *groups);
    LIVE_COPY(dynamic, direct * 8 * 16);
    LIVE_COPY(frame_source->text, 8192);
#undef LIVE_COPY
    live_slot.size = at;
    live_slot.tick = live_frame;
    live_prepared = true;
    live_compile_ms = (clock_seconds(CLOCK_MONOTONIC) - start) * 1000;
#ifdef GLES3_VALIDATION
    const char *dir = getenv("ENG_GLES3_COMMAND_DUMP");
    if (dir) {
        char path[4096];
        snprintf(path, sizeof path, "%s/tick-%u.live", dir, live_frame);
        FILE *f = fopen(path, "wbx");
        RENDER_CHECK_VALID(f && fwrite(live_slot.bytes, 1, at, f) == at && !fclose(f));
    }
#endif
}

/* Convert only page-group runs in the offscreen warm packet. */
bool rr_renderer_open(void)
{
    const char *pack = getenv("ENG_GLES3_ASSETS");
    RENDER_CHECK(pack);
    FILE *f = open_asset(pack, "gles3.lookup");
    uint32_t h[4];
    read_exact(f, h, 16);
    RENDER_CHECK(h[0] == 0x4b4c314e && h[1] == 1 && h[2] <= 131072 && h[3] <= MAX_MATERIALS);
    live_nm = h[2];
    live_nt = h[3];
    material_cache_init();
    live_meshes = malloc(live_nm * sizeof *live_meshes);
    live_materials = malloc(live_nt * sizeof *live_materials);
    RENDER_CHECK(live_meshes && live_materials);
    read_exact(f, live_meshes, live_nm * sizeof *live_meshes);
    read_exact(f, live_materials, live_nt * sizeof *live_materials);
    RENDER_CHECK(fgetc(f) == EOF && !fclose(f));
#ifdef GLES3_VALIDATION

#endif
    frame_diagnostic = false;
    gles3_front = 1;
    gles3_ram = 0;
    stage_sync = false;
    load_render_assets(pack, NULL);
    glGenTextures(1, &readback_color);
    glBindTexture(GL_TEXTURE_2D, readback_color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 640, 480, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glGenFramebuffers(1, &readback_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, readback_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, readback_color, 0);
    RENDER_CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    live_loaded = true;
    gles3_pre_warm();
    fprintf(stderr, "[GLES3] gles3_from_first_picture=1 meshes=%u materials=%u\n", live_nm, live_nt);
    return true;
}

/* All compiler scratch is owned by rr-prepare. Source data is a const slot;
 * GL state and GPU resource copies are exclusively owned by rr-present. */

extern void rr_video_readback_output(unsigned);
#ifdef GLES3_VALIDATION
extern void rr_validation_packet(unsigned, const void *, size_t);
#endif

static unsigned frame_gles3_count;
static double frame_ring_wait;
static void state_receipt(rr_presentation *s, const char *why)
{
    if (rr_gles3_full_telemetry()) {
        if (s->dynamic_cache_dynamic && ((DynamicCacheSlot *)s->dynamic_cache_dynamic)->tick == s->frame) {
            const DynamicCacheSlot *d = s->dynamic_cache_dynamic;
            fprintf(stderr,
                    "[UPLOAD-STAGING] tick=%u copied_bytes=%llu metadata_bytes=%llu\n",
                    s->frame,
                    (unsigned long long)d->copied_bytes,
                    (unsigned long long)d->metadata_bytes);
        }
        fprintf(stderr,
                "[MATERIAL_CACHE-CACHE] tick=%u occupancy=%u lookups=%llu probes=%llu max_probe=%u full=%llu clears=%u "
                "generation=%u "
                "capacity=%u pool_limit=65535\n",
                s->frame,
                material_cache_occupancy,
                (unsigned long long)material_cache_lookups,
                (unsigned long long)material_cache_probes,
                material_cache_maxprobe,
                (unsigned long long)material_cache_full,
                material_cache_clears,
                material_cache_generation,
                material_cache_capacity);
    }
    material_cache_clears = 0;
    material_cache_lookups = material_cache_probes = material_cache_full = material_cache_maxprobe = 0;
    if (rr_gles3_full_telemetry()) {
        const DynamicCacheSlot *d = s->dynamic_cache_dynamic;
        if (d && d->tick == s->frame)
            fprintf(stderr,
                    "[DYNAMIC-CACHE-PREP] tick=%u hits=%u misses=%u evictions=%u pending_bytes=%u\n",
                    s->frame,
                    d->hits,
                    d->misses,
                    d->evictions,
                    d->bytes);
        fprintf(stderr,
                "[RENDER-COVERAGE] tick=%u main=%u sub=%u screen=%u course=%u major=%u walk=%u direct=%u gles3=%u "
                "reason=%s\n",
                s->frame,
                s->state_main,
                s->state_sub,
                s->state_screen,
                s->state_course,
                s->dynamic_cache_major,
                s->walk,
                s->direct_count,
                s->gles3_tag,
                why);
    }
}
bool rr_renderer_prepare(rr_presentation *s)
{
    s->gles3_walk_ms = s->gles3_compile_ms = 0;
    frame_source = s;
    live_frame = s->frame;
    rr_gl_renderer_select(s);
    dynamic_cache_key_valid = false;
    if (!live_loaded)
        live_fail("gles3 resources unavailable", -1, -1);
    if (!s->gles3_payload) {
        s->gles3_payload = calloc(1, sizeof(RenderPacket));
        if (!s->gles3_payload)
            live_fail("command slot allocation", -1, -1);
    }
    if (!s->dynamic_cache_dynamic) {
        s->dynamic_cache_dynamic = calloc(1, sizeof(DynamicCacheSlot));
        if (!s->dynamic_cache_dynamic)
            live_fail("dynamic slot allocation", -1, -1);
    }
    dynamic_cache_slot = s->dynamic_cache_dynamic;
    preparation_stage_begin(dynamic_cache_slot);
    dynamic_cache_slot->tick = s->frame;
    frame_output = s->gles3_payload;
#ifdef GLES3_VALIDATION
    const char *force = getenv("ENG_GLES3_FORCE_UNSUPPORTED");
    if (force && s->frame == (unsigned)atoi(force)) {
        s->walk = true;
        s->poly[0x2ff] = 0x45;
        s->poly[0x300] = 0x1234;
    }
#endif
    count = 0;
    double t = clock_seconds(CLOCK_MONOTONIC);
    use_exact_projection = true;
    if (s->walk) {
        eng_list_cfg cfg = {ENG_LIST_HEAD_S22, 1, NULL, NULL, NULL, NULL};
        eng_walk_list(pw, &cfg, emit, NULL);
    }
    use_exact_projection = false;
    s->gles3_walk_ms = (clock_seconds(CLOCK_MONOTONIC) - t) * 1000;
    live_compile();
#ifdef GLES3_VALIDATION
    memcpy(s->record_splits, record_splits, sizeof record_splits);
    s->record_protected_visible = record_protected_visible;
#endif
    memcpy(s->palette_revision, palette_generations, sizeof s->palette_revision);
    s->gles3_compile_ms = live_compile_ms;
    s->gles3_tag = 1;
#ifdef GLES3_VALIDATION
    if (s->frame >= (getenv("ENG_GLES3_RECEIPTS_FROM") ? strtoul(getenv("ENG_GLES3_RECEIPTS_FROM"), NULL, 10) : 1800) &&
        getenv("ENG_GLES3_RECEIPTS"))
        rr_validation_packet(
            s->frame, ((RenderPacket *)s->gles3_payload)->bytes, ((RenderPacket *)s->gles3_payload)->size);
#endif
    state_receipt(s, "covered");
    return true;
}
bool rr_renderer_draw(const rr_presentation *s, eng_render_stats *stats)
{
    unsigned i = frame_gpu_index++ % 3;
    double start = clock_seconds(CLOCK_MONOTONIC);
    if (frame_fences[i]) {
        GLenum r;
        do {
            r = glClientWaitSync(frame_fences[i], GL_SYNC_FLUSH_COMMANDS_BIT, 100000000);
        } while (r == GL_TIMEOUT_EXPIRED);
        RENDER_CHECK(r != GL_WAIT_FAILED);
        glDeleteSync(frame_fences[i]);
        frame_fences[i] = 0;
    }
    frame_ring_wait += clock_seconds(CLOCK_MONOTONIC) - start;
    instance_texture = frame_instances[i];
    dynamic_mesh_texture = frame_dynamic[i];
    attribute_buffer = frame_attributes[i];
    frame_hud = &s->video;
    gles3_drawing = s;
    double capture_up = clock_seconds(CLOCK_MONOTONIC), capture_ucpu = clock_seconds(CLOCK_THREAD_CPUTIME_ID);
    capture_groups = 0;
    {
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, palette_textures[i]);
        uint8_t row[256 * 4];
        for (unsigned g = 0; g < 128; g++)
            if (!palette_valid[i][g] || uploaded_palette_revision[i][g] != s->palette_revision[g]) {
                capture_groups++;
                for (unsigned p = 0; p < 256; p++) {
                    for (unsigned c = 0; c < 3; c++)
                        row[p * 4 + c] = s->pal[g * 256 + p + c * 32768];
                    row[p * 4 + 3] = 255;
                }
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, g, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, row);
                palette_valid[i][g] = true;
                uploaded_palette_revision[i][g] = s->palette_revision[g];
            }
        glActiveTexture(GL_TEXTURE0);
    }
    capture_upload_wall = (clock_seconds(CLOCK_MONOTONIC) - capture_up) * 1000;
    capture_upload_cpu = (clock_seconds(CLOCK_THREAD_CPUTIME_ID) - capture_ucpu) * 1000;
    dynamic_cache_upload(s, i);
    const char *capture_output = "-";
#ifdef GLES3_VALIDATION
    capture_output = getenv("ENG_GLES3_CAPTURE") ? getenv("ENG_GLES3_CAPTURE") : "-";
#endif
    draw_render_packet(s->gles3_payload, 0, capture_output, clock_seconds(CLOCK_MONOTONIC), 0);
    frame_fences[i] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    RENDER_CHECK(frame_fences[i]);
    const RenderPacketHeader *h = (const RenderPacketHeader *)((const RenderPacket *)s->gles3_payload)->bytes;
    memset(stats, 0, sizeof *stats);
    stats->draws = h->groups;
    stats->triangles = h->visible * 2;
    frame_gles3_count++;
    return true;
}
void rr_renderer_close(void)
{
    if (!live_loaded)
        return;
    for (int i = 0; i < 3; i++)
        if (frame_fences[i]) {
            glDeleteSync(frame_fences[i]);
            frame_fences[i] = 0;
        }
    fprintf(stderr, "[GLES3] pictures=%u first_gles=1 ring_wait_ms=%.6f\n", frame_gles3_count, frame_ring_wait * 1000);
    gles3_flush();
    live_loaded = false;
}

/* Synthetic ROM-independent draw exercises both projection shaders and mesh
 * sources in all three resource copies. Nothing is swapped during warm-up. */
void gles3_pre_warm(void)
{
    rr_presentation *s = calloc(1, sizeof *s);
    RenderPacket *slot = calloc(1, sizeof *slot);
    DynamicCacheSlot *d = calloc(1, sizeof *d);
    RENDER_CHECK(s && slot && d);
    s->dynamic_cache_dynamic = d;
    s->gles3_payload = slot;
    rr_gl_renderer_select(s);
    RENDER_CHECK(rr_gl_video_prepare(&s->video));
    frame_hud = &s->video;
    gles3_drawing = s;
#ifdef GLES3_VALIDATION
    if (provenance_debug &&
        (live_frame == 1800 || live_frame == 1965 || live_frame == 2500 || live_frame == 2643 || live_frame == 3000 ||
         live_frame == 3546 || live_frame == 3600 || live_frame == 4200 || live_frame == 4800 || live_frame == 5400 ||
         live_frame == 5639 || live_frame == 5655 || live_frame == 5867 || live_frame == 5874 || live_frame == 5878 ||
         live_frame == 5889 || live_frame == 5921 || live_frame == 6006)) {
        for (unsigned j = 0; j < dynamic_cache_slot->materials; j++) {
            int *r = dynamic_cache_slot->material[j].recipe;
            fprintf(stderr,
                    "[GLES3-MATERIAL] tick=%u id=%u recipe=%d,%d,%d,%d,%d,%d,%d\n",
                    live_frame,
                    material_count + j,
                    r[0],
                    r[1],
                    r[2],
                    r[3],
                    r[4],
                    r[5],
                    r[6]);
        }
    }
#endif
    RenderPacketHeader h = {
        .magic = 0x33504e52, .version = 3, .n = 3, .visible = 3, .groups = 3, .instances = 1, .dynamic = 8};
    RenderInstance ins = {.data = {32768, 0, 0, 0, 0, 32768, 0, 0, 0, 0, 32768, 4096, 0, 0, 1, 1}};
    RenderQuad q[3] = {0};
    uint16_t indices[4] = {0, 1, 2, 0};
    DrawGroup groups[3] = {0};
    float dynamic[8][4] = {
        {-16, -16, 4096}, {0, 0}, {16, -16, 4096}, {1, 0}, {16, 16, 4096}, {1, 1}, {-16, 16, 4096}, {0, 1}};
    uint8_t text[8192] = {0};
    for (unsigned j = 0; j < 3; j++) {
        q[j].mesh = j == 2 ? -1 : 0;
        q[j].depth = .6f - j * .1f;
        q[j].flags = j == 1 ? 2 : 0;
        for (unsigned k = 0; k < 4; k++)
            q[j].brightness[k] = 64;
        groups[j] =
            (DrawGroup){.first = j, .count = 1, .page = material_data[0].page, .mode = j == 1, .box = {0, 0, 640, 480}};
    }
    size_t at = 0;
#define WARM_COPY(p, n)                                                                                                \
    do {                                                                                                               \
        memcpy(slot->bytes + at, p, n);                                                                                \
        at += n;                                                                                                       \
    } while (0)
    WARM_COPY(&h, sizeof h);
    WARM_COPY(&ins, sizeof ins);
    WARM_COPY(q, sizeof q);
    WARM_COPY(indices, sizeof indices);
    WARM_COPY(groups, sizeof groups);
    WARM_COPY(dynamic, sizeof dynamic);
    WARM_COPY(text, sizeof text);
#undef WARM_COPY
    slot->size = at;
    slot->tick = 0;
    rr_video_readback_output(readback_fbo);
    for (unsigned i = 0; i < 3; i++) {
        instance_texture = frame_instances[i];
        dynamic_mesh_texture = frame_dynamic[i];
        attribute_buffer = frame_attributes[i];
        dynamic_cache_upload(s, i);
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, palette_textures[i]);
        draw_render_packet(slot, -1, "-", clock_seconds(CLOCK_MONOTONIC), 0);
    }
    glFinish();
    rr_video_readback_output(0);
    rr_gl_presentation_release();
    free(d);
    free(slot);
    free(s);
    fprintf(stderr,
            "[GLES3-PREWARM] copies=3 shaders=2 static_mesh=1 dynamic_mesh=1 offscreen=1 before_first_picture=1\n");
}
