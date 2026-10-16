#!/usr/bin/env python3
import pathlib,json,csv,re,argparse,hashlib
p=argparse.ArgumentParser();p.add_argument('run',type=pathlib.Path);p.add_argument('--smoke',action='store_true');a=p.parse_args();r=a.run;root=pathlib.Path(__file__).resolve().parents[3];ref=root/'raverace/native/generated/c1b/host-final-rgba';assert (r/'exit.txt').read_text().strip()=='0';assert (r/'restoration-failed.txt').read_text().strip()=='0'
for k in ['clocks','production','rt']:assert (r/(k+'.before')).read_bytes()==(r/(k+'.after')).read_bytes(),k
assert len((r/'protected.after').read_text().splitlines())==47 and all(x.endswith(': OK') for x in (r/'protected.after').read_text().splitlines())
rows=list(csv.DictReader((r/'thread.csv').open()));assert [int(x['frame']) for x in rows]==list(range(1,(1900 if a.smoke else 6006)+1));states=[json.loads(x) for x in (r/'states.jsonl').read_text().splitlines()];expected=[json.loads(x) for x in (ref/'states.jsonl').read_text().splitlines()];assert (states[:-3]==expected[:len(states)-3]) if a.smoke else states==expected
log=(r/'run.txt').read_text();assert '[G1B] dynamic_page=1024 copies=3' in log and 'precision=lowp' in log;io=re.search(r'\[P1B-IO\] (.*)',log)[1];assert 'errors=0' in io and 'dropped_records=0' in io
raw=list(csv.DictReader((r/'command-sha256.csv').open()));expectedraw=list(csv.DictReader((root/'raverace/native/generated/g1b/colour-final/command-sha256.csv').open()))
if raw:assert raw==(expectedraw[:101] if a.smoke else expectedraw)
else:assert 'packets=0' in io
if (r/'semantic-sha256.csv').exists():assert (r/'semantic-sha256.csv').read_bytes()==(ref/'semantic-sha256.csv').read_bytes()
policies={}
if a.smoke:
 for l in (r/'census.txt').read_text().splitlines():
  m=re.match(r'(\d+) \((.*?)\) (.*)',l)
  if m and m[2] in ['play-v2','validation-v2','play-stream','rr-prepare','rr-present']:
   f=m[3].split();policies[m[2]]=dict(nice=int(f[16]),policy=int(f[38]),priority=int(f[15]))
 assert len(policies)==3 and all(x['nice']==x['policy']==0 for x in policies.values()),policies
(r/'verification.json').write_text(json.dumps(dict(pass_checks=True,ticks=len(rows),checkpoint_domain_records=len(states),native_packets=len(raw),policies=policies,binary_receipts=(r/'binary.sha256').read_text()),indent=2)+'\n');print(r.name,'PASS',len(rows),'ticks',len(raw),'packets',policies)
