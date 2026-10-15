#!/usr/bin/env python3
"""Private unchanged-HUD instrumentation and explicit shadow/LUT omission controls."""
import argparse,pathlib
p=argparse.ArgumentParser();p.add_argument('out',type=pathlib.Path);a=p.parse_args();a.out.mkdir(exist_ok=False);root=pathlib.Path(__file__).resolve().parents[2]
s=(root/'native/render/race_v3.c').read_text();layer=(root/'src/rr_video_layers.c').read_text();hud=(root/'src/rr_gpu_hud.inc').read_text();geo=(root/'src/rr_gpu_hud_geometry.inc').read_text()
header='''static int hud_control3;static int hud_fences3;static double hud_pen_wall3,hud_pen_cpu3;
static double hud_stamp3(clockid_t k){struct timespec t;clock_gettime(k,&t);return t.tv_sec+t.tv_nsec/1e9;}
'''
s=s.replace('#include "../../src/rr_video_layers.c"',header+'#include "'+str((a.out/'layers.c').resolve())+'"')
s=s.replace('CHECK3(argc==7);','hud_control3=getenv("ENG_NATIVE_HUD_CONTROL")?atoi(getenv("ENG_NATIVE_HUD_CONTROL")):0;hud_fences3=getenv("ENG_NATIVE_HUD_FENCES")!=NULL;CHECK3(hud_control3>=0&&hud_control3<=2);CHECK3(argc==7);')
s=s.replace('if(capture)save3(out,h->tick,"composed");','printf("HUDPROFILE tick=%u control=%d pen_wall_ms=%.6f pen_cpu_ms=%.6f shadow_enabled=%u\\n",h->tick,hud_control3,hud_pen_wall3,hud_pen_cpu3,hs[3]);\n if(capture)save3(out,h->tick,"composed");')
geo=geo.replace(' bool rebuild=', ' double hp_start=hud_stamp3(CLOCK_MONOTONIC),hp_cpu=hud_stamp3(CLOCK_THREAD_CPUTIME_ID);\n bool rebuild=',1)
geo=geo.replace(' return error("glyph HUD update");',' if(hud_fences3){glFinish();}hud_pen_wall3=(hud_stamp3(CLOCK_MONOTONIC)-hp_start)*1000;hud_pen_cpu3=(hud_stamp3(CLOCK_THREAD_CPUTIME_ID)-hp_cpu)*1000;\n return error("glyph HUD update");')
start=geo.index('static const char *glyph_final_fs=');end=geo.index('static void gpu_account',start);block=geo[start:end];cheap=block.replace('glyph_final_fs','glyph_final_no_lut_fs')
line=next(l for l in cheap.splitlines() if 'vec3 o;o.r=' in l);cheap=cheap.replace(line,' "gl_FragColor=vec4(c/255.,1.); }\\n";')
geo=geo[:end]+cheap+geo[end:]
hud=hud.replace('if(gpu_geometry())fs=glyph_final_fs;','if(gpu_geometry())fs=hud_control3==2?glyph_final_no_lut_fs:glyph_final_fs;')
hud=hud.replace('glUniform1i(gpu_shadow,frame->shadow_enabled);','glUniform1i(gpu_shadow,hud_control3==1?0:frame->shadow_enabled);')
for name,data in [('probe.c',s),('layers.c',layer),('rr_gpu_hud.inc',hud),('rr_gpu_hud_geometry.inc',geo)]: (a.out/name).write_text(data)
