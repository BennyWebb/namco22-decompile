#!/usr/bin/env python3
"""Real-ROM read A/B (+ optional split), including deterministic presentation history."""
import json,pathlib,re,subprocess,sys
base,fast,headless,roms,out=(str(pathlib.Path(p).resolve()) for p in sys.argv[1:6])
root=pathlib.Path(out);root.mkdir(parents=True,exist_ok=True)
for frame in (600,1700):
 script=['--frames',str(frame),'--coin','180','--gas','200','--steer','300:2304','--steer','700:2048']
 ref=subprocess.run([headless,roms,*script],capture_output=True,text=True,check=True)
 images=[];digests=[]
 variants=[('baseline',base),('fast',fast)]
 if len(sys.argv)>6: variants.append(('split',str(pathlib.Path(sys.argv[6]).resolve())))
 for name,binary in variants:
  image=root/f'{frame}-{name}.ppm'
  args=[binary,roms,*script,'--timing','independent','--test-present-every','3','--skip-geometry-on-dropped-frames','on','--texture-budget-mib','64','--capture-frame',str(frame),'--capture',str(image)]
  r=subprocess.run(args,capture_output=True,text=True,timeout=180)
  (root/f'{frame}-{name}.log').write_text(r.stderr);(root/f'{frame}-{name}.json').write_text(r.stdout)
  assert r.returncode==0,r.stderr
  state=json.loads(r.stdout);assert state==json.loads(ref.stdout)
  assert state['traps']==state['unmapped']==state['romwrites']==0 and not state['dsp_fault'] and not state['sound_fault']
  if name=='split': assert '[MEM-READER] implementation=split' in r.stderr
  assert f'[MEM-READS] fast={"off" if name=="baseline" else "on"} counters=off' in r.stderr
  images.append(image.read_bytes())
  digests.append((next(l.split(' ticks=')[1] for l in r.stderr.splitlines() if l.startswith('[GEOMETRY]')),next(l for l in r.stderr.splitlines() if l.startswith('[BOARD-STATE]'))))
 assert all(image==images[0] for image in images),frame
 assert all(digest==digests[0] for digest in digests),frame
 print(frame,'/'.join(name for name,_ in variants)+': headless state, pixels, video/board digest identical',flush=True)
print('Real game memory-read A/B passed')
