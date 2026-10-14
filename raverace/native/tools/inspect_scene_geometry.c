/* Milestone 1 CLI source-geometry oracle. Uses the incumbent geometry stage. */
#include <stdio.h>
#include <stdlib.h>
#include "../../../engine/geo_hw.c"
#include "../../../engine/slave_list.c"
const int32_t *g_eng_pointrom;
uint32_t g_eng_pointrom_n;
eng_point_ext_fn g_eng_pointram;
static uint32_t poly[0x8000];
static int32_t ram[0x20000];
static int wanted;
static int32_t point_ram(uint32_t a){return a>=0xf00000 && a<0xf20000?ram[a-0xf00000]:-1;}
static uint32_t word(int i){return poly[i&0x7fff];}
static void load(const char*name,void*data,size_t n){FILE*f=fopen(name,"rb");if(!f||fread(data,1,n,f)!=n)exit(2);fclose(f);}
static void output(const geo_quad*q,void*unused){
    (void)unused;if(g_bbox_cur!=wanted)return;
    printf("{\"code\":%d,\"color\":%u,\"flags\":%d,\"bank\":%d,\"cmode\":%d,\"m\":[",g_bbox_cur,q->color,q->flags_raw,q->texbank,q->cmode);
    for(int r=0;r<3;r++)for(int c=0;c<3;c++)printf("%s%d",r||c?",":"",g_view.m[r][c]);
    printf("],\"t\":[%d,%d,%d],\"zoom\":[%d,%d],\"center\":[%d,%d],\"vertices\":[",g_view.t[0],g_view.t[1],g_view.t[2],g_view.zoom_mant,g_view.zoom_shift,320+g_view.vx,240+g_view.vy);
    for(int i=0;i<4;i++){const geo_vert*v=&q->v[i];printf("%s[%d,%d,%d,%u,%u,%d,%d]",i?",":"",v->sx16,v->sy16,v->z,v->u,v->v,v->bri,v->valid);}
    puts("]}");
}
int main(int argc,char**argv){if(argc!=5)return 2;int32_t*rom=malloc(0x200000*4);if(!rom)return 2;load(argv[1],rom,0x200000*4);g_eng_pointrom=rom;g_eng_pointrom_n=0x200000;g_eng_pointram=point_ram;load(argv[2],poly,sizeof poly);load(argv[3],ram,sizeof ram);wanted=atoi(argv[4]);eng_list_cfg cfg={ENG_LIST_HEAD_S22,1,NULL,NULL,NULL,NULL};eng_walk_list(word,&cfg,output,NULL);free(rom);return ferror(stdout)?2:0;}
