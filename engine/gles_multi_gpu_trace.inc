/* Optional nonblocking elapsed GPU span for the ordered four-sampler experiment. */
#ifdef RR_GLES2_MULTITEXTURE_BATCH
#include <SDL.h>
#include <GLES2/gl2ext.h>
static FILE *multi_gpu_file;
static const char *multi_gpu_path;
static PFNGLGENQUERIESEXTPROC multi_gen_query;
static PFNGLDELETEQUERIESEXTPROC multi_delete_query;
static PFNGLBEGINQUERYEXTPROC multi_begin_query;
static PFNGLENDQUERYEXTPROC multi_end_query;
static PFNGLGETQUERYOBJECTUIVEXTPROC multi_query_available;
static PFNGLGETQUERYOBJECTUI64VEXTPROC multi_query_result;
static struct { GLuint id; unsigned frame; bool pending, disjoint; } multi_gpu_slots[32];
static unsigned multi_gpu_next, multi_gpu_dropped;
static int multi_gpu_active = -1;
static unsigned multi_gpu_from = 1800, multi_gpu_to = 5639;
static void multi_gpu_poll(unsigned frame)
{
    if (!multi_gpu_file) return;
    GLint disjoint = 0;
    glGetIntegerv(GL_GPU_DISJOINT_EXT, &disjoint);
    for (unsigned i = 0; i < 32; i++) if (multi_gpu_slots[i].pending) {
        if (disjoint) multi_gpu_slots[i].disjoint = true;
        GLuint ready = 0;
        multi_query_available(multi_gpu_slots[i].id, GL_QUERY_RESULT_AVAILABLE_EXT, &ready);
        if (!ready) continue;
        GLuint64EXT ns = 0;
        multi_query_result(multi_gpu_slots[i].id, GL_QUERY_RESULT_EXT, &ns);
        if (multi_gpu_slots[i].frame >= multi_gpu_from && multi_gpu_slots[i].frame <= multi_gpu_to)
            fprintf(multi_gpu_file, "%u,%llu,%d,%u\n", multi_gpu_slots[i].frame,
                (unsigned long long)ns, multi_gpu_slots[i].disjoint, frame - multi_gpu_slots[i].frame);
        multi_gpu_slots[i].pending = false;
    }
}
static void multi_gpu_begin(unsigned frame)
{
    if (!multi_gpu_file) return;
    multi_gpu_poll(frame);
    if (frame < multi_gpu_from || frame > multi_gpu_to) return;
    unsigned slot = multi_gpu_next++ % 32;
    if (multi_gpu_slots[slot].pending) { multi_gpu_dropped++; return; }
    multi_gpu_slots[slot].frame = frame;
    multi_gpu_slots[slot].disjoint = false;
    multi_gpu_active = (int)slot;
    multi_begin_query(GL_TIME_ELAPSED_EXT, multi_gpu_slots[slot].id);
}
static void multi_gpu_end(void)
{
    if (multi_gpu_active < 0) return;
    multi_end_query(GL_TIME_ELAPSED_EXT);
    multi_gpu_slots[multi_gpu_active].pending = true;
    multi_gpu_active = -1;
}
static bool multi_gpu_open(void)
{
    const char *path = multi_gpu_path;
    if (!path || !*path) return true;
    const char *ext = (const char *)glGetString(GL_EXTENSIONS);
    if (!ext || !strstr(ext, "GL_EXT_disjoint_timer_query")) {
        fprintf(stderr, "[MULTI-GPU] elapsed timer extension unavailable\n"); return true;
    }
#define GET(var, type, name) var = (type)SDL_GL_GetProcAddress(name)
    GET(multi_gen_query, PFNGLGENQUERIESEXTPROC, "glGenQueriesEXT");
    GET(multi_delete_query, PFNGLDELETEQUERIESEXTPROC, "glDeleteQueriesEXT");
    GET(multi_begin_query, PFNGLBEGINQUERYEXTPROC, "glBeginQueryEXT");
    GET(multi_end_query, PFNGLENDQUERYEXTPROC, "glEndQueryEXT");
    GET(multi_query_available, PFNGLGETQUERYOBJECTUIVEXTPROC, "glGetQueryObjectuivEXT");
    GET(multi_query_result, PFNGLGETQUERYOBJECTUI64VEXTPROC, "glGetQueryObjectui64vEXT");
#undef GET
    if (!multi_gen_query || !multi_delete_query || !multi_begin_query || !multi_end_query ||
        !multi_query_available || !multi_query_result) {
        fprintf(stderr, "[MULTI-GPU] timer entry points unavailable\n"); return true;
    }
    multi_gpu_file = fopen(path, "wx");
    if (!multi_gpu_file) { perror("RR_MULTI_GPU_TRACE"); return false; }
    fputs("frame,elapsed_ns,disjoint,result_age_ticks\n", multi_gpu_file);
    for (unsigned i = 0; i < 32; i++) multi_gen_query(1, &multi_gpu_slots[i].id);
    fprintf(stderr, "[MULTI-GPU] nonblocking elapsed queries enabled; span can include GPU idle\n");
    return glGetError() == GL_NO_ERROR;
}
static void multi_gpu_close(void)
{
    if (!multi_gpu_file) return;
    multi_gpu_poll(multi_gpu_to + 1);
    unsigned pending = 0;
    for (unsigned i = 0; i < 32; i++) {
        pending += multi_gpu_slots[i].pending;
        multi_delete_query(1, &multi_gpu_slots[i].id);
    }
    fprintf(stderr, "[MULTI-GPU] pending=%u ring_full=%u\n", pending, multi_gpu_dropped);
    fclose(multi_gpu_file); multi_gpu_file = NULL;
}
#endif
