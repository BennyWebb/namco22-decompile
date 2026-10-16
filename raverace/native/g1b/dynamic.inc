#include "dynamic.h"
extern uint8_t texture_pen_lookup(int,int,int);
static G1BSlot*g1b_slot;static const G1BSlot*g1b_drawing;static unsigned g1b_page_index;
static GLuint g1b_pages[3],g1b_uv[3],g1b_mapping[3];static uint64_t g1b_resident[3][G1B_ENTRIES];
static int g1b_last_recipe[7],g1b_fail_code,g1b_fail_packet;static bool g1b_key_valid;
static int32_t*g1b_reference;static unsigned g1b_reference_count;
static uint8_t g1b_pen(int bank,int cm,int u,int v){unsigned p=texture_pen_lookup(u,v,bank);if(cm&4)return (0xec+((cm&8)<<1)+((p>>(2*(~cm&3)))&3))&255;if(cm&2)return (0xe0+((cm&8)<<1)+((p>>(4*(~cm&1)))&15))&255;return p;}
static int g1b_reference_id(const int*r){unsigned lo=0,hi=g1b_reference_count;while(lo<hi){unsigned m=lo+(hi-lo)/2;int c=0;for(int j=0;j<7&&!c;j++)c=(g1b_reference[m*8+j]>r[j])-(g1b_reference[m*8+j]<r[j]);if(c<0)lo=m+1;else hi=m;}return lo<g1b_reference_count&&!memcmp(g1b_reference+lo*8,r,28)?g1b_reference[lo*8+7]:-1;}
static unsigned g1b_group(unsigned material){return material<material_count3?c1_group[material]:g1b_drawing->material[material-material_count3].group;}
static unsigned g1b_material_page(unsigned m){return m<material_count3?material_data3[m].page:g1b_page_index;}
static int g1b_material(const int*r){
 memcpy(g1b_last_recipe,r,28);g1b_key_valid=true;
 for(unsigned i=0;i<g1b_slot->materials;i++)if(!memcmp(g1b_slot->material[i].recipe,r,28)){g1b_slot->hits++;return material_count3+i;}
 int key[]={r[0],r[2],r[3],r[4],r[5],r[6]};bool decode=false;int id=g1b_reserve(g1b_slot,key,&decode);if(id<0)live_fail("dynamic footprint dimensions/capacity",-1,-1);G1BEntry*e=g1b_entries+id;
 if(decode){for(unsigned y=0;y<e->h;y++)for(unsigned x=0;x<e->w;x++)g1b_page[(e->y+y)*1024+e->x+x]=g1b_pen(key[0],key[1],key[2]-2+x,key[3]-2+y);}
 unsigned u;for(u=0;u<g1b_slot->uploads;u++)if(g1b_slot->upload[u].entry==(unsigned)id)break;
 if(u==g1b_slot->uploads){if(u>=G1B_MATERIALS||g1b_slot->bytes+e->w*e->h>G1B_BYTES)live_fail("dynamic slot capacity",-1,-1);G1BUpload*p=g1b_slot->upload+u;*p=(G1BUpload){.entry=id,.x=e->x,.y=e->y,.w=e->w,.h=e->h,.offset=g1b_slot->bytes,.generation=e->generation};memcpy(p->key,key,24);for(unsigned y=0;y<e->h;y++)memcpy(g1b_slot->pens+p->offset+y*e->w,g1b_page+(e->y+y)*1024+e->x,e->w);g1b_slot->bytes+=e->w*e->h;g1b_slot->uploads++;
#ifdef G1B_VALIDATION
 if(decode){unsigned char hash[32];SHA256(g1b_slot->pens+p->offset,e->w*e->h,hash);fprintf(stderr,"[G1B-DECODE] tick=%u key=%d,%d,%d,%d,%d,%d bytes=%u sha256=",live_frame,key[0],key[1],key[2],key[3],key[4],key[5],e->w*e->h);for(unsigned j=0;j<32;j++)fprintf(stderr,"%02x",hash[j]);fprintf(stderr,"\n");}
#endif
 }
 unsigned m=g1b_slot->materials++;if(m>=G1B_MATERIALS)live_fail("dynamic material capacity",-1,-1);G1BMaterial*v=g1b_slot->material+m;v->uv[0]=v->uv[1]=1.f/1024;v->uv[2]=(e->x+2.5-r[3])/1024.;v->uv[3]=(e->y+2.5-r[4])/1024.;v->group=r[1];v->upload=u;memcpy(v->recipe,r,28);return material_count3+m;
}
static void g1b_upload(const rr_presentation*s,unsigned copy){
 g1b_drawing=s->g1b_dynamic;CHECK3(g1b_drawing);pages3[g1b_page_index]=g1b_pages[copy];materials3=g1b_uv[copy];if(c1d_debug)c1d_mapping=g1b_mapping[copy];unsigned bytes=0;
 glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,g1b_pages[copy]);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
 for(unsigned j=0;j<g1b_drawing->uploads;j++){const G1BUpload*p=g1b_drawing->upload+j;if(g1b_resident[copy][p->entry]!=p->generation){glTexSubImage2D(GL_TEXTURE_2D,0,p->x,p->y,p->w,p->h,GL_RED_INTEGER,GL_UNSIGNED_BYTE,g1b_drawing->pens+p->offset);g1b_resident[copy][p->entry]=p->generation;bytes+=p->w*p->h;}}
 for(unsigned j=0;j<g1b_drawing->materials;j++){const G1BMaterial*m=g1b_drawing->material+j;unsigned id=material_count3+j;glBindTexture(GL_TEXTURE_2D,g1b_uv[copy]);glTexSubImage2D(GL_TEXTURE_2D,0,id%1024,id/1024,1,1,GL_RGBA,GL_FLOAT,m->uv);if(c1d_debug){const G1BUpload*p=g1b_drawing->upload+m->upload;int map[]={m->recipe[0],m->recipe[3],m->recipe[4],p->x+2,p->y+2,m->recipe[5],m->recipe[6],m->recipe[1]};unsigned at=id*2;glBindTexture(GL_TEXTURE_2D,g1b_mapping[copy]);glTexSubImage2D(GL_TEXTURE_2D,0,at%1024,at/1024,2,1,GL_RGBA_INTEGER,GL_INT,map);}}
 CHECK3(glGetError()==GL_NO_ERROR);glActiveTexture(GL_TEXTURE0);
 fprintf(stderr,"[G1B-UPLOAD] tick=%u copy=%u bytes=%u materials=%u hits=%u misses=%u evictions=%u\n",s->frame,copy,bytes,g1b_drawing->materials,g1b_drawing->hits,g1b_drawing->misses,g1b_drawing->evictions);
}
