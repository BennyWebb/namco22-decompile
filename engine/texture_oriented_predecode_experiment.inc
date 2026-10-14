/* Complete immutable ROM representation, independent of geometry/replays and
 * palettes. Tilemap entries select oriented 16x16 raw-pen tiles. */
static uint32_t *oriented_ids;
static uint8_t *oriented_pixels;
static const uint8_t *oriented_rom,*oriented_map;
static int oriented_tile16;
static unsigned oriented_count;
static unsigned long long oriented_bakes,oriented_texels;
static void oriented_summary(void)
{
    fprintf(stderr,"[ORIENTED-PREDECODE] bakes=%llu texels=%llu CPU_bytes=%zu GPU_extra_bytes=0\n",oriented_bakes,oriented_texels,(size_t)0x100000*4+(size_t)oriented_count*256);
}
static unsigned oriented_key(unsigned index,unsigned *tile_out,int *attr_out)
{
    unsigned tile=g_texture_tilemap[index*2]|(g_texture_tilemap[index*2+1]<<8);
    int attr=get_tile_attr(index);
    if((attr&1)&&!g_eng_tex_tile16)tile|=0x10000;
    *tile_out=tile;*attr_out=attr;
    return tile<TEXTURE_TOTAL_SIZE/256 ? tile*8+((attr&14)>>1) : (TEXTURE_TOTAL_SIZE/256)*8;
}
static void oriented_prepare(int verify)
{
    if(!getenv("ENG_TEXTURE_PREDECODE_ORIENTED")||!g_texture_data||!g_texture_tilemap)return;
    double start=gles_seconds();
    uint32_t *keys=calloc((TEXTURE_TOTAL_SIZE/256)*8+1,sizeof *keys);
    uint32_t *ids=malloc((size_t)0x100000*sizeof *ids);
    if(!keys||!ids){free(keys);free(ids);return;}
    unsigned count=0;
    for(unsigned i=0;i<0x100000;i++) {
        unsigned tile;int attr;unsigned key=oriented_key(i,&tile,&attr);
        if(!keys[key]) {
            if(count==131072){fprintf(stderr,"[ORIENTED-PREDECODE] capacity fallback\n");free(keys);free(ids);return;}
            keys[key]=++count;
        }
        ids[i]=keys[key]-1;
    }
    uint8_t *pixels=malloc((size_t)count*256),*done=calloc(count,1);
    if(!pixels||!done){free(keys);free(ids);free(pixels);free(done);return;}
    for(unsigned i=0;i<0x100000;i++) {
        unsigned id=ids[i];if(done[id])continue;done[id]=1;
        unsigned tile;int attr;oriented_key(i,&tile,&attr);
        for(unsigned y=0;y<16;y++)for(unsigned x=0;x<16;x++) {
            unsigned sx=x,sy=y;
            if(attr&4)sx=15-sx;if(attr&2)sy=15-sy;
            if(attr&8){unsigned t=sx;sx=sy;sy=t;}
            unsigned offset=tile*256+sy*16+sx;
            uint8_t pen=offset<TEXTURE_TOTAL_SIZE?g_texture_data[offset]:0;
            pixels[id*256+y*16+x]=pen;
            if(verify&&pen!=texture_pen_lookup((i&255)*16+x,((i>>8)&255)*16+y,i>>16)) {
                fprintf(stderr,"[ORIENTED-PREDECODE] raw pen verification failure\n");exit(2);
            }
        }
    }
    free(keys);free(done);
    oriented_ids=ids;oriented_pixels=pixels;oriented_count=count;
    oriented_rom=g_texture_data;oriented_map=g_texture_tilemap;oriented_tile16=g_eng_tex_tile16;
    atexit(oriented_summary);
    fprintf(stderr,"[ORIENTED-PREDECODE] startup_seconds=%.6f unique_tiles=%u CPU_bytes=%zu scratch_peak_bytes=%zu verified_raw_pens=%zu GPU_extra_bytes=0\n",gles_seconds()-start,count,(size_t)0x100000*4+(size_t)count*256,(size_t)((TEXTURE_TOTAL_SIZE/256)*8+1)*4+count,verify?(size_t)count*256:0);
}
__attribute__((always_inline)) static inline void oriented_decode_row(uint8_t *dst,int width,int min_u,int step,int vrow,int y,const uint8_t lut[256][4],int offset,int shift,int mask)
{
    for(int px=0;px<width;) {
        unsigned u=(min_u+px*step)&4095,index=vrow|((u&4080)>>4);
        int count=(16-(u&15)+step-1)/step;if(count>width-px)count=width-px;
        {
            /* The existing tile-run decoder uses tile zero for an invalid
             * map address. Keep that fallback for diagnostic callers too. */
            const uint8_t *src=index<0x100000 ? oriented_pixels+(size_t)oriented_ids[index]*256+y*16+(u&15) : g_texture_data+y*16+(u&15);
            if(!offset&&!shift&&mask==255)
                for(int n=0;n<count;n++){memcpy(dst,lut[src[n*step]],4);dst+=4;}
            else
                for(int n=0;n<count;n++){unsigned pen=(offset+((src[n*step]>>shift)&mask))&255;memcpy(dst,lut[pen],4);dst+=4;}
        }
        px+=count;
    }
}
