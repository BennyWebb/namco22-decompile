/* Optional EXT elapsed queries. Results are read only after AVAILABLE: no waits.
 * Elapsed GPU command span can include GPU idle between CPU submissions. */
#include <GLES2/gl2ext.h>
static bool audit_gpu;
static FILE *gpu_file;
static PFNGLGENQUERIESEXTPROC gpu_gen;
static PFNGLDELETEQUERIESEXTPROC gpu_delete;
static PFNGLBEGINQUERYEXTPROC gpu_begin_query;
static PFNGLENDQUERYEXTPROC gpu_end_query;
static PFNGLGETQUERYOBJECTUIVEXTPROC gpu_available;
static PFNGLGETQUERYOBJECTUI64VEXTPROC gpu_result;
static struct { GLuint query; uint32_t frame; bool pending,disjoint; } gpu_slots[16];
static int gpu_next,gpu_active=-1;
static unsigned gpu_valid,gpu_invalid,gpu_dropped;
static bool gpu_open(void)
{
    if(!audit_gpu) return true;
    const char *ext=(const char *)glGetString(GL_EXTENSIONS);
    if(!ext || !strstr(ext,"GL_EXT_disjoint_timer_query")) {
        fprintf(stderr,"[GPU-AUDIT] elapsed queries unavailable\n");audit_gpu=false;return true;
    }
#define P(var,type,name) var=(type)SDL_GL_GetProcAddress(name)
    P(gpu_gen,PFNGLGENQUERIESEXTPROC,"glGenQueriesEXT");
    P(gpu_delete,PFNGLDELETEQUERIESEXTPROC,"glDeleteQueriesEXT");
    P(gpu_begin_query,PFNGLBEGINQUERYEXTPROC,"glBeginQueryEXT");
    P(gpu_end_query,PFNGLENDQUERYEXTPROC,"glEndQueryEXT");
    P(gpu_available,PFNGLGETQUERYOBJECTUIVEXTPROC,"glGetQueryObjectuivEXT");
    P(gpu_result,PFNGLGETQUERYOBJECTUI64VEXTPROC,"glGetQueryObjectui64vEXT");
#undef P
    if(!gpu_gen || !gpu_delete || !gpu_begin_query || !gpu_end_query || !gpu_available || !gpu_result) {
        fprintf(stderr,"[GPU-AUDIT] advertised query entry points missing\n");audit_gpu=false;return true;
    }
    char path[1024];if(snprintf(path,sizeof path,"%s.gpu.csv",audit_path)>=(int)sizeof path)return false;
    gpu_file=fopen(path,"w");if(!gpu_file)return false;
    fputs("frame,elapsed_ns,disjoint,result_age_ticks\n",gpu_file);
    for(int i=0;i<16;i++) gpu_gen(1,&gpu_slots[i].query);
    fprintf(stderr,"[GPU-AUDIT] nonblocking 16-query ring; GL elapsed span is not active GPU utilisation\n");
    return glGetError()==GL_NO_ERROR;
}
static void gpu_poll(uint32_t frame)
{
    if(!audit_gpu) return;
    GLint disjoint=0;glGetIntegerv(GL_GPU_DISJOINT_EXT,&disjoint);
    for(int i=0;i<16;i++) if(gpu_slots[i].pending) {
        if(disjoint)gpu_slots[i].disjoint=true;
        GLuint ready=0;gpu_available(gpu_slots[i].query,GL_QUERY_RESULT_AVAILABLE_EXT,&ready);
        if(ready) {
            GLuint64EXT ns=0;gpu_result(gpu_slots[i].query,GL_QUERY_RESULT_EXT,&ns);
            if(gpu_slots[i].frame>=profile_from) {
                fprintf(gpu_file,"%u,%llu,%d,%u\n",gpu_slots[i].frame,(unsigned long long)ns,gpu_slots[i].disjoint,frame-gpu_slots[i].frame);
                if(gpu_slots[i].disjoint)gpu_invalid++;else gpu_valid++;
            }
            gpu_slots[i].pending=false;
        }
    }
}
static void gpu_begin_frame(uint32_t frame)
{
    if(!audit_gpu)return;
    gpu_poll(frame);int i=gpu_next;gpu_next=(gpu_next+1)%16;
    if(gpu_slots[i].pending) {gpu_dropped++;return;}
    gpu_slots[i].frame=frame;gpu_slots[i].disjoint=false;gpu_active=i;
    gpu_begin_query(GL_TIME_ELAPSED_EXT,gpu_slots[i].query);
}
static void gpu_end_frame(void)
{
    if(!audit_gpu || gpu_active<0)return;
    gpu_end_query(GL_TIME_ELAPSED_EXT);gpu_slots[gpu_active].pending=true;gpu_active=-1;
}
static void gpu_close(void)
{
    if(!gpu_file)return;
    gpu_poll(timing_steps+1);
    unsigned pending=0;for(int i=0;i<16;i++) {pending+=gpu_slots[i].pending;gpu_delete(1,&gpu_slots[i].query);}
    fprintf(stderr,"[GPU-AUDIT] valid=%u disjoint=%u ring_full_skips=%u pending_at_exit=%u\n",gpu_valid,gpu_invalid,gpu_dropped,pending);
    fclose(gpu_file);gpu_file=NULL;
}
