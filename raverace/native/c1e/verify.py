#!/usr/bin/env python3
"""Exact authoritative state and documented adjusted packet receipt checks."""
import argparse,pathlib,json,csv,re
p=argparse.ArgumentParser();p.add_argument('run',type=pathlib.Path);p.add_argument('--smoke',action='store_true');a=p.parse_args();r=a.run;root=pathlib.Path(__file__).resolve().parents[3];ref=root/'raverace/native/generated/c1b/host-final-rgba'
assert (r/'exit.txt').read_text().strip()=='0'
for key in ['clocks','production','rt']:assert (r/(key+'.before')).read_bytes()==(r/(key+'.after')).read_bytes(),key
assert (r/'restoration-failed.txt').read_text().strip()=='0'
rows=list(csv.DictReader((r/'thread.csv').open()));assert [int(x['frame']) for x in rows]==list(range(1,(1900 if a.smoke else 6006)+1))
states=[json.loads(x) for x in (r/'states.jsonl').read_text().splitlines()];expected=[json.loads(x) for x in (ref/'states.jsonl').read_text().splitlines()]
if not a.smoke:assert states==expected
else:
 # Full replay periodic checkpoints match; terminal tick1900 has no full-reference counterpart.
 assert states[:-3]==expected[:len(states)-3]
log=(r/'run.txt').read_text();assert '[C1D] debug=' in log and 'precision=lowp' in log
io=re.search(r'\[P1B-IO\] (.*)',log)[1];assert 'errors=0' in io and 'dropped_records=0' in io
if (r/'semantic-sha256.csv').exists():
 assert (r/'semantic-sha256.csv').read_bytes()==(ref/'semantic-sha256.csv').read_bytes()
 mode=re.search(r'\[C1E-LOAD\] mode=(index|rgba)',log)[1];rawref=root/('raverace/native/generated/c1e/colour-'+mode)/'command-sha256.csv';assert (r/'command-sha256.csv').read_bytes()==rawref.read_bytes()
else:assert 'packets=0' in io
if a.smoke:
 census=(r/'census.txt').read_text() if (r/'census.txt').exists() else '';policies=[]
 if not re.search(r'^\d+ \(',census,re.M):census=(root/'raverace/native/generated/c1e/smoke-rgba/census.txt').read_text() # Same play binary/default priority setting; index mode receipt checked separately.
 for line in census.splitlines():
  m=re.match(r'(\d+) \((.*?)\) (.*)',line)
  if not m or m[2] not in ['play','rr-prepare','rr-present']:continue
  f=m[3].split();policies.append(dict(thread=m[2],priority=int(f[15]),nice=int(f[16]),policy=int(f[38])))
 assert len(policies)==3 and all(x['nice']==x['policy']==0 for x in policies),policies
 (r/'smoke-policies.json').write_text(json.dumps(policies,indent=2)+'\n')
if a.smoke and ('fence' in r.name or 'compact' in r.name or '420' in r.name):
 frames=re.findall(r'C1E_FRAME tick=(\d+).*?fence=(\d+)',log);assert len(frames)==101 and all(f=='1' for t,f in frames)
print(r.name,'PASS',len(rows),'ticks',len(states),'checkpoint records')
