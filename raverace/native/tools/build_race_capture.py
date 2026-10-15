#!/usr/bin/env python3
"""Privately relink headless capture; shared simulation sources stay untouched."""
import argparse,json,pathlib,shlex,subprocess
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);p.add_argument('--first',type=int,default=1800);p.add_argument('--last',type=int,default=5639);a=p.parse_args();root=pathlib.Path(__file__).resolve().parents[3];out=a.out.resolve();out.mkdir(parents=True,exist_ok=False);build=a.build.resolve()
f=(build/'CMakeFiles/rr_headless.dir/flags.make').read_text();get=lambda n:shlex.split(f.split(n+' = ',1)[1].split('\n',1)[0]);link=shlex.split((build/'CMakeFiles/rr_headless.dir/link.txt').read_text());cc=link[0]
geo=(root/'engine/geo_hw.c').read_text();assert geo.count('uint32_t uv_raw[8];')==1
geo='static unsigned m3_packet,m3_instance; static int m3_brightness[4];\n'+geo.replace('uint32_t uv_raw[8];','uint32_t uv_raw[8]; m3_packet=addr;')
geo=geo.replace('    q.cmode   =', '    memcpy(m3_brightness,briv,sizeof briv);\n    q.cmode   =',1)
(out/'geometry.c').write_text(geo)
slave=(root/'engine/slave_list.c').read_text().replace('g_bbox_cur = (int)code;', 'g_bbox_cur = (int)code; m3_instance=src;')
(out/'slave.c').write_text(slave)
body=(root/'raverace/native/scene/capture.inc').read_text().replace('"geometry.c"',json.dumps(str(out/'geometry.c'))).replace('"slave_list.c"',json.dumps(str(out/'slave.c')))
assert 1<=a.first<=a.last<=6006
body=body.replace('frame==1800',f'frame=={a.first}').replace('frame<1800||frame>5639',f'frame<{a.first}||frame>{a.last}').replace('frame==5639',f'frame=={a.last}')
(out/'capture.c').write_text(body)
s=(root/'raverace/src/rr_main.c').read_text();needle='    rr_scene_frame(rr_dsp_slave_active());\n    rr_scene_consume();';assert s.count(needle)==1
s='void m3_capture(unsigned frame, int walk);\n'+s.replace(needle,'    m3_capture(frame, rr_scene_frame(rr_dsp_slave_active()));\n    rr_scene_consume();');(out/'main.c').write_text(s)
for name in ['main','capture']:
 subprocess.run([cc,*get('C_DEFINES'),*get('C_INCLUDES'),*get('C_FLAGS'),'-I'+str(root/'engine'),'-I'+str(root/'raverace/src'),'-c',str(out/(name+'.c')),'-o',str(out/(name+'.o'))],check=True)
cmd=[str(out/'main.o') if x.endswith('/src/rr_main.c.o') else str(out/'race_capture') if x=='rr_headless' else x for x in link if not x.startswith('-Wl,--dependency-file=')];cmd.append(str(out/'capture.o'));cmd.append('-lm');subprocess.run(cmd,cwd=build,check=True);print(out/'race_capture')
