#!/usr/bin/env python3
"""Real translated game: optional profiler changes neither state nor pixels."""
import json,pathlib,re,subprocess,sys
binary,headless,roms,out=(str(pathlib.Path(p).resolve()) for p in sys.argv[1:5])
root=pathlib.Path(out);root.mkdir(parents=True,exist_ok=True)
script=['--frames','600','--coin','180','--gas','200','--steer','300:2304']
ref=subprocess.run([headless,roms,*script],capture_output=True,text=True,check=True)
images=[]
for mode,flags in [('off',[]),('stages',['--profile-simulation']),('sampled',['--sample-simulation',str(root/'samples.tsv')])]:
 image=root/(mode+'.ppm')
 r=subprocess.run([binary,roms,*script,'--unpaced','--simulation-profile-from','100','--capture-frame','600','--capture',str(image),*flags],capture_output=True,text=True,timeout=120)
 (root/(mode+'.log')).write_text(r.stderr)
 assert r.returncode==0,r.stderr
 assert json.loads(r.stdout)==json.loads(ref.stdout)
 images.append(image.read_bytes())
 if mode=='off': assert '[SIM-STAGE]' not in r.stderr
 else:
  line=next(l for l in r.stderr.splitlines() if l.startswith('[SIM] '));assert 'ticks=501' in line and 'overflow=0' in line
  assert float(re.search(r'reconciliation_max_us=([\d.]+)',line)[1])<0.01
  rows=[dict(re.findall(r'(\w+)=([^ ]+)',l)) for l in r.stderr.splitlines() if l.startswith('[SIM-STAGE]')]
  means={row['name']:float(row['avg_ms']) for row in rows}
  assert means['translated68k_inclusive_memory']>means['scheduler']>0
  assert abs(sum(v for k,v in means.items() if not k.startswith('simulation_'))-means['simulation_wall'])<0.00002
  for row in rows: assert float(row['median_ms'])<=float(row['p95_ms'])<=float(row['max_ms'])
assert images[0]==images[1]==images[2]
assert (root/'samples.tsv').read_text().count('\n')>10
assert (root/'samples.tsv.maps').exists()
print('Profiling off/stages/sampling: identical headless state, pixels, finite distributions, reconciled exclusive stages and native PC samples')
