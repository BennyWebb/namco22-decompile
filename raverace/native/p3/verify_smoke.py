#!/usr/bin/env python3
import pathlib,argparse,json,csv,re,collections
p=argparse.ArgumentParser();p.add_argument('run',type=pathlib.Path);p.add_argument('mode',choices=['nice','rr','off']);a=p.parse_args();b=a.run
assert (b/'exit.txt').read_text().strip()=='0';rows=list(csv.DictReader((b/'thread.csv').open()));assert [int(r['frame']) for r in rows]==list(range(1,1901))
root=pathlib.Path(__file__).resolve().parents[3];ref=[json.loads(l) for l in (root/'raverace/benchmarks/native-p1-20261015/validation-q3/states.jsonl').read_text().splitlines()];actual=[json.loads(l) for l in (b/'states.jsonl').read_text().splitlines()];assert len(actual)==21 and actual[:18]==ref[:18];assert actual[-3]['frames']==actual[-2]['board_frame']==actual[-1]['audio_frame']==1900
log=(b/'run.txt').read_text();records=[dict(re.findall(r'(\w+)=([^ ]+)',l)) for l in log.splitlines() if l.startswith('[P3-PRIORITY]')]
expected={0:(2,5,0),1:(0,0,-5),2:(2,5,0)} if a.mode=='rr' else {0:(0,0,-10),1:(0,0,-5),2:(0,0,-10)}
if a.mode=='off':
 assert not records
 seen=set();main=(b/'pid.txt').read_text().strip()
 for line in (b/'memory.txt').read_text().splitlines():
  m=re.match(r'(\d+) \((.*)\) (.*)',line)
  if not m:continue
  role=0 if m[1]==main else 1 if m[2]=='rr-prepare' else 2 if m[2]=='rr-present' else None
  if role is not None:
   x=m[3].split();assert (int(x[38]),int(x[37]),int(x[16]))==(0,0,0);seen.add(role)
 assert seen=={0,1,2}
else:
 assert len(records)==3
 for r in records:
  role=int(r['role']);assert (int(r['policy']),int(r['rt']),int(r['nice']))==expected[role],r;assert int(r['error'])==0
 seen=set()
 for line in (b/'memory.txt').read_text().splitlines():
  m=re.match(r'(\d+) \((.*)\) (.*)',line)
  if not m:continue
  r=next((r for r in records if r['tid']==m[1]),None)
  if not r:continue
  x=m[3].split();role=int(r['role'])
  if (int(x[38]),int(x[37]),int(x[16]))==expected[role]:seen.add(role)
 assert seen=={0,1,2},seen
for tag in ['clocks','production','rt']:assert (b/(tag+'.before')).read_bytes()==(b/(tag+'.after')).read_bytes()
assert (b/'restoration-failed.txt').read_text().strip()=='0'
assert re.search(r'P1B-IO.*dropped_records=0 .*errors=0',log)
result=dict(mode=a.mode,ticks=1900,checkpoint_domain_records=len(actual),original_reference_records_equal=18,terminal_tick=1900,receipts=records,census_applied=True,restored=True)
(b/'smoke-proof.json').write_text(json.dumps(result,indent=2)+'\n');print(b.name,'SMOKE PASS',a.mode)
