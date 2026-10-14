/* Frozen-world and synthetic HUD equivalence probe. See GPU_HUD_PROTOTYPE.md.
 * Uses the production compositor and the original build_text extracted at build. */
#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#define RR_GLES2 1
#define RR_GPU_HUD 1
#include "../src/rr_video_layers.c"
unsigned g_eng_frame;
void eng_gles2_priority(bool on){(void)on;}
#define NW 640
#define NH 480
static rr_video_cpu_frame state;
static uint8_t raw[0x20000],attributes[16],txt_rgba[NW*NH*4],shd_rgba[NW*NH*4];
static uint8_t *case_cgram=raw,*case_text=raw+0x1e000,*case_tilemapattr=attributes;
#define VIDEO_ARRAY(field) case_##field
static int mixer_b(int off){return off>=8&&off<17?state.multipliers[(off-8)/3][(off-8)%3]:0;}
static void pen_rgb(int pen,uint8_t *out){memcpy(out,state.palette[pen&255],4);}
#include "hud_cpu_reference.inc"
static uint8_t world_pixels[NW*NH*4],expected[NW*NH*4],actual[NW*NH*4];
static bool load(const char *prefix,const char *suffix,void *data,size_t n)
{
 char path[1024];snprintf(path,sizeof path,"%s.%s",prefix,suffix);FILE *f=fopen(path,"rb");if(!f)return false;bool ok=fread(data,1,n,f)==n;fclose(f);return ok;
}
static void prepare(void)
{
 int sx=(state.scroll_x+0x35c)&1023;attributes[0]=sx>>8;attributes[1]=sx;attributes[2]=state.scroll_y>>8;attributes[3]=state.scroll_y;
 state.characters=raw;state.tilemap=raw+0x1e000;state.text=txt_rgba;state.shadow=shd_rgba;
 state.any_text=build_text(0,state.shadow_enabled,&state.any_shadow);
 glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,scene);glTexSubImage2D(GL_TEXTURE_2D,0,0,0,NW,NH,GL_RGBA,GL_UNSIGNED_BYTE,world_pixels);
}
static bool check(unsigned id)
{
 g_eng_frame=id;prepare();
 if(!rr_video_layers_compose(state.text,state.shadow,true,state.any_text,state.any_shadow,state.lut))return false;
 glReadPixels(0,0,NW,NH,GL_RGBA,GL_UNSIGNED_BYTE,expected);
 if(!rr_video_layers_compose_gpu(&state))return false;
 glReadPixels(0,0,NW,NH,GL_RGBA,GL_UNSIGNED_BYTE,actual);
 size_t differing=0;unsigned max=0;for(size_t i=0;i<sizeof actual;i++){unsigned d=abs((int)actual[i]-expected[i]);if(d){differing++;if(d>max)max=d;}}
 if(differing){fprintf(stderr,"case=%u channels=%zu max=%u\n",id,differing,max);return false;}return true;
}
static int compare_double(const void *a,const void *b){double x=*(const double *)a,y=*(const double *)b;return (x>y)-(x<y);}
static void timing(bool dynamic)
{
 if(!strstr((const char *)glGetString(GL_EXTENSIONS),"GL_EXT_disjoint_timer_query")){puts("GPU elapsed queries unavailable");return;}
 PFNGLGENQUERIESEXTPROC gen=(PFNGLGENQUERIESEXTPROC)SDL_GL_GetProcAddress("glGenQueriesEXT");
 PFNGLBEGINQUERYEXTPROC begin=(PFNGLBEGINQUERYEXTPROC)SDL_GL_GetProcAddress("glBeginQueryEXT");
 PFNGLENDQUERYEXTPROC end=(PFNGLENDQUERYEXTPROC)SDL_GL_GetProcAddress("glEndQueryEXT");
 PFNGLGETQUERYOBJECTUI64VEXTPROC get=(PFNGLGETQUERYOBJECTUI64VEXTPROC)SDL_GL_GetProcAddress("glGetQueryObjectui64vEXT");
 PFNGLDELETEQUERIESEXTPROC del=(PFNGLDELETEQUERIESEXTPROC)SDL_GL_GetProcAddress("glDeleteQueriesEXT");
 if(!gen||!begin||!end||!get||!del)return;
 GLuint q;gen(1,&q);double values[2][100];GLint initial;glGetIntegerv(GL_GPU_DISJOINT_EXT,&initial);
 /* Deliberately synchronous isolated shader-cost measurement. Resources are
  * unchanged and uploads warmed; this is never the full-game benchmark. */
 for(int i=-5;i<100;i++)for(int order=0;order<2;order++){
  int mode=(i&1)^order;
  if(dynamic){raw[0x1e000]^=4;prepare();}
  begin(GL_TIME_ELAPSED_EXT,q);
  bool ok=mode?rr_video_layers_compose_gpu(&state):rr_video_layers_compose(state.text,state.shadow,dynamic,state.any_text,state.any_shadow,state.lut);
  end(GL_TIME_ELAPSED_EXT);GLuint64EXT ns=0;get(q,GL_QUERY_RESULT_EXT,&ns);GLint disjoint;glGetIntegerv(GL_GPU_DISJOINT_EXT,&disjoint);
  if(!ok||disjoint){printf("GPU timing invalid mode=%d iteration=%d ok=%d disjoint=%d\n",mode,i,ok,disjoint);del(1,&q);return;}if(i>=0)values[mode][i]=ns/1e6;
 }
 for(int mode=0;mode<2;mode++){qsort(values[mode],100,sizeof(double),compare_double);printf("FROZEN-GPU-SPAN mode=%s updates=%s samples=100 median_ms=%.6f p95_ms=%.6f scope=HUD_commands_includes_CPU_feeding_gaps\n",mode?(gpu_geometry()?"glyph-quads":"packed-tilemap"):"reference",dynamic?"dirty":"warm",(values[mode][49]+values[mode][50])/2,values[mode][94]);}del(1,&q);
}
int main(int argc,char **argv)
{
 if(SDL_Init(SDL_INIT_VIDEO))return 2;SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
 SDL_Window *w=SDL_CreateWindow("HUD probe",SDL_WINDOWPOS_UNDEFINED,SDL_WINDOWPOS_UNDEFINED,NW,NH,SDL_WINDOW_OPENGL|SDL_WINDOW_FULLSCREEN);if(!w)return 2;
 SDL_GLContext ctx=SDL_GL_CreateContext(w);if(!ctx)return 2;SDL_GL_SetSwapInterval(0);setenv("ENG_GPU_HUD","1",1);unsetenv("ENG_GPU_HUD_VERIFY");unsetenv("ENG_GPU_HUD_CAPTURE");
 if(!rr_video_layers_open())return 3;text_tex=texture(NW,NH);shadow_tex=texture(NW,NH);
 unsigned cases=0;
 for(int arg=1;arg<argc;arg++){
  uint8_t input[16+0x20000+1024+9+1024];uint32_t h[4];
  if(!load(argv[arg],"state",input,sizeof input)||!load(argv[arg],"world.rgba",world_pixels,sizeof world_pixels))return 4;
  memcpy(h,input,16);if(h[0]!=0x48554431)return 4;state.scroll_x=h[1];state.scroll_y=h[2];state.shadow_enabled=h[3];size_t at=16;
#define READ_PART(p,n) do{memcpy(p,input+at,n);at+=n;}while(0)
  READ_PART(raw,sizeof raw);READ_PART(state.palette,1024);READ_PART(state.multipliers,9);READ_PART(state.lut,1024);
#undef READ_PART
  if(!check(++cases))return 5;
  if(!load(argv[arg],"reference.rgba",expected,sizeof expected)||memcmp(actual,expected,sizeof actual)){fprintf(stderr,"captured reference mismatch %s\n",argv[arg]);return 5;}
  if(strstr(argv[arg],"/hud-2502")){printf("TIMING fixture=%s\n",argv[arg]);timing(false);timing(true);}
 }
 /* Explicit edge coverage: nibble order, both flips, all pen values and palette
  * groups, wrapping scroll, last characters aliasing tilemap, all shadow pens,
  * priority masks, palette changes and nontrivial output LUTs. */
 for(unsigned variant=0;variant<32;variant++){
  for(unsigned i=0;i<sizeof raw;i++)raw[i]=(uint8_t)((i*37+(i>>7)*19+variant*13)^(i>>3));
  for(unsigned i=0;i<4096;i++){unsigned word=((i+variant*17)&1023)|((i&3)<<10)|(((i+variant)&15)<<12);raw[0x1e000+i*2]=word>>8;raw[0x1e001+i*2]=word;}
  for(unsigned i=0;i<256;i++){for(int c=0;c<3;c++)state.palette[i][c]=(i*(17+c*20)+variant*(13+c))&255;state.palette[i][3]=255;state.lut[i][0]=(i*3)&255;state.lut[i][1]=255-i;state.lut[i][2]=i/2;state.lut[i][3]=255;}
  for(int k=0;k<3;k++)for(int c=0;c<3;c++)state.multipliers[k][c]=(variant*53+k*79+c*31)&255;
  state.scroll_x=(variant*137)&1023;state.scroll_y=(variant*251)&1023;state.shadow_enabled=variant&1;
  for(unsigned y=0;y<NH;y++)for(unsigned x=0;x<NW;x++){size_t p=(y*NW+x)*4;world_pixels[p]=x+variant;world_pixels[p+1]=y*3;world_pixels[p+2]=x+y;world_pixels[p+3]=variant&2?(((x/7+y/11)&1)?255:0):0;}
  if(!check(++cases))return 5;
 }
 printf("PASS cases=%u RGBA_pixels=%u\n",cases,cases*NW*NH);rr_video_layers_close();SDL_GL_DeleteContext(ctx);SDL_DestroyWindow(w);SDL_Quit();return 0;
}
