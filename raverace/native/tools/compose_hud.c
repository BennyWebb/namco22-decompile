/* Isolated captured HUD/compositor validation. No game simulation or native loop. */
#define main original_hud_probe_main
#include "../../tools/gpu_hud_probe.c"
#undef main
int main(int argc,char**argv){
 if(argc!=4)return 2;
 if(SDL_Init(SDL_INIT_VIDEO))return 2;
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,2);
 SDL_Window*w=SDL_CreateWindow("Captured native HUD check",0,0,640,480,SDL_WINDOW_OPENGL);
 if(!w||!SDL_GL_CreateContext(w))return 2;
 setenv("ENG_GPU_HUD","1",1);unsetenv("ENG_GPU_HUD_VERIFY");unsetenv("ENG_GPU_HUD_CAPTURE");
 if(!rr_video_layers_open())return 3;
 text_tex=texture(NW,NH);shadow_tex=texture(NW,NH);
 uint8_t input[16+0x20000+1024+9+1024];uint32_t h[4];
 if(!load(argv[1],"state",input,sizeof input))return 4;
 memcpy(h,input,16);if(h[0]!=0x48554431)return 4;
 state.scroll_x=h[1];state.scroll_y=h[2];state.shadow_enabled=h[3];size_t at=16;
 memcpy(raw,input+at,sizeof raw);at+=sizeof raw;
 memcpy(state.palette,input+at,1024);at+=1024;
 memcpy(state.multipliers,input+at,9);at+=9;
 memcpy(state.lut,input+at,1024);
 FILE*f=fopen(argv[2],"rb");if(!f||fread(world_pixels,4,NW*NH,f)!=NW*NH)return 4;fclose(f);
 if(!check(1))return 5; /* exact original CPU compositor vs production GPU HUD */
 uint8_t*old=malloc(sizeof actual),*oldworld=malloc(sizeof actual);
 if(!old||!oldworld||!load(argv[1],"reference.rgba",old,sizeof actual)||!load(argv[1],"world.rgba",oldworld,sizeof actual))return 4;
 unsigned opaque=0,differences=0,alpha_changes=0,glyph_priority_changes=0;
 for(unsigned i=0;i<NW*NH;i++){
  alpha_changes+=oldworld[i*4+3]!=world_pixels[i*4+3];
  unsigned ti=((NH-1-i/NW)*NW+i%NW)*4;
  if(txt_rgba[ti+3]&&((oldworld[i*4+3]<128)!=(world_pixels[i*4+3]<128)))glyph_priority_changes++;
  if(txt_rgba[ti+3]&&oldworld[i*4+3]<128&&world_pixels[i*4+3]<128){opaque++;differences+=memcmp(old+i*4,actual+i*4,4)!=0;}
 }
 f=fopen(argv[3],"wbx");if(!f||fwrite(actual,4,NW*NH,f)!=NW*NH)return 4;fclose(f);
 printf("HUD_CHECK cpu_gpu_differences=0 opaque_glyph_pixels=%u changed_opaque_glyph_pixels=%u world_priority_pixels_changed=%u changed_glyph_priority_pixels=%u\n",opaque,differences,alpha_changes,glyph_priority_changes);
 rr_video_layers_close();SDL_Quit();free(old);free(oldworld);return (differences||glyph_priority_changes)?5:0;
}
