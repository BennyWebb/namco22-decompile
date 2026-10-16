#!/usr/bin/env python3
"""Assemble separate Native app; deliberately includes no compiled asset cache."""
import pathlib,shutil,subprocess,json,hashlib,tarfile
root=pathlib.Path(__file__).resolve().parents[3];src=pathlib.Path(__file__).parent;b=root/'raverace/native/generated/g2';out=b/'app';out.mkdir(exist_ok=True)
cc=json.loads((b/'b2-play/arm/build.json').read_text())['commands'][0][0]
sysroot=subprocess.check_output([cc,'-print-sysroot'],text=True).strip()
commands=[]
for source,dest,extra in [('asset_builder.c','asset_builder-arm',[]),('session_io.c','session-io.so',['-shared','-fPIC']),('smoke_chord.c','smoke-chord.so',['-shared','-fPIC','-I'+sysroot+'/usr/include/SDL2','-ldl'])]:
 cmd=[cc,'-O2',*extra,str(src/source),'-o',str(b/dest)];subprocess.run(cmd,check=True);commands.append(cmd)
nm=str(pathlib.Path(cc).with_name('aarch64-buildroot-linux-gnu-nm'))
assert ' T SDL_PollEvent' in subprocess.check_output([nm,'-D',str(b/'smoke-chord.so')],text=True)
assert ' T open' in subprocess.check_output([nm,'-D',str(b/'session-io.so')],text=True)
(b/'package-build.json').write_text(json.dumps(commands,indent=2)+'\n')
for n in ['session.py','clocks.py','assets.py','telemetry.py','remove.sh']:shutil.copyfile(src/n,out/n)
for a,n in [('b2-play/arm/rr_p1b','rr_native'),('asset_builder-arm','asset_builder'),('smoke-chord.so','smoke-chord.so'),('session-io.so','session-io.so')]:shutil.copyfile(b/a,out/n)
for n in ['rr_native','asset_builder','remove.sh']:(out/n).chmod(0o755)
shutil.copytree(b/'asset-kit',out/'asset-kit',dirs_exist_ok=True)
shaders=root/'raverace/native/generated/stage1b/hardware-shaders'
assert shaders.exists(),shaders
shutil.copytree(shaders,out/'shaders',dirs_exist_ok=True)
with tarfile.open(b/'app.tar.gz','w:gz') as t:
 for p in sorted(out.rglob('*')):
  if p.is_file():t.add(p,arcname=str(p.relative_to(out)))
manifest={str(p.relative_to(out)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(out.rglob('*')) if p.is_file()};(b/'app-hashes.json').write_text(json.dumps(manifest,indent=2)+'\n');print(out)
