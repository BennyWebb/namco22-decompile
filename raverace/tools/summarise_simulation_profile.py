#!/usr/bin/env python3
"""Validate retrieved profiling comparison and emit per-run TSV + median table."""
import csv,hashlib,json,pathlib,re,statistics,sys
root=pathlib.Path(sys.argv[1]);rows=[];states={};videos={}
for path in sorted(root.glob('*-*-*.log')):
 workload,trial,mode=path.stem.split('-')
 if workload not in ('city','race') or mode not in ('off','stages','sampled'):continue
 lines=path.read_text().splitlines()
 def fields(prefix):
  return dict(re.findall(r'(\w+)=([^ ]+)',next(l for l in reversed(lines) if l.startswith(prefix))))
 frames=1700 if workload=='city' else 3600
 raw=path.with_suffix('.json').read_bytes();state=json.loads(raw)
 assert path.with_suffix('.exit').read_text().strip()=='0'
 assert state['frames']==frames and state['traps']==state['unmapped']==state['romwrites']==0
 assert not state['dsp_fault'] and not state['sound_fault']
 assert states.setdefault(workload,raw)==raw
 rate=fields('[SIM-RATE] ');clock=fields('[GEOMETRY-RATE] ');video=fields('[GEOMETRY] ')
 assert int(rate['steps'])==frames-1399 and int(rate['rendered'])+int(rate['skipped'])==int(rate['steps'])
 assert video['effective']=='on' and video['test_present_every']=='0'
 digest={k:video[k] for k in ('ticks','walks','direct_consumed','digest')}
 assert videos.setdefault(workload,digest)==digest
 row=dict(workload=workload,trial=trial,mode=mode,**rate,resyncs=clock['resyncs'],discarded_lag_ms=clock['discarded_lag_ms'],state_sha256=hashlib.sha256(raw).hexdigest())
 row['RSS_peak_mib']=float(fields('[MEMORY] ')['RSS_peak_kib'])/1024
 if mode!='off':
  summary=fields('[SIM] ');assert int(summary['ticks'])==frames-1399 and summary['overflow']=='0'
  assert float(summary['reconciliation_max_us'])<0.01
  stages=[]
  for l in lines:
   if l.startswith('[SIM-STAGE] '):
    data=dict(re.findall(r'(\w+)=([^ ]+)',l));stages.append(data)
    for key in ('avg_ms','median_ms','p95_ms','max_ms','percent'):row[data['name']+'_'+key]=data[key]
  wall=float(row['simulation_wall_avg_ms'])
  assert abs(sum(float(s['avg_ms']) for s in stages if not s['name'].startswith('simulation_'))-wall)<0.00002
  if mode=='sampled':assert fields('[SIM-SAMPLES] ')['overflow']=='0'
 else:assert not any(l.startswith('[SIM-STAGE]') for l in lines)
 rows.append(row)
assert len(rows)==18,len(rows)
keys=list(dict.fromkeys(k for r in rows for k in r))
with (root/'comparison.tsv').open('w') as f:
 w=csv.DictWriter(f,keys,delimiter='\t');w.writeheader();w.writerows(rows)
print('workload mode sim_Hz render_FPS wall_s renders skipped resyncs discarded_ms simulation_ms RSS_MiB')
for workload in ('city','race'):
 for mode in ('off','stages','sampled'):
  rs=[r for r in rows if r['workload']==workload and r['mode']==mode];assert len(rs)==3
  cols=('sim_hz','render_fps','wall_s','rendered','skipped','resyncs','discarded_lag_ms','simulation_wall_avg_ms','RSS_peak_mib')
  print(workload,mode,*(f'{statistics.median(float(r[k]) for r in rs):.3f}' if k in rs[0] else 'NA' for k in cols))
print('Complete states/video digests and exclusive accounting validated:',root/'comparison.tsv')
