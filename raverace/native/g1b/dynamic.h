/* CPU allocator owns immutable ROM pens. Slot copies sever cache lifetime from GL. */
#define G1B_SIDE 1024
#define G1B_CELL 8
#define G1B_ENTRIES 4096
#define G1B_MATERIALS 512
#define G1B_BYTES (1024*1024)
typedef struct {int key[6];unsigned x,y,w,h;uint64_t generation,last;bool occupied;} G1BEntry;
typedef struct {unsigned entry,x,y,w,h,offset;uint64_t generation;int key[6];} G1BUpload;
typedef struct {float uv[4];unsigned group,upload;int recipe[7];} G1BMaterial;
typedef struct {unsigned tick,uploads,materials,bytes,hits,misses,evictions;G1BUpload upload[G1B_MATERIALS];G1BMaterial material[G1B_MATERIALS];uint8_t pens[G1B_BYTES];} G1BSlot;
static G1BEntry g1b_entries[G1B_ENTRIES];static uint16_t g1b_cells[128*128];static uint8_t g1b_page[G1B_BYTES];static uint64_t g1b_generation,g1b_serial;
static void g1b_mark(unsigned id,bool fill){G1BEntry*e=g1b_entries+id;for(unsigned y=e->y/8;y<(e->y+e->h+7)/8;y++)for(unsigned x=e->x/8;x<(e->x+e->w+7)/8;x++)g1b_cells[y*128+x]=fill?id+1:0;}
static bool g1b_place(unsigned w,unsigned h,unsigned*x,unsigned*y){unsigned cw=(w+7)/8,ch=(h+7)/8;for(unsigned yy=0;yy+ch<=128;yy++)for(unsigned xx=0;xx+cw<=128;xx++){bool empty=true;for(unsigned j=0;j<ch&&empty;j++)for(unsigned i=0;i<cw;i++)if(g1b_cells[(yy+j)*128+xx+i]){empty=false;break;}if(empty){*x=xx*8;*y=yy*8;return true;}}return false;}
static bool g1b_pinned(const G1BSlot*s,unsigned i){for(unsigned j=0;j<s->uploads;j++)if(s->upload[j].entry==i)return true;return false;}
static unsigned g1b_lru(const G1BSlot*s){unsigned best=G1B_ENTRIES;for(unsigned i=0;i<G1B_ENTRIES;i++)if(g1b_entries[i].occupied&&!g1b_pinned(s,i)&&(best==G1B_ENTRIES||g1b_entries[i].last<g1b_entries[best].last))best=i;return best;}
/* Returns an existing or freshly allocated footprint. Palette is deliberately absent. */
static int g1b_reserve(G1BSlot*s,const int key[6],bool*decode){
 if(key[0]<0||key[0]>15||key[1]<0||key[1]>15||key[2]<0||key[2]>4095||key[3]<0||key[3]>4095||key[4]<1||key[5]<1||key[4]>1020||key[5]>1020)return -1;
 for(unsigned i=0;i<G1B_ENTRIES;i++)if(g1b_entries[i].occupied&&!memcmp(g1b_entries[i].key,key,24)){s->hits++;g1b_entries[i].last=++g1b_serial;*decode=false;return i;}
 s->misses++;unsigned x,y,id;
 for(;;){for(id=0;id<G1B_ENTRIES&&g1b_entries[id].occupied;id++);if(id<G1B_ENTRIES&&g1b_place(key[4]+4,key[5]+4,&x,&y))break;unsigned old=g1b_lru(s);if(old==G1B_ENTRIES)return -1;g1b_mark(old,false);g1b_entries[old].occupied=false;s->evictions++;}
 G1BEntry*e=g1b_entries+id;memcpy(e->key,key,24);e->x=x;e->y=y;e->w=key[4]+4;e->h=key[5]+4;e->generation=++g1b_generation;e->last=++g1b_serial;e->occupied=true;g1b_mark(id,true);*decode=true;return id;
}
