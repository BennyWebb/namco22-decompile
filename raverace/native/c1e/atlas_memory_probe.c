/* Allocation attribution only, no game code or performance results. */
#include <SDL2/SDL.h>
#include <GLES3/gl3.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
static long pages(void){FILE*f=fopen("/sys/kernel/debug/mali0/gpu_memory","r");char label[80];long n=-1;if(f){if(fscanf(f,"%79s %ld",label,&n)!=2)n=-1;fclose(f);}return n;}
#define REQUIRE(x) do{if(!(x)){fprintf(stderr,"probe failure line=%d SDL=%s GL=%x\n",__LINE__,SDL_GetError(),glGetError());exit(2);}}while(0)
static void run(const char*dir,int index){
 unsigned stable=0;for(unsigned i=0;i<100&&stable<3;i++){long n=pages();stable=n==0?stable+1:0;struct timespec delay={0,100000000};nanosleep(&delay,NULL);}REQUIRE(stable==3);REQUIRE(SDL_Init(SDL_INIT_VIDEO)==0);SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,16);SDL_GL_SetAttribute(SDL_GL_RED_SIZE,8);SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,8);SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,8);
 SDL_Window*w=SDL_CreateWindow("C1e atlas allocation diagnostic",0,0,640,480,SDL_WINDOW_OPENGL|SDL_WINDOW_FULLSCREEN);REQUIRE(w);SDL_GLContext c=SDL_GL_CreateContext(w);REQUIRE(c);REQUIRE(SDL_GL_MakeCurrent(w,c)==0);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);SDL_GL_SwapWindow(w);glFinish();long before=pages();REQUIRE(before>=0);
 char path[4096],magic[40];snprintf(path,sizeof path,"%s/%s",dir,index?"atlas.meta":"race.meta");FILE*f=fopen(path,"r");REQUIRE(f);unsigned n,unused[5];if(index)REQUIRE(fscanf(f,"%u",&n)==1);else REQUIRE(fscanf(f,"%39s %u %u %u %u %u %u",magic,&n,unused,unused+1,unused+2,unused+3,unused+4)==7);REQUIRE(n>0&&n<=16);unsigned width[16],height[16];for(unsigned i=0;i<n;i++)REQUIRE(fscanf(f,"%u %u",width+i,height+i)==2);fclose(f);GLuint textures[16];glGenTextures(n,textures);uint64_t bytes=0;
 for(unsigned i=0;i<n;i++){size_t size=(size_t)width[i]*height[i]*(index?1:4);void*p=malloc(size);REQUIRE(p);snprintf(path,sizeof path,"%s/page-%u.%s",dir,i,index?"r8ui":"rgba");f=fopen(path,"rb");REQUIRE(f);REQUIRE(fread(p,1,size,f)==size&&fgetc(f)==EOF);fclose(f);glBindTexture(GL_TEXTURE_2D,textures[i]);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glTexImage2D(GL_TEXTURE_2D,0,index?GL_R8UI:GL_RGBA8,width[i],height[i],0,index?GL_RED_INTEGER:GL_RGBA,GL_UNSIGNED_BYTE,p);glFinish();REQUIRE(glGetError()==GL_NO_ERROR);free(p);bytes+=size;printf("ALLOCATION mode=%s page=%u bytes=%zu mali_pages=%ld\n",index?"index":"rgba",i,size,pages());}
 long after=pages();printf("ATLAS mode=%s baseline_pages=%ld loaded_pages=%ld delta_pages=%ld delta_bytes=%ld api_bytes=%llu gl=%s renderer=%s\n",index?"index":"rgba",before,after,after-before,(after-before)*4096,(unsigned long long)bytes,glGetString(GL_VERSION),glGetString(GL_RENDERER));glDeleteTextures(n,textures);glFinish();printf("RELEASE mode=%s pages=%ld\n",index?"index":"rgba",pages());SDL_GL_DeleteContext(c);SDL_DestroyWindow(w);SDL_Quit();
}
int main(int argc,char**argv){if(argc!=3)return 2;REQUIRE(!strcmp(argv[2],"index")||!strcmp(argv[2],"rgba"));run(argv[1],!strcmp(argv[2],"index"));return 0;}
