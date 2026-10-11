/* Small SDL input bridge. No desktop UI, audio or simulation timing changes. */
#include "rr_hw.h"
#include "rr_game.h"
static bool interactive, input_log;
static const char *controller_db, *es_input = "/userdata/system/configs/emulationstation/es_input.cfg";
static bool input_coin, input_gas, input_brake, input_view, input_up, input_down;
static int input_wheel;

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
void rr_gles2_apply_input(void)
{
    if (!interactive) return;
    input_bit(0x1000,input_coin); input_bit(0x0040,input_view);
    input_bit(0x0001,input_down); input_bit(0x0002,input_up);
    g_hw.steer=(uint16_t)(0x800+input_wheel*((g_rr_game->steer_max-g_rr_game->steer_min)/2)/32767);
    input_ramp(&g_hw.gas,input_gas?g_rr_game->gas_max:0,g_rr_game->gas_delta);
    input_ramp(&g_hw.brake,input_brake?g_rr_game->brake_max:0,g_rr_game->brake_delta);
}
bool rr_gles2_interactive(void) { return interactive; }
