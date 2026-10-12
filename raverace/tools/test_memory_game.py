#!/usr/bin/env python3
"""Real-ROM read-fast-path A/B, including deterministic presented-frame history."""
import json,pathlib,re,subprocess,sys
base,fast,headless,roms,out=(str(pathlib.Path(p).resolve()) for p in sys.argv[1:6])
root=pathlib.Path(out);root.mkdir(parents=True,exist_ok=True)
for frame in (600,1700):
 script=['--frames',str(frame),'--coin','180','--gas','200','--steer','300:2304','--steer','700:2048']
 ref=subprocess.run([headless,roms,*script],capture_output=True,text=True,check=True)
 images=[];digests=[]
 for name,binary in [('baseline',base),('fast',fast)]:
  image=root/f'{frame}-{name}.ppm'
  args=[binary,roms,*script,'--timing','independent','--test-present-every','3','--skip-geometry-on-dropped-frames','on','--texture-budget-mib','64','--capture-frame',str(frame),'--capture',str(image)]
  r=subprocess.run(args,capture_output=True,text=True,timeout=180)
  (root/f'{frame}-{name}.log').write_text(r.stderr);(root/f'{frame}-{name}.json').write_text(r.stdout)
  assert r.returncode==0,r.stderr
  state=json.loads(r.stdout);assert state==json.loads(ref.stdout)
  assert state['traps']==state['unmapped']==state['romwrites']==0 and not state['dsp_fault'] and not state['sound_fault']
  assert f'[MEM-READS] fast={"off" if name=="baseline" else "on"} counters=off' in r.stderr
  images.append(image.read_bytes())
  digests.append((next(l.split(' ticks=')[1] for l in r.stderr.splitlines() if l.startswith('[GEOMETRY]')),next(l for l in r.stderr.splitlines() if l.startswith('[BOARD-STATE]'))))
 assert images[0]==images[1],frame
 assert digests[0]==digests[1],frame
 print(frame,'baseline/fast: headless state, pixels, video digest identical',flush=True)
print('Real game memory-read A/B passed')
