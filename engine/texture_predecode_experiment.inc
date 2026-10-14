/* Temporary CPU-only experiment. The manifest selects work, never resources.
 * All samples are built from the ROMs loaded in this process; palettes remain
 * live. A miss uses the ordinary decoder. No GPU cache insertion or GL calls. */
#define PEN_BUCKETS 16384
#define PEN_LIMIT (128u * 1024u * 1024u)
#define PEN_ENTRY_LIMIT 16384
typedef struct PenBake {
    int u,v,ru,rv,bank,cmode,cap,w,h,tile16;
    uint8_t *pens;
    struct PenBake *next;
} PenBake;
static PenBake *pen_buckets[PEN_BUCKETS];
static size_t pen_bytes,pen_count;
static unsigned long long pen_hits,pen_fallbacks;
static unsigned long long pen_verified_texels;
static int pen_verify;
#include "texture_oriented_predecode_experiment.inc"
static unsigned pen_hash(int u,int v,int ru,int rv,int bank,int cmode,int cap)
{
    unsigned h=2166136261u;
    int keys[]={u,v,ru,rv,bank,cmode,cap};
    for(unsigned i=0;i<7;i++){h^=(unsigned)keys[i];h*=16777619u;}
    return h&(PEN_BUCKETS-1);
}
static const uint8_t *pen_find(int u,int v,int ru,int rv,int bank,int cmode,int cap,int w,int h)
{
    for(PenBake *p=pen_buckets[pen_hash(u,v,ru,rv,bank,cmode,cap)];p;p=p->next)
        if(p->u==u&&p->v==v&&p->ru==ru&&p->rv==rv&&p->bank==bank&&p->cmode==cmode&&p->cap==cap&&p->w==w&&p->h==h&&p->tile16==g_eng_tex_tile16)
            return p->pens;
    return NULL;
}
static void pen_summary(void)
{
    fprintf(stderr,"[PREDECODE] hits=%llu fallback=%llu bytes=%zu entries=%zu verified_texels=%llu GPU_preloaded=0\n",pen_hits,pen_fallbacks,pen_bytes,pen_count,pen_verified_texels);
}
void tex_bake_predecode_prepare(void)
{
    static int initialized;
    if(initialized)return;
    initialized=1;
    pen_verify=getenv("ENG_TEXTURE_PREDECODE_VERIFY")!=NULL;
    oriented_prepare(pen_verify);
    const char *path=getenv("ENG_TEXTURE_PREDECODE_MANIFEST");
    if(!path||!*path)return;
    double start=gles_seconds();
    FILE *f=fopen(path,"r");
    if(!f){perror("[PREDECODE] manifest");return;}
    char line[256];
    if(!fgets(line,sizeof line,f)||strcmp(line,"RR_PENS_EXPERIMENT_1\n")){fclose(f);fprintf(stderr,"[PREDECODE] invalid manifest; fallback\n");return;}
    while(fgets(line,sizeof line,f)) {
        PenBake key={0};char extra;
        if(sscanf(line,"%d %d %d %d %d %d %d %d %d %c",&key.u,&key.v,&key.ru,&key.rv,&key.bank,&key.cmode,&key.cap,&key.w,&key.h,&extra)!=9)continue;
        if(key.u<0||key.u>4095||key.v<0||key.v>4095||key.ru<1||key.ru>4096||key.rv<1||key.rv>4096||key.bank<0||key.bank>15||key.cmode<0||key.cmode>15||(key.cap!=256&&key.cap!=512&&key.cap!=1024)||key.w<1||key.w>1025||key.h<1||key.h>1025)continue;
        if(pen_find(key.u,key.v,key.ru,key.rv,key.bank,key.cmode,key.cap,key.w,key.h))continue;
        size_t bytes=(size_t)key.w*key.h;
        if(bytes>PEN_LIMIT-pen_bytes||pen_count>=PEN_ENTRY_LIMIT)continue;
        PenBake *p=malloc(sizeof *p);
        uint8_t *pens=malloc(bytes);
        if(!p||!pens){free(p);free(pens);break;}
        *p=key;p->pens=pens;p->tile16=g_eng_tex_tile16;
        int offset,shift,mask;cmode_params(key.cmode,&offset,&shift,&mask);
        int su=(key.ru+key.cap-1)/key.cap,sv=(key.rv+key.cap-1)/key.cap;
        for(int y=0;y<key.h;y++)for(int x=0;x<key.w;x++)
            pens[(size_t)y*key.w+x]=(offset+((texture_pen_lookup(key.u+x*su,key.v+y*sv,key.bank)>>shift)&mask))&255;
        unsigned bucket=pen_hash(key.u,key.v,key.ru,key.rv,key.bank,key.cmode,key.cap);
        p->next=pen_buckets[bucket];pen_buckets[bucket]=p;
        pen_bytes+=bytes;pen_count++;
    }
    fclose(f);atexit(pen_summary);
    fprintf(stderr,"[PREDECODE] startup_seconds=%.6f bytes=%zu metadata=%zu entries=%zu limit=%u GPU_preloaded=0\n",gles_seconds()-start,pen_bytes,sizeof pen_buckets+pen_count*sizeof(PenBake),pen_count,PEN_LIMIT);
}
