/* Existing decoder, executed one sampled tile run at a time. ROM mapping,
 * attribute orientation and palette conversion are unchanged. No cache/state,
 * no allocation, no SIMD rounding, and no speculative/out-of-bounds reads. */
static void texture_decode_tile_runs(uint8_t *dst, int width, int min_u,
    int step, int vrow, int local_y, const uint8_t lut[256][4],
    int cm_offset, int cm_shift, int cm_mask)
{
    for (int px=0; px<width;) {
        int u=(min_u+px*step)&0xfff;
        int index=vrow|((u&0xff0)>>4),off=index*2;
        uint32_t tile=0;int attr=0;
        if(off+1<0x200000) {
            tile=g_texture_tilemap[off]|(g_texture_tilemap[off+1]<<8);
            attr=get_tile_attr(index);
            if((attr&1)&&!g_eng_tex_tile16)tile|=0x10000;
        }
        int count=(16-(u&15)+step-1)/step;
        if(count>width-px)count=width-px;
        int x=u&15,y=local_y,stride=step;
        if(attr&4){x=15-x;stride=-stride;}
        if(attr&2)y=15-y;
        int pos=(int)(tile*256)+(attr&8 ? x*16+y : y*16+x);
        if(attr&8)stride*=16;
        if(tile<TEXTURE_TOTAL_SIZE/256) {
            for(int n=0;n<count;n++,pos+=stride) {
                unsigned fetch=g_texture_data[pos];
                unsigned pen=(cm_offset+((fetch>>cm_shift)&cm_mask))&255;
                memcpy(dst,lut[pen],4);dst+=4;
            }
        } else {
            /* The original per-texel range check produces fetch zero. */
            for(int n=0;n<count;n++){memcpy(dst,lut[cm_offset&255],4);dst+=4;}
        }
        px+=count;
    }
}
