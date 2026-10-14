/* Independent, bounded GPU preload diagnostic. Captured RGBA is never trusted
 * as live state: compare the complete normally decoded bake before adopting an
 * unused private object. Cache lookup, tiers, palette generations and eviction
 * remain the ordinary path. These objects never enter the cache ahead of use. */
#define GPW_LIMIT (8u*1024u*1024u)
#define GPW_ENTRIES 128
typedef struct { uint32_t key[9]; uint8_t *pixels; GLuint texture; size_t bytes; } GpuPrepared;
static GpuPrepared gpw[GPW_ENTRIES];
static unsigned gpw_n,gpw_hits,gpw_mismatch,gpw_record_n;
static size_t gpw_reserved,gpw_initial,gpw_total_peak,gpw_record_bytes;
static FILE *gpw_record;
static unsigned gpw_record_from=1965,gpw_record_to=1970;
static const char gpw_magic[]="RR_GPU_PREWARM_1\n";
static int gpw_read32(FILE *f,uint32_t *v)
{
    *v=0;
    for(unsigned i=0;i<4;i++){int c=fgetc(f);if(c==EOF)return 0;*v|=(uint32_t)c<<(8*i);}
    return 1;
}
static void gpw_write32(FILE *f,uint32_t v)
{for(unsigned i=0;i<4;i++)fputc((v>>(8*i))&255,f);}
static void gpw_init(void)
{
    static int initialized;
    if(initialized)return;
    initialized=1;
    const char *path=getenv("ENG_TEXTURE_GPU_RECORD");
    if(path&&*path) {
        gpw_record=fopen(path,"wx");
        if(!gpw_record){perror("[GPU-PREWARM] record");exit(2);}
        setvbuf(gpw_record,NULL,_IOFBF,1024*1024);fputs(gpw_magic,gpw_record);
        const char *e=getenv("ENG_TEXTURE_GPU_RECORD_FROM");if(e)gpw_record_from=strtoul(e,NULL,10);
        e=getenv("ENG_TEXTURE_GPU_RECORD_TO");if(e)gpw_record_to=strtoul(e,NULL,10);
    }
    path=getenv("ENG_TEXTURE_GPU_PREWARM");if(!path||!*path)return;
    double start=gles_seconds();FILE *f=fopen(path,"rb");char header[64];
    if(!f){perror("[GPU-PREWARM] open; fallback");return;}
    if(!fgets(header,sizeof header,f)||strcmp(header,gpw_magic)){fclose(f);return;}
    while(gpw_n<GPW_ENTRIES) {
        GpuPrepared p={0};unsigned n;
        for(n=0;n<9;n++)if(!gpw_read32(f,&p.key[n]))break;
        if(n!=9)break;
        uint32_t *k=p.key;
        if(k[0]>4095||k[1]>4095||!k[2]||k[2]>4096||!k[3]||k[3]>4096||k[4]>15||k[5]>15||(k[6]!=256&&k[6]!=512&&k[6]!=1024)||!k[7]||k[7]>1025||!k[8]||k[8]>1025)break;
        p.bytes=(size_t)k[7]*k[8]*4;
        if(p.bytes>GPW_LIMIT-gpw_reserved)break;
        p.pixels=malloc(p.bytes);if(!p.pixels)break;
        if(fread(p.pixels,1,p.bytes,f)!=p.bytes){free(p.pixels);break;}
        glGenTextures(1,&p.texture);glBindTexture(GL_TEXTURE_2D,p.texture);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,k[7],k[8],0,GL_RGBA,GL_UNSIGNED_BYTE,p.pixels);
        if(glGetError()!=GL_NO_ERROR){glDeleteTextures(1,&p.texture);free(p.pixels);break;}
        gpw[gpw_n++]=p;gpw_reserved+=p.bytes;
    }
    fclose(f);gpw_initial=gpw_reserved;gpw_total_peak=gpw_reserved;
    tex_bake_gles2_binding_reset();
    fprintf(stderr,"[GPU-PREWARM] startup_seconds=%.6f objects=%u CPU_reference_bytes=%zu GPU_reserved_bytes=%zu limit=%u\n",gles_seconds()-start,gpw_n,gpw_initial,gpw_initial,GPW_LIMIT);
}
static void gpw_record_bake(const uint32_t key[9],const uint8_t *pixels)
{
    size_t bytes=(size_t)key[7]*key[8]*4;
    if(!gpw_record||g_eng_frame<gpw_record_from||g_eng_frame>gpw_record_to||gpw_record_n>=GPW_ENTRIES||bytes>GPW_LIMIT-gpw_record_bytes)return;
    for(unsigned i=0;i<9;i++)gpw_write32(gpw_record,key[i]);
    fwrite(pixels,1,bytes,gpw_record);gpw_record_bytes+=bytes;gpw_record_n++;
    if(ferror(gpw_record)){fprintf(stderr,"[GPU-PREWARM] record failure\n");exit(2);}
}
static GLuint gpw_take(const uint32_t key[9],const uint8_t *pixels)
{
    for(unsigned i=0;i<gpw_n;i++) {
        GpuPrepared *p=&gpw[i];
        if(!p->texture||memcmp(key,p->key,sizeof p->key))continue;
        if(memcmp(pixels,p->pixels,p->bytes)){gpw_mismatch++;continue;}
        GLuint t=p->texture;p->texture=0;gpw_reserved-=p->bytes;gpw_hits++;
        return t;
    }
    return 0;
}
static void gpw_track(size_t active)
{if(active+gpw_reserved>gpw_total_peak)gpw_total_peak=active+gpw_reserved;}
static void gpw_close(void)
{
    if(gpw_record){fclose(gpw_record);gpw_record=NULL;fprintf(stderr,"[GPU-PREWARM-RECORD] objects=%u bytes=%zu\n",gpw_record_n,gpw_record_bytes);}
    if(gpw_n)fprintf(stderr,"[GPU-PREWARM] hits=%u content_mismatch=%u unused=%u GPU_reserved_remaining=%zu GPU_active_plus_reserved_peak=%zu CPU_reference_bytes=%zu\n",gpw_hits,gpw_mismatch,gpw_n-gpw_hits,gpw_reserved,gpw_total_peak,gpw_initial);
    for(unsigned i=0;i<gpw_n;i++){if(gpw[i].texture)glDeleteTextures(1,&gpw[i].texture);free(gpw[i].pixels);gpw[i].pixels=NULL;gpw[i].texture=0;}
}
