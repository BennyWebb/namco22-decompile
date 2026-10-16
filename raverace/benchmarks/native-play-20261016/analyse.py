#!/usr/bin/env python3
"""Read local copied telemetry only; retain all summary totals and limits."""
import pathlib,json,csv,re,collections,hashlib,math
b=pathlib.Path(__file__).parent;r=next((b/'sessions').iterdir());j=json.loads((r/'summary.json').read_text());assert (r/'summary.json').read_bytes()==(b/'latest-summary.json').read_bytes();assert (r/'summary.txt').read_bytes()==(b/'latest-summary.txt').read_bytes()
T=1/59.7104
fields=pathlib.Path('raverace/benchmarks/native-g2-20261016/reference/thread.csv').open().readline().strip().split(',')
rows=[]
for n in ['profile.log.1','profile.log']:
 for line in (r/n).read_text().splitlines():
  v=next(csv.reader([line]));assert len(v)==len(fields);rows.append(dict(zip(fields,v)))
assert [int(x['frame']) for x in rows]==list(range(int(rows[0]['frame']),48877));by={int(x['frame']):x for x in rows}
logs=''.join((r/n).read_text() for n in ['stderr.log.1','stderr.log']);start,end=map(float,re.search(r'\[THREAD-PERIOD\] start=([\d.]+) end=([\d.]+)',logs).groups())
full=[];grouped=collections.defaultdict(lambda:dict(native=0,legacy=0,repeats=0,settled_underrun_frames=0,main_gaps_over50ms=0))
resources={};total=collections.Counter()
for key,g in j['groups'].items():
 if key!='unknown':
  vals=dict((k,int(v)) for k,v in re.findall(r'(main|sub|screen|course)=(\d+)',key));x=dict(state=key,**vals,pictures=g['native']+g['legacy'],nominal_seconds=(g['native']+g['legacy'])*T,**{k:g[k] for k in ['native','legacy','repeats','settled_underrun_frames','main_gaps_over50ms','preparation_p99_ms_upper_0_01']});full.append(x)
  z=grouped[(vals['main'],vals['sub'],vals['course'])]
  for k in z:z[k]+=g[k]
 for k in ['native','legacy','repeats','native_repeats','settled_underrun_frames','settled_native_underrun_frames','main_gaps_over50ms','native_main_gaps_over50ms','clock_flagged_samples']:total[k]+=g[k]
 for k,v in g['resources'].items():
  a=resources.setdefault(k,dict(v));a['min']=min(a['min'],v['min']);a['max']=max(a['max'],v['max'])
assert total['native']+total['legacy']==j['processed_pictures'];assert len(j['groups'])<128 and 'overflow' not in j['groups']
with (b/'state-course.csv').open('w') as f:
 w=csv.DictWriter(f,fieldnames=list(full[0]));w.writeheader();w.writerows(full)
coarse=[]
for (main,sub,course),g in grouped.items():coarse.append(dict(main=main,sub=sub,course=course,nominal_seconds=(g['native']+g['legacy'])*T,**g))
with (b/'state-course-grouped.csv').open('w') as f:
 w=csv.DictWriter(f,fieldnames=list(coarse[0]));w.writeheader();w.writerows(coarse)
events=[]
for e in j['events']:
 x=dict(e)
 if e['tick'] in by:x.update(elapsed_seconds=float(by[e['tick']]['presented'])-start,timestamp_method='exact retained swap return minus THREAD-PERIOD start',presented_monotonic=float(by[e['tick']]['presented']))
 else:x.update(elapsed_seconds=e['tick']*T,timestamp_method='approximate nominal tick/59.7104; early timestamps rotated out')
 if e['tick'] in by:x['row']=by[e['tick']]
 events.append(x)
(b/'events.json').write_text(json.dumps(events,indent=2)+'\n')
res=[json.loads(x) for x in (r/'resources.log').read_text().splitlines()];near=[]
for tick in [42068,42070,48556]:
 t=float(by[tick]['presented']);near.append(dict(tick=tick,samples=sorted(res,key=lambda x:abs(x['monotonic']-t))[:3]))
(b/'event-resource-context.json').write_text(json.dumps(near,indent=2)+'\n')
summary=dict(totals=dict(total),pipeline_seconds=end-start,pipeline_start_monotonic=start,pipeline_end_monotonic=end,nominal_picture_seconds=j['processed_pictures']*T,retained_profile_first=int(rows[0]['frame']),retained_profile_last=int(rows[-1]['frame']),state_groups=len(j['groups']),resources=resources,events=len(events),fallback_exact_keys=j['fallback_exact_keys'],log_io=j['io_receipts'],save_writes=j['save_writes'],record_overflow=int(re.search(r'record_overflow=(\d+)',logs)[1]),simulation_profiler_overflow=int(re.search(r'\[SIM\].*?overflow=(\d+)',logs)[1]),per_main_sub_course=coarse)
(b/'analysis.json').write_text(json.dumps(summary,indent=2)+'\n')
manifest={str(p.relative_to(b)):hashlib.sha256(p.read_bytes()).hexdigest() for p in r.rglob('*') if p.is_file()};manifest.update({n:hashlib.sha256((b/n).read_bytes()).hexdigest() for n in ['latest-summary.txt','latest-summary.json','clock-journal.json']});(b/'retrieved-sha256.json').write_text(json.dumps(manifest,indent=2)+'\n')
print(json.dumps(summary,indent=2))
