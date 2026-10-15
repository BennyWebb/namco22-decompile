/* Live adapter for the proven M3 command contract; original simulation untouched.
 * Included after private M3 backend and original geometry/list implementation. */
#include "rr_mem.h"
#include "rr_dsp.h"
#include "rr_scene.h"
#include "live_api.h"
#include "rr_gles2.h"
typedef struct {int32_t code,packet,texel;} LiveMesh;
typedef struct {int32_t recipe[7],material;uint8_t palette[768];} LiveMaterial;
typedef struct {Quad3 q;float transform[16];int32_t box[4];unsigned page,protected_,reason;int32_t source_key;} LiveCommand;
static LiveMesh *live_meshes;static LiveMaterial *live_materials;static uint32_t live_nm,live_nt,live_frame,live_from;
static LiveCommand live_cmd[MAXQ3];static Slot3 live_slot;static bool live_prepared,live_loaded;
static uint8_t live_pal[128][768];static double live_walk_ms,live_compile_ms;
static unsigned live_native_pictures;
static void live_fail(const char *why,int code,int packet){fprintf(stderr,"[NATIVE-LIVE-BLOCKER] tick=%u reason=%s code=%d packet=%d emitted=%u\n",live_frame,why,code,packet,count);exit(2);}
static int live_recipe_compare(const int32_t *a,const int32_t*b){for(int i=0;i<7;i++)if(a[i]!=b[i])return a[i]<b[i]?-1:1;return 0;}
static int live_material(const int32_t *recipe){unsigned lo=0,hi=live_nt;while(lo<hi){unsigned mid=lo+(hi-lo)/2;if(live_recipe_compare(live_materials[mid].recipe,recipe)<0)lo=mid+1;else hi=mid;}for(unsigned i=lo;i<live_nt&&!live_recipe_compare(live_materials[i].recipe,recipe);i++)if(!memcmp(live_materials[i].palette,live_pal[recipe[1]],768))return live_materials[i].material;return -1;}
static int live_mesh(int code,int packet){unsigned lo=0,hi=live_nm;while(lo<hi){unsigned m=lo+(hi-lo)/2;LiveMesh *v=live_meshes+m;if(v->code<code||(v->code==code&&v->packet<packet))lo=m+1;else hi=m;}return lo<live_nm&&live_meshes[lo].code==code&&live_meshes[lo].packet==packet?live_meshes[lo].texel:-1;}
static int live_painter(const void*a,const void*b){const LiveCommand*x=a,*y=b;if(x->q.zsort!=y->q.zsort)return x->q.zsort>y->q.zsort?-1:1;return (y->q.order>x->q.order)-(y->q.order<x->q.order);}
static int live_batch(const void*a,const void*b){const LiveCommand*x=a,*y=b;if(x->protected_!=y->protected_)return x->protected_<y->protected_?-1:1;if(x->page!=y->page)return x->page<y->page?-1:1;for(int i=0;i<4;i++)if(x->box[i]!=y->box[i])return x->box[i]<y->box[i]?-1:1;return x->q.depth>y->q.depth?-1:x->q.depth<y->q.depth?1:0;}
static int live_visible(const void*a,const void*b){unsigned x=*(const uint16_t*)a,y=*(const uint16_t*)b;unsigned mx=(live_cmd[x].q.flags>>1)&3,my=(live_cmd[y].q.flags>>1)&3;unsigned nx=mx&1,ny=my&1,px=mx>>1,py=my>>1;if(nx!=ny)return nx<ny?-1:1;if(px!=py)return px<py?-1:1;int c=live_batch(live_cmd+x,live_cmd+y);return c?c:(x>y)-(x<y);}
/* Same conservative float32 projection-error enclosure as compact_race.py. */
static unsigned live_cull(const M3Quad *r){double low[4][3],high[4][3],zoom=ldexp((double)r->meta[24],-r->meta[25]);for(int v=0;v<4;v++)for(int c=0;c<3;c++){int64_t dot=0;double absolute=0;for(int k=0;k<3;k++){int64_t term=(int64_t)r->local[v*6+k]*r->meta[12+k*3+c];dot+=term;absolute+=fabs((double)term);}double tr=r->meta[21+c],error=absolute/(32768.*1048576.)+fabs(tr)/1048576.+2;double xyz=(double)(dot>>15)+tr;low[v][c]=xyz-error;high[v][c]=xyz+error;}for(int v=0;v<4;v++)if(low[v][2]<=1)return 0;bool outside[4]={true,true,true,true};for(int v=0;v<4;v++){double xl=1e100,xh=-1e100,yl=1e100,yh=-1e100;for(int a=0;a<2;a++)for(int b=0;b<2;b++){double z=b?high[v][2]:low[v][2],x=(a?high[v][0]:low[v][0])/z*zoom,y=(a?high[v][1]:low[v][1])/z*zoom;xl=fmin(xl,x);xh=fmax(xh,x);yl=fmin(yl,y);yh=fmax(yh,y);}outside[0]&=r->meta[26]+xh<r->meta[28]-4;outside[1]&=r->meta[26]+xl>r->meta[29]+4;outside[2]&=r->meta[27]-yl<r->meta[30]-4;outside[3]&=r->meta[27]-yh>r->meta[31]+4;}for(unsigned i=0;i<4;i++)if(outside[i])return i+1;return 0;}
static void live_compile(void){
 double start=clock3(CLOCK_MONOTONIC);unsigned direct=rr_scene_direct_count(),n=count+direct;if(n>MAXQ3||direct>512)live_fail("command/dynamic capacity",-1,-1);
 for(int g=0;g<128;g++)for(int i=0;i<256;i++)for(int c=0;c<3;c++)live_pal[g][i*3+c]=g_rr.pal[g*256+i+c*32768];
 float dynamic[4096][4];memset(dynamic,0,sizeof dynamic);
 for(unsigned i=0;i<count;i++){
  const M3Quad*r=records+i;LiveCommand*c=live_cmd+direct+i;memset(c,0,sizeof *c);if(r->meta[0]==5||r->meta[7])live_fail("unsupported point-RAM/solid format",r->meta[0],r->meta[1]);int mesh=live_mesh(r->meta[0],r->meta[1]);if(mesh<0)live_fail("mesh absent from immutable M3 pack",r->meta[0],r->meta[1]);
  int u0=4096,u1=0,v0=4096,v1=0;for(int j=0;j<4;j++){int u=r->local[j*6+3],v=r->local[j*6+4];u0=u<u0?u:u0;u1=u>u1?u:u1;v0=v<v0?v:v0;v1=v>v1?v:v1;CHECK3(r->local[j*6+5]>=0&&r->local[j*6+5]<=65535);c->q.brightness[j]=r->local[j*6+5];}
  int32_t recipe[]={r->meta[5],(r->meta[3]>>8)&127,r->meta[6],u0,v0,u1-u0+1,v1-v0+1};int material=live_material(recipe);if(material<0)live_fail("material/palette absent from immutable M3 pack",r->meta[0],r->meta[1]);c->page=material_data3[material].page;c->q.mesh=mesh;c->q.material=material;c->q.source_instance=r->meta[35];c->source_key=r->meta[35];c->q.order=direct+i;c->q.zsort=r->meta[2]&0xffffff;
  for(int j=0;j<3;j++){for(int k=0;k<3;k++)c->transform[j*4+k]=r->meta[12+k*3+j];c->transform[j*4+3]=r->meta[21+j];}double zoom=ldexp((double)r->meta[24],-r->meta[25]);c->transform[12]=r->meta[26]/320.-1;c->transform[13]=1-r->meta[27]/240.;c->transform[14]=zoom/320;c->transform[15]=zoom/240;
  int l=r->meta[28]>0?r->meta[28]:0,rr=r->meta[29]<639?r->meta[29]:639,t=r->meta[30]>0?r->meta[30]:0,b=r->meta[31]<479?r->meta[31]:479;c->box[0]=l;c->box[1]=479-b;c->box[2]=rr-l+1;c->box[3]=b-t+1;
  bool valid=true;for(int j=0;j<4;j++)valid&=r->oracle[j*7+6]!=0;if(valid)for(int j=0;j<2;j++){int a=j?2:1,b=j?3:2;int64_t ax=(int64_t)r->oracle[a*7]-r->oracle[0],ay=(int64_t)r->oracle[a*7+1]-r->oracle[1],bx=(int64_t)r->oracle[b*7]-r->oracle[0],by=(int64_t)r->oracle[b*7+1]-r->oracle[1];c->protected_|=ax*by-ay*bx==0;}
  c->reason=live_cull(r);if(c->box[2]<=0||c->box[3]<=0)c->reason=5;c->q.flags=((r->meta[6]&7)==1)|((r->meta[34]!=0)<<1)|(c->protected_<<2)|(c->reason<<8);
 }
 for(unsigned i=0;i<direct;i++){
  const uint16_t*w=rr_scene_direct(i);LiveCommand*c=live_cmd+i;memset(c,0,sizeof *c);int u0=4096,u1=0,v0=4096,v1=0;
  for(unsigned j=0;j<4;j++){const uint16_t*s=w+4+j*6;int u=s[0]&4095,v=s[1]&4095;u0=u<u0?u:u0;u1=u>u1?u:u1;v0=v<v0?v:v0;v1=v>v1?v:v1;double ooz=s[5]?ldexp((double)s[5],-(46-(s[4]&63)>0?46-(s[4]&63):0)):65536.;int64_t z=(int64_t)(1./ooz);if(z<1)z=1;if(z>2000000000)z=2000000000;dynamic[i*8+j*2][0]=(float)((int64_t)(int16_t)s[2]*z);dynamic[i*8+j*2][1]=(float)(-(int64_t)(int16_t)s[3]*z);dynamic[i*8+j*2][2]=z;dynamic[i*8+j*2+1][0]=u;dynamic[i*8+j*2+1][1]=v;c->q.brightness[j]=s[4]>>8;}
  int32_t recipe[]={w[5]>>12,(w[2]>>8)&127,w[4]>>12,u0,v0,u1-u0+1,v1-v0+1};int material=live_material(recipe);if(material<0)live_fail("direct material/palette absent from immutable M3 pack",-1,i);c->q.material=material;c->page=material_data3[material].page;c->q.mesh=-1-i*8;c->q.source_instance=65535;c->source_key=-1-(int)i;c->q.order=i;c->q.zsort=((w[1]&4095)<<12)|(w[0]&4095);c->q.flags=((w[4]>>12)&7)==1;c->transform[0]=c->transform[5]=c->transform[10]=32768;c->transform[14]=1.f/320;c->transform[15]=1.f/240;c->box[2]=640;c->box[3]=480;
 }
 qsort(live_cmd,n,sizeof *live_cmd,live_painter);for(unsigned i=0;i<n;i++)live_cmd[i].q.depth=.9-1.8*(i+1)/(n+1.);qsort(live_cmd,n,sizeof *live_cmd,live_batch);
 Instance3 ins[MAXI3];int32_t ins_source[MAXI3];unsigned ni=0;Quad3 quads[MAXQ3];uint16_t visible[MAXQ3];unsigned nv=0;Group3 groups[MAXQ3];unsigned ng=0;
 for(unsigned i=0;i<n;i++){LiveCommand*c=live_cmd+i;unsigned k=0;for(;k<ni;k++){if(ins_source[k]==c->source_key&&!memcmp(ins[k].data,c->transform,64)&&!memcmp(ins[k].box,c->box,16))break;}if(k==ni){if(ni>=MAXI3)live_fail("instance capacity",-1,-1);ins_source[ni]=c->source_key;memcpy(ins[ni].data,c->transform,64);memcpy(ins[ni].box,c->box,16);ni++;}c->q.instance=k;quads[i]=c->q;if(!c->reason)visible[nv++]=i;}
 qsort(visible,nv,2,live_visible);
 for(unsigned i=0;i<nv;i++){LiveCommand*c=live_cmd+visible[i];unsigned mode=(c->q.flags>>1)&3;Group3*g=ng?groups+ng-1:NULL;if(g&&!(mode&2)&&g->page==c->page&&g->mode==mode&&!memcmp(g->box,c->box,16))g->count++;else{g=groups+ng++;g->first=i;g->count=1;g->page=c->page;g->mode=mode;memcpy(g->box,c->box,16);}}
 Header3 h={.magic=0x33504e52,.version=3,.tick=live_frame,.n=n,.visible=nv,.groups=ng,.instances=ni,.dynamic=direct*8};unsigned bgbase=(g_rr.mixer[4]<<8)&0x7f00;for(unsigned c=0;c<3;c++)h.bg[c]=g_rr.pal[bgbase+255+c*32768]/255.f;
 size_t at=0;
#define LIVE_COPY(p,n) do{size_t bytes_=(n);CHECK3(at+bytes_<=RAW3);memcpy(live_slot.bytes+at,(p),bytes_);at+=bytes_;}while(0)
 LIVE_COPY(&h,sizeof h);LIVE_COPY(ins,ni*sizeof *ins);LIVE_COPY(quads,n*sizeof *quads);LIVE_COPY(visible,nv*2);if(at&3){memset(live_slot.bytes+at,0,4-(at&3));at=(at+3)&~3u;}LIVE_COPY(groups,ng*sizeof *groups);LIVE_COPY(dynamic,direct*8*16);LIVE_COPY(g_rr.text,8192);
#undef LIVE_COPY
 live_slot.size=at;live_slot.tick=live_frame;live_prepared=true;live_compile_ms=(clock3(CLOCK_MONOTONIC)-start)*1000;
 const char *dir=getenv("ENG_LIVE_COMMAND_DUMP");if(dir){char path[4096];snprintf(path,sizeof path,"%s/tick-%u.live",dir,live_frame);FILE*f=fopen(path,"wbx");CHECK3(f&&fwrite(live_slot.bytes,1,at,f)==at&&!fclose(f));}
}
bool rr_native_open(void){const char*pack=getenv("ENG_LIVE_PACK"),*shaders=getenv("ENG_LIVE_SHADERS"),*lookup=getenv("ENG_LIVE_LOOKUP");if(!pack||!shaders||!lookup){fprintf(stderr,"[NATIVE-LIVE] explicit pack/shaders/lookup paths required\n");return false;}FILE*f=fopen(lookup,"rb");CHECK3(f);uint32_t h[4];read3(f,h,16);CHECK3(h[0]==0x344c5252&&h[1]==1&&h[2]<=32768&&h[3]<=8192);live_nm=h[2];live_nt=h[3];live_meshes=malloc(live_nm*sizeof *live_meshes);live_materials=malloc(live_nt*sizeof *live_materials);CHECK3(live_meshes&&live_materials);read3(f,live_meshes,live_nm*sizeof *live_meshes);read3(f,live_materials,live_nt*sizeof *live_materials);CHECK3(fgetc(f)==EOF&&!fclose(f));live_from=getenv("ENG_LIVE_FROM")?atoi(getenv("ENG_LIVE_FROM")):1800;native_front3=1;native_ram3=true;stage_sync3=false;double t=clock3(CLOCK_MONOTONIC);load3(pack,shaders);live_loaded=true;fprintf(stderr,"[NATIVE-LIVE] source=live-simulation serial=1 activation_tick=%u preload_ms=%.3f meshes=%u materials=%u no_frame_stream=1\n",live_from,(clock3(CLOCK_MONOTONIC)-t)*1000,live_nm,live_nt);return true;}
void rr_native_set_frame(uint32_t f){live_frame=f;live_prepared=false;}
bool rr_native_active(void){return live_loaded&&live_frame>=live_from;}
void rr_native_prepare(bool slave,bool geometry){bool walk=rr_scene_frame(slave);rr_gles2_scene_tick(walk,rr_scene_direct_count());count=0;double t=clock3(CLOCK_MONOTONIC);if(geometry){if(walk){eng_list_cfg cfg={ENG_LIST_HEAD_S22,1,NULL,NULL,NULL,NULL};eng_walk_list(pw,&cfg,emit,NULL);}live_walk_ms=(clock3(CLOCK_MONOTONIC)-t)*1000;live_compile();}rr_scene_consume();}
bool rr_native_draw(eng_gles2_stats*stats){if(!live_prepared)return false;double t=clock3(CLOCK_MONOTONIC);draw3(&live_slot,0,getenv("ENG_LIVE_CAPTURE")?getenv("ENG_LIVE_CAPTURE"):"-",t,0);Header3*h=(Header3*)live_slot.bytes;memset(stats,0,sizeof *stats);stats->draws=h->groups;stats->triangles=h->visible*2;live_native_pictures++;printf("LIVE frame=%u walk_ms=%.6f compile_ms=%.6f renderer_ms=%.6f draws=%u source=%u visible=%u protected_pipeline=original\n",live_frame,live_walk_ms,live_compile_ms,(clock3(CLOCK_MONOTONIC)-t)*1000,h->groups,h->n,h->visible);return true;}
void rr_native_close(void){if(!live_loaded)return;fprintf(stderr,"[NATIVE-LIVE] completed_native_pictures=%u\n",live_native_pictures);native_flush3();live_loaded=false;}
