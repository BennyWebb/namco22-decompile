#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "dynamic.h"
/* Exercise cached snapshots surviving repeated region/id eviction before display. */
static unsigned char expected[3][G1B_BYTES],gpu[3][G1B_BYTES];static G1BSlot queued[3];
int main(void){unsigned long long checksum=1469598103934665603ull;unsigned evictions=0;for(unsigned frame=0;frame<300;frame++){
 unsigned slot=frame%3;G1BSlot*s=queued+slot;memset(s,0,sizeof *s);int key[]={frame%16,frame%16,(frame*73)%4096,(frame*131)%4096,500,500};bool decode;int id=g1b_reserve(s,key,&decode);assert(id>=0);G1BEntry*e=g1b_entries+id;evictions+=s->evictions;assert(decode);s->uploads=1;s->bytes=e->w*e->h;s->upload[0]=(G1BUpload){.entry=id,.x=e->x,.y=e->y,.w=e->w,.h=e->h,.generation=e->generation};for(unsigned i=0;i<s->bytes;i++)s->pens[i]=(frame+i*17)&255;memcpy(expected[slot],s->pens,s->bytes);
 /* A second pending picture can replace CPU cache content, not the queued bytes. */
 G1BSlot*tmp=calloc(1,sizeof *tmp);int other[]={frame%16,0,frame,frame,800,800};int j=g1b_reserve(tmp,other,&decode);assert(j>=0);evictions+=tmp->evictions;free(tmp);assert(!memcmp(expected[slot],s->pens,s->bytes));
 for(unsigned y=0;y<s->upload[0].h;y++)memcpy(gpu[slot]+(s->upload[0].y+y)*1024+s->upload[0].x,s->pens+y*s->upload[0].w,s->upload[0].w);
 for(unsigned y=0;y<s->upload[0].h;y++)assert(!memcmp(gpu[slot]+(s->upload[0].y+y)*1024+s->upload[0].x,expected[slot]+y*s->upload[0].w,s->upload[0].w));checksum=(checksum^e->generation)*1099511628211ull;
 }
 G1BSlot*s=queued;memset(s,0,sizeof *s);int bad[]={0,0,0,0,1021,1};bool decode;assert(g1b_reserve(s,bad,&decode)==-1);printf("PASS frames=300 interleaved_evictions=%u checksum=%llu bounds=PASS\n",evictions,checksum);return 0;}
