/* Real GLES compositor readback against System 22's integer composition rules.
 * cc -DRR_GLES2 -Iengine -Iraverace/include $(pkg-config --cflags sdl2)
 *   raverace/tools/test_video_layers.c raverace/src/rr_video_layers.c
 *   $(pkg-config --libs sdl2) -lGLESv2 -lm -o test_video_layers */
#include "rr_video_layers.h"
#include <SDL.h>
#include <GLES2/gl2.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char text[640*480*4],shadow[640*480*4],lut[256][4];
void eng_gles2_priority(bool on){(void)on;}
static void check(bool priority,bool any_text,bool any_shadow,unsigned r,unsigned g,unsigned b)
{
 rr_video_layers_begin();glDisable(GL_SCISSOR_TEST);glDisable(GL_BLEND);
 glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
 glClearColor(64/255.f,128/255.f,192/255.f,priority?1:0);glClear(GL_COLOR_BUFFER_BIT);
 assert(rr_video_layers_compose(text,shadow,true,any_text,any_shadow,lut));
 unsigned char pixel[4];glReadPixels(320,240,1,1,GL_RGBA,GL_UNSIGNED_BYTE,pixel);
 if(pixel[0]!=r||pixel[1]!=g||pixel[2]!=b){fprintf(stderr,"got %u/%u/%u expected %u/%u/%u\n",pixel[0],pixel[1],pixel[2],r,g,b);assert(0);}
}
int main(void)
{
 assert(SDL_Init(SDL_INIT_VIDEO)==0);
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);
 SDL_GL_SetAttribute(SDL_GL_RED_SIZE,8);SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,8);SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,8);
 SDL_Window *w=SDL_CreateWindow("video layer test",0,0,640,480,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);assert(w);
 SDL_GLContext c=SDL_GL_CreateContext(w);assert(c);assert(rr_video_layers_open());
 for(unsigned i=0;i<256;i++){lut[i][0]=lut[i][1]=lut[i][2]=i;lut[i][3]=255;}
 check(false,false,false,64,128,192);
 for(unsigned i=0;i<640*480;i++){
  text[i*4]=200;text[i*4+1]=80;text[i*4+2]=33;text[i*4+3]=255;
  shadow[i*4]=128;shadow[i*4+1]=64;shadow[i*4+2]=255;shadow[i*4+3]=255;
 }
 check(false,true,false,200,80,33);check(true,true,true,64,128,192);
 check(false,false,true,32,32,191);check(false,true,true,200,80,33);
 memset(text,0,sizeof text);memset(shadow,0,sizeof shadow);
 check(false,true,true,64,128,192);
 /* Non-identity per-channel fade/gamma lookup and opaque final alpha. */
 for(unsigned i=0;i<256;i++){lut[i][0]=255-i;lut[i][1]=i/2;lut[i][2]=(i*3)%256;}
 check(false,false,false,191,64,64);
 rr_video_layers_close();SDL_GL_DeleteContext(c);SDL_DestroyWindow(w);SDL_Quit();
 puts("Priority, transparency, shadows, text, and per-channel LUT readback passed");
}
