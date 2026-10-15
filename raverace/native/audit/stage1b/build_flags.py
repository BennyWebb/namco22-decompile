#!/usr/bin/env python3
"""Private Stage1b relink: generated sources only; no algorithm changes.
Production diagnostics can additionally reuse the existing default-OFF shutdown
CSV exporter; its simulation hot-path object code is unchanged by that exporter.
"""
import argparse,pathlib,shlex,subprocess,time,json,hashlib,os
p=argparse.ArgumentParser();p.add_argument('build',type=pathlib.Path);p.add_argument('target',choices=['arm','host']);p.add_argument('variant',choices=['O1','O2','O2-nosp']);p.add_argument('--production',action='store_true');a=p.parse_args()
root=pathlib.Path(__file__).resolve().parents[4];build=a.build.resolve();out=root/'raverace/native/generated/stage1b'/a.target/a.variant;out.mkdir(parents=True,exist_ok=True)
f=(build/'CMakeFiles/rr_gles2.dir/flags.make').read_text();get=lambda n:shlex.split(f.split(n+' = ',1)[1].split('\n',1)[0]);link=shlex.split((build/'CMakeFiles/rr_gles2.dir/link.txt').read_text());cc=link[0];options=['-O1' if a.variant=='O1' else '-O2','-w'];options+=['-fno-stack-protector'] if a.variant=='O2-nosp' else []
commands=[];timings={};started=time.monotonic();buildenv={**os.environ,"CCACHE_DISABLE":"1"}
for name in ['rr_lifted','snd_driver']:
 cmd=[cc,*get('C_DEFINES'),*get('C_INCLUDES'),*get('C_FLAGS'),*options,'-c',str(root/f'raverace/gen/{name}.c'),'-o',str(out/(name+'.o'))];commands.append(cmd);t=time.monotonic();subprocess.run(cmd,check=True,env=buildenv);timings[name]=time.monotonic()-t
proto=root/f'raverace/native/generated/milestone4a/coverage-{a.target}';stage1=root/f'raverace/native/generated/stage1/{a.target}';telemetry=root/'raverace/native/generated/milestone4a-hardware/telemetry-arm-v2' if a.target=='arm' else proto
replacements={f'{n}.c.o':out/(n+'.o') for n in ['rr_lifted','snd_driver']}
if a.production:
 replacements['rr_sim_profile.c.o']=telemetry/'sim.o'
 exe=out/'rr_production_flags'
else:
 replacements.update({'rr_main.c.o':proto/'main.o','rr_gl.c.o':stage1/'gl.o','rr_gles2.c.o':telemetry/'gles.o','rr_video_layers.c.o':proto/'layers.o'})
 if (telemetry/'sim.o').exists():replacements['rr_sim_profile.c.o']=telemetry/'sim.o'
 exe=out/'rr_native_flags'
cmd=[];unchanged={}
for arg in link:
 if arg.startswith('-Wl,--dependency-file='):continue
 if not a.production and (arg.endswith('/engine/geo_hw.c.o') or arg.endswith('/engine/slave_list.c.o')):continue
 original=pathlib.Path(arg)
 if original.name in replacements:arg=str(replacements[original.name])
 if arg=='rr_gles2':arg=str(exe)
 cmd.append(arg)
 if arg.endswith('.o'):
  path=pathlib.Path(arg);path=path if path.is_absolute() else build/path
  unchanged[str(path)]=hashlib.sha256(path.read_bytes()).hexdigest()
if not a.production:cmd += [str(stage1/'live.o'),'-lz'];unchanged[str(stage1/'live.o')]=hashlib.sha256((stage1/'live.o').read_bytes()).hexdigest()
commands.append(cmd);t=time.monotonic();subprocess.run(cmd,cwd=build,check=True,env=buildenv);timings['link']=time.monotonic()-t
summary=dict(target=a.target,variant=a.variant,kind='production' if a.production else 'native',commands=commands,build_seconds=time.monotonic()-started,component_build_seconds=timings,unstripped_binary_bytes=exe.stat().st_size,executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),linked_object_sha256=unchanged,changed_generated_sources=['gen/rr_lifted.c','gen/snd_driver.c'],generated_source_flags=options,ccache_disabled=True,note='Other game/compiler/renderer objects are reused; command records establish the only flag scopes. The default-OFF CSV exporter reports already-collected samples after timing ends.')
if a.target=='arm':
 strip=pathlib.Path(cc).with_name('aarch64-buildroot-linux-gnu-strip');dest=exe.with_name(exe.name+'.stripped');subprocess.run([str(strip),'-o',str(dest),str(exe)],check=True);summary['stripped_binary_bytes']=dest.stat().st_size;summary['stripped_sha256']=hashlib.sha256(dest.read_bytes()).hexdigest()
(out/('production-build.json' if a.production else 'native-build.json')).write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps({k:v for k,v in summary.items() if k not in ['commands','linked_object_sha256']},indent=2))
