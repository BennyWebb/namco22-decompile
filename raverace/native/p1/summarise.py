#!/usr/bin/env python3
"""Collate already-analysed P1 runs; preserve separate fenced/unfenced populations."""
import argparse,csv,json,pathlib
p=argparse.ArgumentParser();p.add_argument('root',type=pathlib.Path);a=p.parse_args()
records=[]
for path in sorted(a.root.glob('reference-*/p1-summary.json')):
 s=json.loads(path.read_text());parts=path.parent.name.split('-');r=dict(run=path.parent.name,round=parts[1],clocks=parts[2],queue=int(parts[3][1:]),mode=parts[4],pictures=s['presented_pictures'],ordered_once=s['ordered_once'],drops=s['recorded_drops'],repeats_estimate=s['repeated_vblanks_estimate'],unsupported=s['unsupported_pictures'],native_audio_underruns=s['native_phase_audio_underruns'],main_progress_gaps_over_50ms=s['stalls_over_50ms'])
 for k,v in s['service_ms'].items():
  if v:
   for q in ['median','p95','p99','worst']:r[k+'_'+q]=v[q]
 for k in ['presentation_interval_ms','resampler_ratio']:
  if s[k]:
   for q in ['median','p95','p99','worst','minimum']:r[k+'_'+q]=s[k][q]
 for k,v in s['memory'].items():
  if isinstance(v,dict) and 'worst' in v:r[k+'_peak']=v['worst'];r[k+'_minimum']=v['minimum']
 if s['diagnostic_gpu_ms']:
  for k,v in s['diagnostic_gpu_ms'].items():
   if v:
    for q in ['median','p95','p99','worst']:r['fence_'+k+'_'+q]=v[q]
 records.append(r)
keys=list(dict.fromkeys(k for r in records for k in r))
with (a.root/'all-run-metrics.csv').open('w') as f:
 w=csv.DictWriter(f,fieldnames=keys);w.writeheader();w.writerows(records)
(a.root/'measurement-counts.json').write_text(json.dumps({mode:{profile:{str(q):sum(r['mode']==mode and r['clocks']==profile and r['queue']==q for r in records) for q in [2,3]} for profile in ['stock','both']} for mode in ['timed','fence']},indent=2)+'\n')
print('runs',len(records))
