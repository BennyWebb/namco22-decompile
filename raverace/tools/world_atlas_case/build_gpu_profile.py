"""Private GPU-span build; reuses the existing nonblocking query infrastructure.
No production source, executable or build object is modified.
"""
from pathlib import Path
import shlex
import subprocess
root=Path(__file__).resolve().parents[3]
out=Path('/tmp/rr-atlas/gpu-profile');out.mkdir(parents=True,exist_ok=True)
build=Path('/tmp/rr-world-array-arm-build')
s=(root/'engine/gles_multi_gpu_trace.inc').read_text().replace('RR_GLES2_MULTITEXTURE_BATCH','RR_GLES_WORLD_ATLAS').replace('MULTI-GPU','WORLD-GPU')
(out/'gles_multi_gpu_trace.inc').write_text(s)
s=(root/'engine/quad_gles2.c').read_text()
s=s.replace('bool eng_gles2_multi_gpu_trace(const char *path)\n{\n#ifdef RR_GLES2_MULTITEXTURE_BATCH','bool eng_gles2_multi_gpu_trace(const char *path)\n{\n#ifdef RR_GLES_WORLD_ATLAS',1)
s=s.replace('    if(!world_open())return false;','    if(!world_open() || !multi_gpu_open())return false;',1)
s=s.replace('    world_begin(bg,count);','    world_begin(bg,count);',1)
s=s.replace('    used = batch_first = command_count = 0; batch_texture = white; current_stats = stats; flush_failed = false;','    used = batch_first = command_count = 0; batch_texture = white; current_stats = stats; flush_failed = false;\n    multi_gpu_begin(g_eng_frame);',1)
s=s.replace('    if(!world_end())return false;','    multi_gpu_end();\n    if(!world_end())return false;',1)
s=s.replace('    world_close();','    world_close();multi_gpu_close();',1)
(out/'quad_profile.c').write_text(s)
flags=(build/'CMakeFiles/rr_gles2.dir/flags.make').read_text()
def get(name):return shlex.split(flags.split(name+' = ',1)[1].split('\n',1)[0])
link=shlex.split((build/'CMakeFiles/rr_gles2.dir/link.txt').read_text());cc=link[0]
sysroot=next(x for x in link if x.startswith('--sysroot='))
subprocess.run([cc,sysroot,*get('C_DEFINES'),*get('C_INCLUDES'),*get('C_FLAGS'),'-DRR_GEOMETRY_INDEX_ORDER','-DRR_ORDERED_SUBMISSION','-DRR_GLES_WORLD_ATLAS','-c',str(out/'quad_profile.c'),'-o',str(out/'quad_profile.o')],check=True)
cmd=[]
for arg in link:
 if arg.startswith('-Wl,--dependency-file='):continue
 if arg.endswith('/engine/quad_gles2.c.o'):arg=str(out/'quad_profile.o')
 if arg=='rr_gles2':arg=str(out/'rr_world_profile')
 cmd.append(arg)
subprocess.run(cmd,cwd=build,check=True)
subprocess.run([str(root.parent/'knulli-scarab/output/h700/host/bin/aarch64-buildroot-linux-gnu-strip'),'--strip-debug',str(out/'rr_world_profile')],check=True)
