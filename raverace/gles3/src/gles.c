/* GLES3-only SDL/input host. No world-renderer backend or legacy switches. */
#include "renderer_api.h"
#include <SDL.h>
#include <GLES3/gl3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "rr_gles3.h"
#include "rr_sim_profile.h"
#include "rr_video_layers.h"
#include "rr_overlay.h"
#include "rr_sound.h"
static SDL_Window *window;
static SDL_GLContext context;
static SDL_GameController *controllers[16];
static SDL_Joystick *raw_joysticks[16];
#include "rr_input_stream.h"
#include "rr_hw.h"
/* Host adapter only: unchanged EEPROM state is snapshotted on the simulation
 * thread; the low-priority supervisor persists it atomically in gles3 saves. */
static bool g2_save_enabled;static unsigned g2_save_ticks;static unsigned char g2_last_eeprom[RR_EEPROM_SIZE];
static void g2_save_emit(void){if(!g2_save_enabled||!memcmp(g2_last_eeprom,g_rr.eeprom,RR_EEPROM_SIZE))return;static const char hex[]="0123456789abcdef";char line[32+RR_EEPROM_SIZE*2];unsigned at=0;const char*prefix="[G2-EEPROM] ";while(*prefix)line[at++]=*prefix++;for(unsigned i=0;i<RR_EEPROM_SIZE;i++){line[at++]=hex[g_rr.eeprom[i]>>4];line[at++]=hex[g_rr.eeprom[i]&15];}line[at++]='\n';fwrite(line,1,at,stderr);memcpy(g2_last_eeprom,g_rr.eeprom,RR_EEPROM_SIZE);}
static void g2_save_tick(void){if(g2_save_enabled&&++g2_save_ticks%120==0)g2_save_emit();}
static bool g2_save_open(bool enabled){g2_save_enabled=enabled;if(!enabled)return true;const char*p=getenv("ENG_GLES3_SAVE_PATH");if(!p)return false;FILE*f=fopen(p,"rb");if(!f)return false;bool ok=fread(g_rr.eeprom,1,RR_EEPROM_SIZE,f)==RR_EEPROM_SIZE&&fgetc(f)==EOF;fclose(f);if(!ok)return false;memcpy(g2_last_eeprom,g_rr.eeprom,RR_EEPROM_SIZE);fprintf(stderr,"[G2-SAVE] loaded=%s bytes=%u transport=async-supervisor\n",p,RR_EEPROM_SIZE);return true;}

/* Small SDL input bridge. No desktop UI, audio or simulation timing changes. */
#include "rr_hw.h"
#include "rr_game.h"
static bool interactive, input_log;
static const char *controller_db, *es_input = "/userdata/system/configs/emulationstation/es_input.cfg";
static bool input_coin, input_gas, input_brake, input_view, input_up, input_down;
static int input_wheel;
static unsigned coin_edges, start_ticks;
static bool chord_start[16], chord_select[16];
/* Event edges survive DOWN+UP in one queue drain (including a slow swap).
 * Only consume them when an emulated input sample is applied. */
static bool input_event(const SDL_Event *event)
{
#ifdef RR_GLES3
    if ((event->type==SDL_KEYDOWN && !event->key.repeat && event->key.keysym.scancode==SDL_SCANCODE_F3) ||
        (event->type==SDL_CONTROLLERBUTTONDOWN && event->cbutton.button==SDL_CONTROLLER_BUTTON_Y)) rr_overlay_toggle();
#endif
    if (!interactive) return true;
    if (event->type == SDL_KEYDOWN && !event->key.repeat) {
        if (event->key.keysym.scancode==SDL_SCANCODE_5 && coin_edges<8) coin_edges++;
        if (event->key.keysym.scancode==SDL_SCANCODE_RETURN) start_ticks=10;
    }
    if (event->type==SDL_CONTROLLERBUTTONDOWN || event->type==SDL_CONTROLLERBUTTONUP) {
        bool down=event->type==SDL_CONTROLLERBUTTONDOWN;
        for (int i=0;i<16;i++) if (controllers[i] &&
            SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controllers[i]))==event->cbutton.which) {
            if (event->cbutton.button==SDL_CONTROLLER_BUTTON_START) {
                chord_start[i]=down; if(down)start_ticks=10;
            }
            if (event->cbutton.button==SDL_CONTROLLER_BUTTON_BACK) {
                chord_select[i]=down; if(down && coin_edges<8)coin_edges++;
            }
            if (chord_start[i] && chord_select[i]) return false;
        }
    }
    return true;
}

/* Read only the simple attribute-per-line format emitted by EmulationStation.
 * Unsupported/truncated input is ignored; an explicit SDL mapping is available. */
static bool input_attr(const char *line, const char *name, char *out, size_t capacity)
{
    char key[64]; snprintf(key, sizeof key, " %s=\"", name);
    const char *p = strstr(line, key); if (!p) return false;
    p += strlen(key); const char *end = strchr(p, '"');
    if (!end || (size_t)(end-p) >= capacity) return false;
    memcpy(out, p, (size_t)(end-p)); out[end-p] = 0; return true;
}
static void input_map_device(int device)
{
    if (SDL_IsGameController(device)) return;
    FILE *fp = fopen(es_input, "r");
    char guid[33]; SDL_JoystickGetGUIDString(SDL_JoystickGetDeviceGUID(device), guid, sizeof guid);
    const char *name = SDL_JoystickNameForIndex(device);
    if (!fp) {
        fprintf(stderr, "[INPUT] unmapped %s GUID=%s; configure --es-input FILE or --controller-db FILE\n", name ? name : "unknown", guid);
        return;
    }
    char line[2048], mapping[2048] = ""; bool match = false;
    while (fgets(line, sizeof line, fp)) {
        if (strstr(line, "<inputConfig ")) {
            char id[64] = "", device_name[256] = "";
            input_attr(line, "deviceGUID", id, sizeof id);
            input_attr(line, "deviceName", device_name, sizeof device_name);
            match = !strcmp(id, guid) || (name && !strcmp(name, device_name));
            /* SDL mapping names may not contain commas; use a fixed label. */
            if (match) snprintf(mapping, sizeof mapping, "%s,Knulli saved controller,", guid);
        } else if (match && strstr(line, "</inputConfig>")) {
            if (SDL_GameControllerAddMapping(mapping) < 0)
                fprintf(stderr, "[INPUT] mapping rejected: %s\n", SDL_GetError());
            else fprintf(stderr, "[INPUT] mapping from %s: %s\n", es_input, mapping);
            break;
        } else if (match && strstr(line, "<input ")) {
            char action[64], type[32], id[16], value[16];
            if (!input_attr(line,"name",action,sizeof action) || !input_attr(line,"type",type,sizeof type) ||
                !input_attr(line,"id",id,sizeof id) || !input_attr(line,"value",value,sizeof value)) continue;
            const char *sdl = NULL;
            const char *pairs[][2] = {{"a","b"},{"b","a"},{"x","y"},{"y","x"},
                {"start","start"},{"select","back"},{"pageup","leftshoulder"},
                {"pagedown","rightshoulder"},{"left","dpleft"},{"right","dpright"},
                {"up","dpup"},{"down","dpdown"},{"joystick1left","leftx"}};
            for (size_t i=0;i<sizeof pairs/sizeof *pairs;i++) if (!strcmp(action,pairs[i][0])) sdl=pairs[i][1];
            if (!sdl) continue;
            char binding[96]; int index=atoi(id), direction=atoi(value);
            if (index<0 || index>255) continue;
            if (!strcmp(type,"button")) snprintf(binding,sizeof binding,"%s:b%d,",sdl,index);
            else if (!strcmp(type,"hat")) snprintf(binding,sizeof binding,"%s:h%d.%d,",sdl,index,direction);
            else if (!strcmp(type,"axis") && !strcmp(sdl,"leftx"))
                snprintf(binding,sizeof binding,"leftx:a%d%s,",index,direction>0?"~":"");
            else if (!strcmp(type,"axis")) snprintf(binding,sizeof binding,"%s:%ca%d,",sdl,direction<0?'-':'+',index);
            else continue;
            if (strlen(mapping)+strlen(binding)<sizeof mapping) strcat(mapping,binding);
        }
    }
    fclose(fp);
}
static bool input_button(SDL_GameController *pad, SDL_GameControllerButton button)
{ return SDL_GameControllerGetButton(pad,button)!=0; }
static bool input_poll(void)
{
    const Uint8 *keys=SDL_GetKeyboardState(NULL);
    input_coin=keys[SDL_SCANCODE_5]; input_gas=keys[SDL_SCANCODE_X] || keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_RETURN];
    input_brake=keys[SDL_SCANCODE_Z] || keys[SDL_SCANCODE_DOWN]; input_view=keys[SDL_SCANCODE_C];
    input_up=keys[SDL_SCANCODE_S]; input_down=keys[SDL_SCANCODE_A];
    int direction=keys[SDL_SCANCODE_RIGHT]-keys[SDL_SCANCODE_LEFT]; input_wheel=0;
    for (int i=0;i<16;i++) if (controllers[i]) {
        SDL_GameController *pad=controllers[i];
        bool start=input_button(pad,SDL_CONTROLLER_BUTTON_START), select=input_button(pad,SDL_CONTROLLER_BUTTON_BACK);
        if (start && select) return false; /* do not inject either action */
        input_coin |= select;
        input_gas |= start || input_button(pad,SDL_CONTROLLER_BUTTON_A) || input_button(pad,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
        input_brake |= input_button(pad,SDL_CONTROLLER_BUTTON_B) || input_button(pad,SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        input_view |= input_button(pad,SDL_CONTROLLER_BUTTON_X);
        input_up |= input_button(pad,SDL_CONTROLLER_BUTTON_DPAD_UP);
        input_down |= input_button(pad,SDL_CONTROLLER_BUTTON_DPAD_DOWN);
        direction += input_button(pad,SDL_CONTROLLER_BUTTON_DPAD_RIGHT)-input_button(pad,SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        int axis=SDL_GameControllerGetAxis(pad,SDL_CONTROLLER_AXIS_LEFTX);
        if (axis < -32767) axis=-32767;
        const int deadzone=4096;
        axis=abs(axis)<=deadzone?0:(axis>0?axis-deadzone:axis+deadzone)*32767/(32767-deadzone);
        if (abs(axis)>abs(input_wheel)) input_wheel=axis;
    }
    if (direction) input_wheel=direction>0?32767:-32767;
    return true;
}
static void input_bit(uint16_t bit, bool pressed)
{ if (pressed) g_hw.inputs &= (uint16_t)~bit; else g_hw.inputs |= bit; }
static void input_ramp(uint16_t *value, int target, int delta)
{ int v=*value; if(v<target) { v+=delta; if(v>target)v=target; } else if(v>target) {v-=delta;if(v<target)v=target;} *value=(uint16_t)v; }
void rr_gles3_apply_input(void)
{g2_save_tick();
    if (!interactive) return;
    bool coin=input_coin;
    if (coin_edges && (g_hw.inputs & 0x1000)) { coin=true; coin_edges--; }
    input_bit(0x1000,coin); input_bit(0x0040,input_view);
    input_bit(0x0001,input_down); input_bit(0x0002,input_up);
    g_hw.steer=(uint16_t)(0x800+input_wheel*((g_rr_game->steer_max-g_rr_game->steer_min)/2)/32767);
    bool gas=input_gas || start_ticks;
    if(start_ticks)start_ticks--;
    input_ramp(&g_hw.gas,gas?g_rr_game->gas_max:0,g_rr_game->gas_delta);
    input_ramp(&g_hw.brake,input_brake?g_rr_game->brake_max:0,g_rr_game->brake_delta);
}
bool rr_gles3_interactive(void) { return interactive; }


static bool failed,audio_enabled,deferred_record_exit;
static const char*thread_csv;
static uint32_t profile_from=1;
static double last_presentation;
static rr_presentation_metrics last_metrics;
static double seconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
double rr_gles3_last_presentation(void){return last_presentation;}
rr_presentation_metrics rr_gles3_last_metrics(void){return last_metrics;}
uint32_t rr_gles3_profile_from(void){return profile_from;}
bool rr_gles3_failed(void){return failed;}
bool rr_gles3_record_exit_pending(void){return deferred_record_exit;}
double rr_gles3_stage_begin(void){return 0;}
void rr_gles3_stage_end(rr_gles3_stage stage,double start){(void)stage;(void)start;}
void rr_gles3_scene_tick(bool walk,unsigned direct){(void)walk;(void)direct;}

static void scan_controllers(void)
{
    for (int i = 0; i < 16; i++) {
        if (raw_joysticks[i] && !SDL_JoystickGetAttached(raw_joysticks[i])) {
            SDL_JoystickClose(raw_joysticks[i]); raw_joysticks[i] = NULL;
        }
        if (controllers[i] && !SDL_GameControllerGetAttached(controllers[i])) {
            SDL_GameControllerClose(controllers[i]); controllers[i] = NULL;
            chord_start[i]=chord_select[i]=false;
        }
    }
    for (int device = 0; device < SDL_NumJoysticks(); device++) {
        input_map_device(device);
        if (!SDL_IsGameController(device)) {
            fprintf(stderr, "[GLES2] unmapped joystick: %s (START+SELECT needs an SDL mapping)\n", SDL_JoystickNameForIndex(device));
            SDL_JoystickID raw_id = SDL_JoystickGetDeviceInstanceID(device);
            int slot = -1; bool present = false;
            for (int i=0;i<16;i++) {
                if (!raw_joysticks[i]) slot=i;
                else if (SDL_JoystickInstanceID(raw_joysticks[i])==raw_id) present=true;
            }
            if (!present && slot>=0) raw_joysticks[slot]=SDL_JoystickOpen(device);
            continue;
        }
        SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(device);
        int slot = -1; bool present = false;
        for (int i = 0; i < 16; i++) {
            if (!controllers[i]) slot = i;
            else if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(controllers[i])) == id) present = true;
        }
        if (!present && slot >= 0) {
            controllers[slot] = SDL_GameControllerOpen(device);
            if (controllers[slot]) {
                SDL_Joystick *js = SDL_GameControllerGetJoystick(controllers[slot]);
                char guid[33]; SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(js),guid,sizeof guid);
                fprintf(stderr,"[INPUT] controller=%s GUID=%s instance=%d axes=%d buttons=%d hats=%d\n",
                    SDL_GameControllerName(controllers[slot]),guid,SDL_JoystickInstanceID(js),
                    SDL_JoystickNumAxes(js),SDL_JoystickNumButtons(js),SDL_JoystickNumHats(js));
            }
            else fprintf(stderr, "[GLES2] controller open: %s\n", SDL_GetError());
        }
    }
}static bool input_exit(void)
{g2_save_emit();
    if(rr_input_recording()){deferred_record_exit=true;return true;}
    return false;
}static bool pump_input(void)
{
    if(deferred_record_exit)return true;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (!input_event(&event)) return input_exit();
        if ((event.type==SDL_KEYDOWN && !event.key.repeat && event.key.keysym.scancode==SDL_SCANCODE_F3) ||
            (event.type==SDL_CONTROLLERBUTTONDOWN && event.cbutton.button==SDL_CONTROLLER_BUTTON_Y))rr_input_record_marker();
        if (input_log && (event.type == SDL_JOYBUTTONDOWN || event.type == SDL_JOYBUTTONUP))
            fprintf(stderr,"[INPUT] joystick=%d button=%u pressed=%u\n",event.jbutton.which,event.jbutton.button,event.jbutton.state);
        if (input_log && event.type == SDL_JOYAXISMOTION && abs(event.jaxis.value)>8192)
            fprintf(stderr,"[INPUT] joystick=%d axis=%u value=%d\n",event.jaxis.which,event.jaxis.axis,event.jaxis.value);
        if (input_log && event.type == SDL_JOYHATMOTION)
            fprintf(stderr,"[INPUT] joystick=%d hat=%u value=%u\n",event.jhat.which,event.jhat.hat,event.jhat.value);
        if (event.type == SDL_QUIT || (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) return input_exit();
        if (event.type == SDL_CONTROLLERDEVICEADDED || event.type == SDL_CONTROLLERDEVICEREMOVED ||
            event.type == SDL_JOYDEVICEADDED || event.type == SDL_JOYDEVICEREMOVED) scan_controllers();
    }
    if (interactive && !input_poll()) return input_exit();
    for (int i = 0; i < 16; i++) if (controllers[i] &&
        SDL_GameControllerGetButton(controllers[i], SDL_CONTROLLER_BUTTON_START) &&
        SDL_GameControllerGetButton(controllers[i], SDL_CONTROLLER_BUTTON_BACK)) return input_exit();
    return true;
}
bool rr_gles3_pump(void){return pump_input();}
bool rr_gles3_options(int*argc,char**argv){
 if(!rr_sim_options(argc,argv))return false;
 int at=2;for(int i=2;i<*argc;i++){
  const char*p=argv[i];
  if(!strcmp(p,"--interactive"))interactive=true;
  else if(!strcmp(p,"--audio")&&i+1<*argc){p=argv[++i];if(strcmp(p,"on")&&strcmp(p,"off"))return false;audio_enabled=!strcmp(p,"on");}
  else if(!strcmp(p,"--thread-profile")&&i+1<*argc)thread_csv=argv[++i];
  else if(!strcmp(p,"--es-input")&&i+1<*argc)es_input=argv[++i];
  else if(!strcmp(p,"--controller-db")&&i+1<*argc)controller_db=argv[++i];
  else if(!strcmp(p,"--overlay")&&i+1<*argc){if(!rr_overlay_mode(argv[++i]))return false;}
  else if(!strcmp(p,"--input-log"))input_log=true;
  else if(!strcmp(p,"--presentation-thread")&&i+1<*argc){if(strcmp(argv[++i],"on"))return false;}
  else if(!strcmp(p,"--timing")&&i+1<*argc){if(strcmp(argv[++i],"independent"))return false;}
  else argv[at++]=argv[i];
 }*argc=at;argv[at]=NULL;rr_presentation_configure(true,false,thread_csv,false);return true;
}
bool rr_gles3_open(void){
 if(!g2_save_open(getenv("ENG_GLES3_SESSION")&&interactive))return false;
 bool mali=false;for(int i=0;i<SDL_GetNumVideoDrivers();i++)if(!strcmp(SDL_GetVideoDriver(i),"mali"))mali=true;
 if(!getenv("SDL_VIDEODRIVER")&&mali)SDL_setenv("SDL_VIDEODRIVER","mali",0);
 if(SDL_Init(SDL_INIT_VIDEO|SDL_INIT_GAMECONTROLLER))return false;
 SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_ES);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,0);SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER,1);SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE,16);
 SDL_GL_SetAttribute(SDL_GL_RED_SIZE,8);SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE,8);SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE,8);
 window=SDL_CreateWindow("Rave Racer GLES3",0,0,640,480,SDL_WINDOW_OPENGL|SDL_WINDOW_FULLSCREEN);if(!window)return false;atexit(rr_gles3_close);
 context=SDL_GL_CreateContext(window);if(!context)return false;
 fprintf(stderr,"[GLES3-GL] %s; %s\n",glGetString(GL_RENDERER),glGetString(GL_VERSION));
 if(SDL_GL_SetSwapInterval(1)&&!getenv("ENG_GLES3_HOST"))return false;
 if(!rr_video_layers_open()||!rr_p1_open())return false;
 if(controller_db&&SDL_GameControllerAddMappingsFromFile(controller_db)<0)return false;
 if(audio_enabled&&!rr_audio_output_open())return false;scan_controllers();
 if(!rr_presentation_start(window,context,0,0,0,1))return false;return true;
}
bool rr_gles3_present_prepared(const rr_presentation*s){
 eng_render_stats stats;double start=seconds();
 if(!rr_p1_draw(s,&stats)){failed=true;return false;}
#ifdef GLES3_VALIDATION
 extern void gles3_check_boot(const rr_presentation*);gles3_check_boot(s);
#endif
 rr_overlay_draw();double begin=seconds();SDL_GL_SwapWindow(window);last_presentation=seconds();rr_overlay_present(s->frame,last_presentation);
 last_metrics=(rr_presentation_metrics){.draw_wall=begin-start,.submission_wall=begin-start,.swap_wall=last_presentation-begin,.draws=stats.draws,.triangles=stats.triangles};return true;
}
void rr_gles3_close(void){
 if(!window)return;
 if(!rr_presentation_finish())failed=true;
 rr_sim_report();rr_memory_profile_report();rr_p1_close();rr_overlay_close();rr_video_layers_close();
 for(int i=0;i<16;i++){if(controllers[i])SDL_GameControllerClose(controllers[i]);if(raw_joysticks[i])SDL_JoystickClose(raw_joysticks[i]);}
 if(context){SDL_GL_DeleteContext(context);context=NULL;}SDL_DestroyWindow(window);window=NULL;SDL_Quit();
}
