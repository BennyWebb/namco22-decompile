#!/usr/bin/env python3
"""Pool only identical configuration/mode, preserving every per-run sample."""
import argparse,pathlib,json,csv,math,statistics,collections
p=argparse.ArgumentParser();p.add_argument('root',type=pathlib.Path);a=p.parse_args();root=a.root

def dist(v):
 s=sorted(v);n=len(s)
 return dict(n=n,median=statistics.median(s),p95=s[math.ceil(.95*n)-1],p99=s[math.ceil(.99*n)-1],worst=s[-1],minimum=s[0],mean=statistics.mean(s)) if n else None
metrics=[];groups=collections.defaultdict(list)
for p in sorted(root.glob('reference-*/p1b-summary.json')):
 s=json.loads(p.read_text());parts=p.parent.name.split('-');group=parts[2]+'-'+parts[3];groups[group].append(p.parent)
 r=dict(run=p.parent.name,queue=int(parts[2][1:]),valve=parts[3],start_c=s['start_temperature_c'],repeats=s['inferred_repeated_vblanks'],audio_underrun_sample_frames=s['native_audio_underrun_sample_frames'],main_publication_gaps50=s['native_main_gaps_over50ms'],source_tick_gaps50=s['native_source_tick_gaps_over50ms'],transition_max_main_gap_ms=s['transition_max_main_gap_ms'],valve_activations=int(s['valve']['activations']),replaced=int(s['valve']['replaced']),log_drops=int(s['io']['dropped_records']),source_packets=s['source_packets'],checkpoints=s['authoritative_checkpoint_sets'])
 for k in ['presentation_interval_ms',*s['service_ms']]:
  d=s[k] if k=='presentation_interval_ms' else s['service_ms'][k]
  for q in ['median','p95','p99','worst']:r[k+'_'+q]=d[q]
 metrics.append(r)
with (root/'all-run-metrics.csv').open('w') as f:
 w=csv.DictWriter(f,fieldnames=list(metrics[0]));w.writeheader();w.writerows(metrics)
pooled={}
for group,paths in sorted(groups.items()):
 rr=[];intervals=[];ss=[]
 for p in paths:
  allrows=list(csv.DictReader((p/'thread.csv').open()));rr.extend(r for r in allrows if 1800<=int(r['frame'])<=6006)
  intervals.extend((float(y['presented'])-float(x['presented']))*1000 for x,y in zip(allrows,allrows[1:]) if 1800<=int(y['frame'])<=6006);ss.append(json.loads((p/'p1b-summary.json').read_text()))
 pooled[group]=dict(runs=[p.name for p in paths],n=len(rr),interval_ms=dist(intervals),repeats=sum(s['inferred_repeated_vblanks'] for s in ss),audio_underrun_sample_frames=sum(s['native_audio_underrun_sample_frames'] for s in ss),main_publication_gaps50=sum(s['native_main_gaps_over50ms'] for s in ss),source_tick_gaps50=sum(s['native_source_tick_gaps_over50ms'] for s in ss),valve_activations=sum(int(s['valve']['activations']) for s in ss),replaced=sum(int(s['valve']['replaced']) for s in ss),log_drops=sum(int(s['io']['dropped_records']) for s in ss),service_ms={k:dist(float(r[k])*scale for r in rr) for k,scale in [('simulation_ms',1),('simulation_blocked_ms',1),('native_walk_ms',1),('native_compile_ms',1),('prep_wall',1000),('prep_cpu',1000),('cpu',1000),('draw_wall',1000),('swap_wall',1000),('publication_age',1000),('prepared_wait',1000)]})
(root/'pooled-metrics.json').write_text(json.dumps(pooled,indent=2)+'\n')
q2=[json.loads((p/'p1b-summary.json').read_text()) for p in groups['q2-off']]
gate=dict(required_headline_runs=3,actual_headline_runs=len(q2),all_zero_native_main_gaps50=all(s['native_main_gaps_over50ms']==s['native_source_tick_gaps_over50ms']==0 for s in q2),all_handful_isolated_single_repeats=all(s['inferred_repeated_vblanks']<=5 and s['repeat_events_all_single'] and s['repeat_events_isolated_60ticks'] for s in q2),all_zero_native_audio_underruns=all(s['native_audio_underrun_sample_frames']==0 for s in q2),all_zero_log_drops=all(int(s['io']['dropped_records'])==0 for s in q2),soak_trend='pending analysis',performance_pass=False)
(root/'headline-exit-gate.json').write_text(json.dumps(gate,indent=2)+'\n');print(json.dumps(gate,indent=2))
