/* Diagnostic only, compiled against private geometry source. MIT. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "rr_mem.h"
#include "rr_dsp.h"
#include "rr_scene.h"
#include "geometry.c"
#include "slave_list.c"
const int32_t *g_eng_pointrom;
uint32_t g_eng_pointrom_n;
eng_point_ext_fn g_eng_pointram;
unsigned g_eng_frame;
extern int32_t *g_pointrom;
extern uint32_t g_pointrom_words;
/* Fixed little-endian protocol record. Metadata layout is documented separately. */
typedef struct {int32_t meta[36],local[24],oracle[28];} M3Quad;
_Static_assert(sizeof(M3Quad)==352,"capture ABI");
static M3Quad records[32768];static unsigned count;
static uint32_t pw(int i){return g_rr.poly[i&0x7fff];}
static void emit(const geo_quad*q,void*unused){
 (void)unused;if(count>=32768)abort();M3Quad*r=records+count;memset(r,0,sizeof *r);
 int32_t m[]={g_bbox_cur,(int32_t)m3_packet,q->zsort,(int32_t)q->color,q->flags_raw,q->texbank,q->cmode,q->objectflags,q->cz_adjust,q->cz_type,q->cz_value,(int32_t)count};memcpy(r->meta,m,sizeof m);
 memcpy(r->meta+12,g_view.m,9*4);memcpy(r->meta+21,g_view.t,3*4);r->meta[24]=g_view.zoom_mant;r->meta[25]=g_view.zoom_shift;r->meta[26]=320+g_view.vx;r->meta[27]=240+g_view.vy;memcpy(r->meta+28,q->clip,4*4);
 r->meta[32]=q->nrv;r->meta[33]=q->ndv;r->meta[34]=q->behind;r->meta[35]=m3_instance;
 for(int i=0;i<4;i++){
  for(int c=0;c<3;c++)r->local[i*6+c]=pt_read(m3_packet+8+i*3+c);
  r->local[i*6+3]=pt_read(m3_packet+i*2)&4095;r->local[i*6+4]=pt_read(m3_packet+i*2+1)&4095;r->local[i*6+5]=m3_brightness[i];
  const geo_vert*v=q->v+i;int32_t a[]={v->sx16,v->sy16,v->z,(int32_t)v->u,(int32_t)v->v,v->bri,v->valid};memcpy(r->oracle+i*7,a,sizeof a);
 }count++;
}
static void write_bytes(FILE*f,const void*p,size_t n){if(fwrite(p,1,n,f)!=n)abort();}
void m3_capture(unsigned frame,int walk){
 const char*dir=getenv("ENG_NATIVE_CAPTURE");if(frame==1800)fprintf(stderr,"[NATIVE-CAPTURE-START] directory=%s\n",dir?dir:"unset");if(!dir||frame<1800||frame>5639)return;
 g_eng_pointrom=g_pointrom;g_eng_pointrom_n=g_pointrom_words;g_eng_pointram=rr_dsp_pointram_read;g_eng_frame=frame;count=0;
 if(walk){eng_list_cfg cfg={ENG_LIST_HEAD_S22,1,NULL,NULL,NULL,NULL};eng_walk_list(pw,&cfg,emit,NULL);}
 char path[4096];snprintf(path,sizeof path,"%s/tick-%u.bin",dir,frame);FILE*f=fopen(path,"wbx");if(!f)abort();
 unsigned direct=rr_scene_direct_count();uint32_t header[]={0x33435352,1,frame,walk,count,direct,sizeof(M3Quad),RR_PAL_SIZE+RR_MIXER_SIZE+RR_CZRAM_SIZE+RR_CGRAM_SIZE+RR_TEXT_SIZE+16};
 write_bytes(f,header,sizeof header);write_bytes(f,records,count*sizeof *records);
 for(unsigned i=0;i<direct;i++)write_bytes(f,rr_scene_direct(i),56);
 write_bytes(f,g_rr.pal,RR_PAL_SIZE);write_bytes(f,g_rr.mixer,RR_MIXER_SIZE);write_bytes(f,g_rr.czram,RR_CZRAM_SIZE);write_bytes(f,g_rr.cgram,RR_CGRAM_SIZE);write_bytes(f,g_rr.text,RR_TEXT_SIZE);write_bytes(f,g_rr.tilemapattr,16);
 if(fclose(f))abort();
 if(frame%300==0||frame==5639)fprintf(stderr,"[NATIVE-CAPTURE] tick=%u quads=%u direct=%u\n",frame,count,direct);
}
