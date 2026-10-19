/* Validation-only CPU text/palette/LUT oracle. Never draws or substitutes. */
#include "renderer_api.h"
#include <GLES3/gl3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern void gles3_read_scene(uint8_t*);
void gles3_check_boot(const rr_presentation*s){
 if(s->frame>159)return;
 static uint8_t world[640*480*4],gpu[640*480*4];gles3_read_scene(world);glReadPixels(0,0,640,480,GL_RGBA,GL_UNSIGNED_BYTE,gpu);
 const rr_video_cpu_frame*f=&s->video;size_t differences=0,nonclear=0,text=0,shadow=0;
 unsigned bgbase=(s->mixer[4]<<8)&0x7f00;uint8_t bg[3];for(unsigned c=0;c<3;c++)bg[c]=s->pal[bgbase+255+c*32768];
 for(unsigned y=0;y<480;y++)for(unsigned x=0;x<640;x++){
  unsigned off=(y*640+x)*4;uint8_t c[4]={world[off],world[off+1],world[off+2],255};
  nonclear+=memcmp(c,bg,3)!=0||world[off+3]!=0;
  if(world[off+3]<128){unsigned tx=(x+f->scroll_x)&1023,ty=(479-y+f->scroll_y)&1023,at=((ty>>4)*64+(tx>>4))*2;unsigned w=f->tilemap[at]*256+f->tilemap[at+1],cx=tx&15,cy=ty&15;if(w&0x400)cx=15-cx;if(w&0x800)cy=15-cy;at=(w&1023)*128+cy*8+cx/2;unsigned byte=at<0x1e000?f->characters[at]:f->tilemap[at-0x1e000],pen=(cx&1)?byte&15:byte>>4;
   if(pen!=15){unsigned ix=((w>>12)<<4)|pen;if(f->shadow_enabled&&ix>=252&&ix<=254){for(unsigned k=0;k<3;k++)c[k]=c[k]*f->multipliers[ix-252][k]/256;shadow++;}else{memcpy(c,f->palette[ix],3);text++;}}
  }
  for(unsigned k=0;k<3;k++)c[k]=f->lut[c[k]][k];for(unsigned k=0;k<4;k++)differences+=c[k]!=gpu[off+k];
 }
 fprintf(stderr,"[GLES3-BOOT-ORACLE] tick=%u world_nonclear=%zu text_pixels=%zu shadow_pixels=%zu differences=%zu\n",s->frame,nonclear,text,shadow,differences);
 if(differences)gles3_unsupported(s->frame,s->g1_main,s->g1_sub,-1,-1,NULL,"boot CPU text/palette oracle mismatch");
}
