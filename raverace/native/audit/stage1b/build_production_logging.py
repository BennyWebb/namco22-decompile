#!/usr/bin/env python3
"""Relink diagnostic production copy with existing opt-in shutdown CSV exporter.
No generated-code flag or simulation-loop change. Used AFTER unchanged B runs.
"""
import pathlib,subprocess,shlex,json,hashlib,re
root=pathlib.Path(__file__).resolve().parents[4];build=root/'raverace/build/playable-knulli';out=root/'raverace/native/generated/stage1b/production-logging';out.mkdir(parents=True,exist_ok=True)
link=shlex.split((build/'CMakeFiles/rr_gles2.dir/link.txt').read_text());old=build/'CMakeFiles/rr_gles2.dir/src/rr_sim_profile.c.o';new=root/'raverace/native/generated/milestone4a-hardware/telemetry-arm-v2/sim.o';exe=out/'rr_production_logging';cmd=[]
for arg in link:
 if arg.startswith('-Wl,--dependency-file='):continue
 if arg.endswith('/rr_sim_profile.c.o'):arg=str(new)
 if arg=='rr_gles2':arg=str(exe)
 cmd.append(arg)
cc=pathlib.Path(link[0]);dump=cc.with_name('aarch64-buildroot-linux-gnu-objdump');proof={}
for name in ['rr_sim_pause','rr_sim_resume','rr_sim_enter','rr_sim_leave','rr_sim_present_tick']:
 values=[]
 for obj in [old,new]:
  s=subprocess.check_output([str(dump),'-d','--disassemble='+name,str(obj)],text=True)
  words=re.findall(r'^\s*[0-9a-f]+:\s+([0-9a-f]{8})\s',s,re.M);assert words,name;values.append(hashlib.sha256(''.join(words).encode()).hexdigest())
 proof[name]={'old':values[0],'csv':values[1],'identical':values[0]==values[1]}
assert all(v['identical'] for v in proof.values()),proof
subprocess.run(cmd,cwd=build,check=True)
strip=cc.with_name('aarch64-buildroot-linux-gnu-strip');subprocess.run([str(strip),'-o',str(exe)+'.stripped',str(exe)],check=True)
(out/'build.json').write_text(json.dumps(dict(command=cmd,hot_function_opcode_hashes=proof,exe_sha256=hashlib.sha256(exe.read_bytes()).hexdigest(),note='Existing Stage1 private CSV exporter is default OFF; only profiler report differs, after replay timing ends.'),indent=2)+'\n')
print(exe)
