/* GPU-owner-only compatible-dimension arrays. The logical texture cache and
 * its replacement order remain authoritative. Each cohort owns two layers. */
#define WORLD_ARRAY_PAGES 4096
#define WORLD_ARRAY_NONE UINT16_MAX
typedef struct {GLuint texture; uint16_t w,h; unsigned mask; GLsync retired;} world_array_page;
static world_array_page world_pages[WORLD_ARRAY_PAGES];
static int world_arrays_enabled, world_selected_layer=-1;
static GLuint world_selected_reference,world_proofs[TEX_CACHE_SIZE];
static int world_proof_enabled;
static size_t world_array_bytes,world_array_peak;
static double world_upload_barrier_seconds;
static unsigned long long world_array_allocations,world_array_uploads,world_array_reuses,world_array_waits;
void tex_bake_world_arrays(int enabled,int proof){world_arrays_enabled=enabled;world_proof_enabled=proof;}
int tex_bake_world_layer(void){return world_selected_layer;}
GLuint tex_bake_world_reference(void){return world_selected_reference;}
void tex_bake_world_dimensions(GLuint reference,int *w,int *h)
{
    *w=*h=1;
    for(int i=0;i<TEX_CACHE_SIZE;i++)if(tex_cache[i].occupied && world_proofs[i]==reference){*w=tex_cache[i].alloc_w;*h=tex_cache[i].alloc_h;return;}
}
static void world_select(TexCacheEntry *e)
{
    world_selected_layer=world_arrays_enabled && e->a_page!=WORLD_ARRAY_NONE ? e->a_x:-1;
    world_selected_reference=world_proofs[e-tex_cache];
}
static void world_reap(int blocking)
{
    for(int i=0;i<WORLD_ARRAY_PAGES;i++){
        world_array_page *p=world_pages+i;if(!p->retired)continue;
        GLenum status=glClientWaitSync(p->retired,blocking?GL_SYNC_FLUSH_COMMANDS_BIT:0,blocking?1000000000ULL:0);
        if(status==GL_WAIT_FAILED){fprintf(stderr,"[WORLD-ARRAY] fence failed\n");exit(2);}
        if(status!=GL_ALREADY_SIGNALED && status!=GL_CONDITION_SATISFIED)continue;
        glDeleteSync(p->retired);glDeleteTextures(1,&p->texture);
        world_array_bytes-=(size_t)p->w*p->h*8;memset(p,0,sizeof *p);
        if(blocking)return;
    }
}
static void world_release(TexCacheEntry *e)
{
    if(e->a_page==WORLD_ARRAY_NONE){tex_free_push(e->gl_texture,e->alloc_w,e->alloc_h);return;}
    world_array_page *p=world_pages+e->a_page;
    if(atlas_flush_hook)atlas_flush_hook(p->texture);
    GLuint *proof=world_proofs+(e-tex_cache);
    if(*proof){if(atlas_flush_hook)atlas_flush_hook(*proof);glDeleteTextures(1,proof);*proof=0;}
    p->mask&=~(1u<<e->a_x);
    if(!p->mask){p->retired=glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE,0);if(!p->retired){fprintf(stderr,"[WORLD-ARRAY] fence creation failed\n");exit(2);}}
    e->a_page=WORLD_ARRAY_NONE;
    tex_bake_gles2_binding_reset();
}
static GLuint world_upload(TexCacheEntry *e,int w,int h,int bw,int bh,int *page,int *layer)
{
    world_reap(0);
    int index=-1;
    for(int i=0;i<WORLD_ARRAY_PAGES;i++){
        world_array_page *p=world_pages+i;
        if(p->texture && !p->retired && p->w==w && p->h==h && p->mask!=3){index=i;world_array_reuses++;break;}
    }
    if(index<0){
        size_t incoming=(size_t)w*h*8,limit=2*tex_cache_budget;
        while(world_array_bytes+incoming>limit){
            size_t previous=world_array_bytes;world_array_waits++;world_reap(1);
            if(previous==world_array_bytes){int retired=0;for(int i=0;i<WORLD_ARRAY_PAGES;i++)retired|=world_pages[i].retired!=NULL;if(!retired){fprintf(stderr,"[WORLD-ARRAY] physical budget exhausted\n");exit(2);}}
        }
        for(int i=0;i<WORLD_ARRAY_PAGES;i++)if(!world_pages[i].texture){index=i;break;}
        if(index<0){world_array_waits++;world_reap(1);for(int i=0;i<WORLD_ARRAY_PAGES;i++)if(!world_pages[i].texture){index=i;break;}}
        /* Exhausting the metadata bound is a failed experiment, never a reason
         * to alter the decoder's quality or evict additional logical entries. */
        if(index<0){fprintf(stderr,"[WORLD-ARRAY] cohort bound exhausted\n");exit(2);}
        world_array_page *p=world_pages+index;p->w=w;p->h=h;
        glGenTextures(1,&p->texture);glBindTexture(GL_TEXTURE_2D_ARRAY,p->texture);
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D_ARRAY,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexImage3D(GL_TEXTURE_2D_ARRAY,0,GL_RGBA8,w,h,2,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
        world_array_bytes+=incoming;world_array_allocations++;
        if(world_array_bytes>world_array_peak)world_array_peak=world_array_bytes;
    }
    world_array_page *p=world_pages+index;int slot=(p->mask&1)?1:0;
    if(atlas_flush_hook){double barrier=gles_seconds();atlas_flush_hook(p->texture);world_upload_barrier_seconds+=gles_seconds()-barrier;}
    glBindTexture(GL_TEXTURE_2D_ARRAY,p->texture);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY,0,0,0,slot,bw,bh,1,GL_RGBA,GL_UNSIGNED_BYTE,tex_pixel_buf);
    p->mask|=1u<<slot;world_array_uploads++;tex_subimages++;
    if(world_proof_enabled){
        GLuint *proof=world_proofs+(e-tex_cache);glGenTextures(1,proof);glBindTexture(GL_TEXTURE_2D,*proof);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,NULL);
        glTexSubImage2D(GL_TEXTURE_2D,0,0,0,bw,bh,GL_RGBA,GL_UNSIGNED_BYTE,tex_pixel_buf);
    }
    tex_bake_gles2_binding_reset();*page=index;*layer=slot;return p->texture;
}
static void world_cache_clear(void)
{
    for(int i=0;i<WORLD_ARRAY_PAGES;i++){
        world_array_page *p=world_pages+i;if(p->retired)glDeleteSync(p->retired);if(p->texture)glDeleteTextures(1,&p->texture);memset(p,0,sizeof *p);
    }
    for(int i=0;i<TEX_CACHE_SIZE;i++)if(world_proofs[i]){glDeleteTextures(1,world_proofs+i);world_proofs[i]=0;}
    world_array_bytes=0;world_selected_layer=-1;world_selected_reference=0;
}
static void world_cache_report(void)
{
    fprintf(stderr,"[WORLD-ARRAY] physical_bytes=%zu peak_bytes=%zu allocations=%llu uploads=%llu hole_reuses=%llu fence_waits=%llu metadata_bytes=%zu proof=%d\n",world_array_bytes,world_array_peak,world_array_allocations,world_array_uploads,world_array_reuses,world_array_waits,sizeof world_pages+sizeof world_proofs,world_proof_enabled);
}
