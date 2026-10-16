#!/usr/bin/env python3
"""Per-run comparison and explicit P2 gate evaluation; no discarded samples."""
import argparse,pathlib,csv,json,statistics,math,collections
p=argparse.ArgumentParser();p.add_argument('root',type=pathlib.Path);p.add_argument('destination',type=pathlib.Path);a=p.parse_args();a.destination.mkdir(exist_ok=True,parents=True)
rows=[];summaries={}
for path in sorted(a.root.glob('*/p2-summary.json')):
 s=json.loads(path.read_text());name=path.parent.name;summaries[name]=s
 row=dict(run=name,role='preliminary-partial-split' if name in ['play-1','timing-a','timing-b'] else 'validation' if 'validation' in name else 'baseline' if name.startswith('baseline') else 'headline-play' if name.startswith('v2-play-') else 'headline-final' if name.startswith('final-') else 'intermediate',native_underrun_frames=s['native_underrun_frames'],settled_underrun_frames=s['settled_underrun_frames'],release_tick=s['release_tick'],repeats=s['inferred_repeats'],main_gaps50=s['main_gaps50'],clock_flagged=s['clock_flagged'],peak_temperature_c=s['peak_native_temperature_c'])
 for window in ['native','dense']:
  for metric in ['prep_wall','prep_cpu','native_compile_ms','native_walk_ms','simulation_ms','draw_wall','swap_wall','publication_age']:
   for pct in ['median','p95','p99','worst']:row[f'{window}_{metric}_{pct}_ms']=s['metrics_ms'][window][metric][pct]
 for pct in ['median','p95','p99','worst']:row[f'swap_interval_{pct}_ms']=s['swap_interval_ms'][pct]
 rows.append(row)
if rows:
 with (a.destination/'run-comparison.csv').open('w') as f:w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
service=[]
for name,s in summaries.items():
 for window,metrics in s['metrics_ms'].items():
  for stage,values in metrics.items():service.append(dict(run=name,window=window,stage=stage,**values))
if service:
 with (a.destination/'service-timings.csv').open('w') as f:w=csv.DictWriter(f,fieldnames=list(service[0]));w.writeheader();w.writerows(service)
headlines={}
for family,names in [('baseline',[f'baseline-{i}' for i in [1,2,3]]),('play',[f'v2-play-{i}' for i in [1,2,3]]),('final',[f'final-{i}' for i in [1,2,3]])]:
 available=[n for n in names if n in summaries];data={k:[] for k in ['prep','compile','walk','dense_prep','dense_compile','dense_walk','interval']}
 for name in available:
  rr=list(csv.DictReader((a.root/name/'thread.csv').open()))
  for old,r in zip(rr,rr[1:]):
   if int(r['frame'])<1800:continue
   for k,col,scale in [('prep','prep_wall',1000),('compile','native_compile_ms',1),('walk','native_walk_ms',1)]:
    data[k].append(float(r[col])*scale)
    if 3550<=int(r['frame'])<=3590:data['dense_'+k].append(float(r[col])*scale)
   data['interval'].append((float(r['presented'])-float(old['presented']))*1000)
 def dist(v):
  s=sorted(v);n=len(s);return dict(n=n,median=statistics.median(s),p95=s[math.ceil(.95*n)-1],p99=s[math.ceil(.99*n)-1],worst=s[-1]) if n else None
 headlines[family]=dict(expected_runs=names,completed_runs=available,pooled_samples={k:dist(v) for k,v in data.items()},native_underrun_frames=sum(summaries[n]['native_underrun_frames'] for n in available),settled_underrun_frames=sum(summaries[n]['settled_underrun_frames'] for n in available),repeats=sum(summaries[n]['inferred_repeats'] for n in available),main_gaps50=sum(summaries[n]['main_gaps50'] for n in available))
gates=[]
for name in ['final-1','final-2','final-3']:
 s=summaries.get(name)
 gates.append(dict(run=name,complete=s is not None,dense_p99_le14=bool(s and s['metrics_ms']['dense']['prep_wall']['p99']<=14),settled_audio_zero=bool(s and s['settled_underrun_frames']==0),main_gaps50_zero=bool(s and s['main_gaps50']==0),at_most_one_single_repeat=bool(s and s['inferred_repeats']<=1),clock_unflagged=bool(s and not s['clock_flagged'])))
out=dict(headlines=headlines,final_run_gates=gates,headline_pass=all(all(v for k,v in g.items() if k!='run') for g in gates),soak_and_validation_require_separate_review=True)
(a.destination/'headline-summary.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out['final_run_gates'],indent=2))
