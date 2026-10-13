/* Test the real input bridge with SDL virtual hardware; no game ROM required.
 * gcc -Iengine -Iraverace/include ... $(pkg-config --cflags --libs sdl2) */
#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
static SDL_GameController *controllers[16];
#ifdef RR_GLES2
static unsigned overlay_toggles;
void rr_overlay_toggle(void) { overlay_toggles++; }
#endif
#include "../src/rr_gles2_input.inc"
rr_hw_t g_hw;
static const rr_game_t game={.steer_min=0x280,.steer_max=0xd80,.gas_max=0x610,.brake_max=0x610,.gas_delta=160,.brake_delta=160};
const rr_game_t *g_rr_game=&game;
static void tick(void) { SDL_JoystickUpdate(); SDL_PumpEvents(); assert(input_poll()); rr_gles2_apply_input(); }
int main(int argc,char **argv)
{
    assert(argc==2); assert(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER)==0);
    int index=SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_UNKNOWN,2,17,1); assert(index>=0);
    char guid[33]; SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(index),guid,sizeof guid);
    es_input=argv[1]; FILE *f=fopen(es_input,"w"); assert(f);
    fprintf(f,"<inputConfig deviceGUID=\"%s\">\n",guid);
    const char *names[]={"b","a","pageup","pagedown","select","start","left","right"};
    for(int i=0;i<8;i++) fprintf(f,"<input name=\"%s\" type=\"button\" id=\"%d\" value=\"1\" />\n",names[i],i);
    fprintf(f,"<input name=\"joystick1left\" type=\"axis\" id=\"0\" value=\"-1\" />\n</inputConfig>\n"); fclose(f);
    input_map_device(index); assert(SDL_IsGameController(index));
    controllers[0]=SDL_GameControllerOpen(index); assert(controllers[0]);
    SDL_Joystick *js=SDL_GameControllerGetJoystick(controllers[0]);
    interactive=true; g_hw.inputs=0xfeff; g_hw.steer=0x800;
#ifdef RR_GLES2
    SDL_Event overlay_event={.type=SDL_CONTROLLERBUTTONDOWN};
    overlay_event.cbutton.button=SDL_CONTROLLER_BUTTON_Y;
    assert(input_event(&overlay_event));assert(overlay_toggles==1);
#endif
    tick();assert(g_hw.steer==0x800 && g_hw.gas==0 && (g_hw.inputs&0x1000));
    SDL_JoystickSetVirtualButton(js,4,1);tick();assert(!(g_hw.inputs&0x1000));
    SDL_JoystickSetVirtualButton(js,4,0);SDL_JoystickSetVirtualButton(js,5,1);tick();assert(g_hw.gas==160 && (g_hw.inputs&0x1000));
    SDL_JoystickSetVirtualButton(js,5,0);SDL_JoystickSetVirtualButton(js,1,1);tick();assert(g_hw.brake==160 && g_hw.gas==0);
    SDL_JoystickSetVirtualAxis(js,0,32767);tick();assert(g_hw.steer==0xd80);
    SDL_JoystickSetVirtualButton(js,6,1);tick();assert(g_hw.steer==0x280); /* D-pad overrides stick */
    SDL_JoystickSetVirtualButton(js,6,0);SDL_JoystickSetVirtualAxis(js,0,1024);tick();assert(g_hw.steer==0x800);
    interactive=false;g_hw.steer=123;rr_gles2_apply_input();assert(g_hw.steer==123);
    interactive=true;
    /* DOWN and UP drained before an emulated sample must still be delivered. */
    SDL_Event event={0};event.type=SDL_CONTROLLERBUTTONDOWN;
    event.cbutton.which=SDL_JoystickInstanceID(js);event.cbutton.button=SDL_CONTROLLER_BUTTON_BACK;
    assert(input_event(&event));event.type=SDL_CONTROLLERBUTTONUP;assert(input_event(&event));
    input_coin=false;g_hw.inputs|=0x1000;rr_gles2_apply_input();assert(!(g_hw.inputs&0x1000));
    rr_gles2_apply_input();assert(g_hw.inputs&0x1000);
    event.type=SDL_CONTROLLERBUTTONDOWN;event.cbutton.button=SDL_CONTROLLER_BUTTON_START;assert(input_event(&event));
    event.type=SDL_CONTROLLERBUTTONUP;assert(input_event(&event));input_gas=false;g_hw.gas=0;
    for(int i=0;i<10;i++)rr_gles2_apply_input();assert(g_hw.gas==0x610);
    rr_gles2_apply_input();assert(g_hw.gas==0x610-160);
    event.type=SDL_CONTROLLERBUTTONDOWN;assert(input_event(&event));event.cbutton.button=SDL_CONTROLLER_BUTTON_BACK;assert(!input_event(&event));
    SDL_JoystickSetVirtualButton(js,4,1);SDL_JoystickSetVirtualButton(js,5,1);SDL_JoystickUpdate();assert(!input_poll());
    SDL_GameControllerClose(controllers[0]);SDL_JoystickDetachVirtual(index);SDL_Quit();puts("Mapping, coin release, START confirm, brake, analog/deadzone, D-pad override, script isolation and quit passed");
}
