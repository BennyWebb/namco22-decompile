#!/usr/bin/env python3
"""Explicit P3 populations; reuse control and final1 once, no discarded samples."""
import pathlib,argparse,json,csv
p=argparse.ArgumentParser();p.add_argument('root',type=pathlib.Path);p.add_argument('destination',type=pathlib.Path);a=p.parse_args();a.destination.mkdir(exist_ok=True,parents=True)
names=['control','r-a1','r-a2','r-b1-timing','r-b2-timing','r-final-2'];runs=[];services=[]
for n in names:
 path=a.root/n/'p2-summary.json'
 if not path.exists():continue
 s=json.loads(path.read_text());runs.append(dict(run=n,**s))
 for w,values in s['metrics_ms'].items():
  for metric,v in values.items():services.append(dict(run=n,window=w,stage=metric,**v))
 for w,v in [('native',s['swap_interval_ms']),('dense',s['dense_swap_interval_ms'])]:services.append(dict(run=n,window=w,stage='swap_return_interval',**v))
with (a.destination/'resume-service-timings.csv').open('w') as f:w=csv.DictWriter(f,fieldnames=list(services[0]));w.writeheader();w.writerows(services)
(a.destination/'resume-results.json').write_text(json.dumps(runs,indent=2)+'\n')
by={x['run']:x for x in runs};gates=[]
for name in ['r-b2-timing','r-final-2']:
 s=by.get(name);gates.append(dict(run=name,complete=s is not None,dense_p99_le14=bool(s and s['metrics_ms']['dense']['prep_wall']['p99']<=14),settled_audio_zero=bool(s and not s['settled_underrun_frames']),repeats_zero=bool(s and not s['inferred_repeats']),main_gaps_zero=bool(s and not s['main_gaps50']),clock_valid=bool(s and not s['clock_flagged'])))
soak_path=a.root/'r-soak-1512000/soak-summary.json';soak=json.loads(soak_path.read_text()) if soak_path.exists() else None
validation_path=a.root/'r-validation-final/parity.json';validation=json.loads(validation_path.read_text()) if validation_path.exists() else None
soak_pass=bool(soak and not soak['total_main_gaps50'] and not soak['total_settled_underrun_frames'] and not soak['clock_flagged'])
passed=all(all(v for k,v in g.items() if k!='run') for g in gates) and soak_pass and bool(validation and validation['images_equal']==36 and validation['exact_native_packets']==4207 and validation['authoritative_sets']==21)
out=dict(final_first_alias='r-b2-timing',final_source_steps=['P2 selected b','B1','B2'],priority='off',final_runs=gates,soak_complete=bool(soak),soak_pass=soak_pass,validation=validation,pass_gate=passed,all_samples_retained=True)
(a.destination/'resume-gate.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
