#!/usr/bin/env python3
"""Native-phase all-thread census: actual policy/nice, CPU/runqueue progress."""
import pathlib,argparse,json,re,collections,csv
p=argparse.ArgumentParser();p.add_argument('run',type=pathlib.Path);p.add_argument('--soak-parent',type=pathlib.Path);p.add_argument('--smoke',action='store_true');a=p.parse_args();b=a.run
if a.smoke:
 rr={int(r['frame']):r for r in csv.DictReader((b/'thread.csv').open())};r=rr[1800];s=dict(clock_begin_monotonic=float(r['presented'])-float(r['publication_age'])-float(r['simulation_ms'])/1000,native_end_monotonic=float(rr[1900]['presented']))
else:s=json.loads((b/'p2-summary.json').read_text())
source=a.soak_parent or b;rows=[];lines=[]
def parse(lines):
 if not lines:return
 t=float(lines[0].split()[1])
 if not s['clock_begin_monotonic']<=t<=s['native_end_monotonic']:return
 files=dict(re.findall(r'FILE ([^\n]+)\n(.*?)END\n',''.join(lines),re.S))
 for path,v in files.items():
  if '/task/' not in path or not path.endswith('/stat'):continue
  m=re.match(r'(\d+) \((.*)\) (.*)',v);tid=int(m[1]);name=m[2];x=m[3].split();base=path[:-4];sched=files.get(base+'schedstat','').split();status=files.get(base+'status','')
  if not sched:continue
  switches={k:int(v) for k,v in re.findall(r'^(voluntary_ctxt_switches|nonvoluntary_ctxt_switches):\s*(\d+)',status,re.M)}
  rows.append(dict(t=t,tid=tid,name=name,state=x[0],nice=int(x[16]),policy=int(x[38]),rt_priority=int(x[37]),cpu=int(x[36]),runtime_ns=int(sched[0]),runqueue_ns=int(sched[1]),slices=int(sched[2]),**switches))
for l in (source/'memory.txt').open():
 if l.startswith('SAMPLE '):parse(lines);lines=[l]
 elif lines:lines.append(l)
parse(lines);groups=collections.defaultdict(list)
for r in rows:groups[r['tid']].append(r)
result=[]
for tid,v in groups.items():
 lo,hi=v[0],v[-1];elapsed=hi['t']-lo['t'];result.append(dict(maximum_census_interval_runqueue_ms=max(((y['runqueue_ns']-x['runqueue_ns'])/1e6 for x,y in zip(v,v[1:])),default=0),maximum_census_interval_ms=max(((y['t']-x['t'])*1000 for x,y in zip(v,v[1:])),default=0),tid=tid,name=lo['name'],samples=len(v),policies=dict(collections.Counter(f"{x['policy']}/{x['rt_priority']}/nice{x['nice']}" for x in v)),cpu_counts=dict(collections.Counter(x['cpu'] for x in v)),states=dict(collections.Counter(x['state'] for x in v)),observed_seconds=elapsed,runtime_ms=(hi['runtime_ns']-lo['runtime_ns'])/1e6,runqueue_ms=(hi['runqueue_ns']-lo['runqueue_ns'])/1e6,core_equivalent=(hi['runtime_ns']-lo['runtime_ns'])/1e9/elapsed if elapsed else None,context_switch_deltas={k:hi.get(k,0)-lo.get(k,0) for k in ['voluntary_ctxt_switches','nonvoluntary_ctxt_switches']}))
assert result,'Missing census'
(b/'thread-census-summary.json').write_text(json.dumps(result,indent=2)+'\n');print(b.name,len(result),'thread census identities')
