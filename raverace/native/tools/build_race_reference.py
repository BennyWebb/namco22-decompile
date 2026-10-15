#!/usr/bin/env python3
"""Private original HUD compositor relink: capture every reference race picture."""
import argparse,pathlib,shlex,json,subprocess
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);a=p.parse_args();root=pathlib.Path(__file__).resolve().parents[3];build=a.build.resolve();out=a.out.resolve();out.mkdir(exist_ok=False,parents=True)
s=(root/'raverace/src/rr_gpu_hud.inc').read_text();start=s.index('  if(dir&&(selected||differences)){');end=s.index('  if(differences){fprintf',start);s=s[:start]+'''  if(dir&&selected&&g_eng_frame>=1800&&g_eng_frame<=5639){
   if(!write_case(dir,"reference.rgba",reference_pixels,W*H*4))return false;
  }
'''+s[end:];(out/'rr_gpu_hud.inc').write_text(s)
s=(root/'raverace/src/rr_video_layers.c').read_text();(out/'layers.c').write_text(s)
f=(build/'CMakeFiles/rr_gles2.dir/flags.make').read_text();get=lambda n:shlex.split(f.split(n+' = ',1)[1].split('\n',1)[0]);link=shlex.split((build/'CMakeFiles/rr_gles2.dir/link.txt').read_text());subprocess.run([link[0],*get('C_DEFINES'),*get('C_INCLUDES'),*get('C_FLAGS'),'-I'+str(root/'raverace/src'),'-c',str(out/'layers.c'),'-o',str(out/'layers.o')],check=True)
s=(root/'raverace/src/rr_presentation.c').read_text();needle='bool selected = rr_clock_render(&clock_state, now(), limit_hz, frame == capture);';assert s.count(needle)==1;s=s.replace(needle,needle+' selected=true; /* diagnostic every-picture reference */');(out/'presentation.c').write_text(s)
subprocess.run([link[0],*get('C_DEFINES'),*get('C_INCLUDES'),*get('C_FLAGS'),'-I'+str(root/'raverace/src'),'-c',str(out/'presentation.c'),'-o',str(out/'presentation.o')],check=True)
cmd=[str(out/'presentation.o') if x.endswith('/src/rr_presentation.c.o') else str(out/'layers.o') if x.endswith('/src/rr_video_layers.c.o') else str(out/'reference_sequence') if x=='rr_gles2' else x for x in link if not x.startswith('-Wl,--dependency-file=')];subprocess.run(cmd,cwd=build,check=True);print(out/'reference_sequence')
