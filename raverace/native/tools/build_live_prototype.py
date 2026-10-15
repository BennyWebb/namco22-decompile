#!/usr/bin/env python3
"""Private serial 4A relink. No shared simulation/source or production changes."""
import argparse,pathlib,subprocess,shlex,json,sys
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);p.add_argument('--rebuild',action='store_true');a=p.parse_args();root=pathlib.Path(__file__).resolve().parents[3];out=a.out.resolve();build=a.build.resolve();assert out.is_relative_to(root/'raverace/native/generated/milestone4a');out.mkdir(parents=True,exist_ok=a.rebuild)
# Generate the current selected M3 backend/HUD; experimental bandwidth/LUT flags off.
if not (out/'m3').exists():subprocess.run([sys.executable,str(root/'raverace/native/tools/build_depth_lut_probe.py'),str(out/'m3'),'--linear'],check=True)
s=(out/'m3/probe.c').read_text();s=s[:s.index('int main(int argc,char**argv)')];s=s.replace('#include "'+str(out/'m3/layers.c')+'"','#include "rr_video_layers.h"\n#include "rr_gl.h"')
s=s.replace('h->tick==5639)', 'h->tick==5639||h->tick==5655||h->tick==5867||h->tick==5874||h->tick==5878||h->tick==5889||h->tick==5921||h->tick==6006)')
s=s.replace('unsigned g_eng_frame;','extern unsigned g_eng_frame;').replace('void eng_gles2_priority(bool enabled_){(void)enabled_;}','')
start=s.index(' fonts3=file3(');end=s.index(' instances3=texture3(',start);s=s[:start]+s[end:]
start=s.index(' stream3=open3(');end=s.index('\n}',start);s=s[:start]+' glFinish();'+s[end:]
start=s.index('const uint8_t*style=styles3+h->style');end=s.index('double hud_submit=',start);s=s[:start]+'g_eng_frame=h->tick;CHECK3(rr_gl_video_compose());'+s[end:]
line=next(l for l in s.splitlines() if 'printf("HUDPROFILE' in l);s=s.replace(line,'')
# Original geometry emission with the exact provenance used by validated M3 capture.
geo=(root/'engine/geo_hw.c').read_text().replace('uint32_t uv_raw[8];','uint32_t uv_raw[8]; m3_packet=addr;').replace('    q.cmode   =','    memcpy(m3_brightness,briv,sizeof briv);\n    q.cmode   =',1)
geo='static unsigned m3_packet,m3_instance;static int m3_brightness[4];\n'+geo;(out/'geometry.c').write_text(geo)
slave=(root/'engine/slave_list.c').read_text().replace('g_bbox_cur = (int)code;', 'g_bbox_cur = (int)code; m3_instance=src;');(out/'slave.c').write_text(slave)
cap=(root/'raverace/native/scene/capture.inc').read_text();cap=cap[:cap.index('static void write_bytes')]
for text in ['const int32_t *g_eng_pointrom;','uint32_t g_eng_pointrom_n;','eng_point_ext_fn g_eng_pointram;','unsigned g_eng_frame;']:cap=cap.replace(text,'')
cap=cap.replace('#include "geometry.c"','#include "'+str(out/'geometry.c')+'"').replace('#include "slave_list.c"','#include "'+str(out/'slave.c')+'"')
s+=cap+'\n#include "'+str(root/'raverace/native/live/compiler.inc')+'"\n';(out/'live.c').write_text(s)
# Private selected HUD implementation, same public API and live original frame source.
layer=(out/'m3/layers.c').read_text();prefix='''#define _GNU_SOURCE 1
static int native_fold3=0,hud_control3=0,hud_fences3=0;static double hud_pen_wall3,hud_pen_cpu3;
#include <time.h>
static double hud_stamp3(clockid_t k){struct timespec t;clock_gettime(k,&t);return t.tv_sec+t.tv_nsec/1e9;}
''';(out/'m3/layers.c').write_text(layer if layer.startswith(prefix) else prefix+layer)
# Keep the original SDL controls, scheduler, audio, overlay, checkpoints and swap.
main=(root/'raverace/src/rr_main.c').read_text();main='#include "live_api.h"\n'+main;main=main.replace('    bool geometry;\n','    rr_native_set_frame(frame);\n    bool geometry;\n',1);(out/'main.c').write_text(main)
gl=(root/'raverace/src/rr_gl.c').read_text();gl='#include "live_api.h"\n'+gl;mark='    if (!assets_ok) return;';assert mark in gl;gl=gl.replace(mark,mark+'\n#ifdef RR_GLES2\n    if(rr_native_active()){g_eng_frame++;rr_native_prepare(slave_active,geometry);qn=0;return;}\n#endif',1);(out/'gl.c').write_text(gl)
gles=(root/'raverace/src/rr_gles2.c').read_text();gles='#include "live_api.h"\n'+gles;gles=gles.replace('{SDL_GL_CONTEXT_MAJOR_VERSION, 2}', '{SDL_GL_CONTEXT_MAJOR_VERSION, 3}');gles=gles.replace('static bool thread_mode=true,snapshot_reference;','static bool thread_mode=false,snapshot_reference;');gles=gles.replace('    if (controller_db &&', '    if(!rr_native_open()) {failed=true;return false;}\n    if (controller_db &&',1);gles=gles.replace('    if(thread_mode && !rr_presentation_start', '    if(thread_mode){fprintf(stderr,"[NATIVE-LIVE] serial prototype requires --presentation-thread off\\n");return false;}\n    if(thread_mode && !rr_presentation_start',1)
start=gles.index('    if (!eng_gles2_draw(quads, count, bg, &stats))');end=gles.index('    rr_overlay_draw();',start);original=gles[start:end];new='''    if(rr_native_active()) {
        if(!rr_native_draw(&stats)){failed=true;return false;}
    }else{
'''+original+'''    }
'''
# Variables declared in the original block must remain visible to the common swap/profile tail.
new=new.replace('    double draw_elapsed = seconds() - draw_start;', '    draw_elapsed = seconds() - draw_start;').replace('    double capture_start = seconds();','    capture_start = seconds();')
new='    double draw_elapsed=0,capture_start=seconds();\n'+new+'    if(rr_native_active()){eng_audit_end(audit_backend);gpu_end_frame();draw_elapsed=seconds()-draw_start;capture_start=seconds();}\n';gles=gles[:start]+new+gles[end:]
# Flush native RAM trace only at shutdown, never synchronously each frame.
mark='void rr_gles2_close(void)\n{';assert mark in gles;gles=gles.replace(mark,mark+'\n    rr_native_close();',1);(out/'gles.c').write_text(gles)
f=(build/'CMakeFiles/rr_gles2.dir/flags.make').read_text();get=lambda n:shlex.split(f.split(n+' = ',1)[1].split('\n',1)[0]);link=shlex.split((build/'CMakeFiles/rr_gles2.dir/link.txt').read_text());cc=link[0];incs=['-I'+str(root/'raverace/native/live'),'-I'+str(root/'raverace/src'),'-I'+str(out/'m3')]
for name,source in [('main',out/'main.c'),('gl',out/'gl.c'),('gles',out/'gles.c'),('layers',out/'m3/layers.c'),('live',out/'live.c')]:subprocess.run([cc,*get('C_DEFINES'),*get('C_INCLUDES'),*get('C_FLAGS'),*incs,'-c',str(source),'-o',str(out/(name+'.o'))],check=True)
replace={'/src/rr_main.c.o':'main','/src/rr_gl.c.o':'gl','/src/rr_gles2.c.o':'gles','/src/rr_video_layers.c.o':'layers'};cmd=[]
for arg in link:
 if arg.startswith('-Wl,--dependency-file=') or arg.endswith('/engine/geo_hw.c.o') or arg.endswith('/engine/slave_list.c.o'):continue
 for suffix,name in replace.items():
  if arg.endswith(suffix):arg=str(out/(name+'.o'));break
 if arg=='rr_gles2':arg=str(out/'rr_native_live')
 cmd.append(arg)
cmd+=[str(out/'live.o'),'-lz'];subprocess.run(cmd,cwd=build,check=True);print(out/'rr_native_live')
