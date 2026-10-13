#!/usr/bin/env python3
"""Summarise off/shader/batch timer diagnostics and dense-opening pairs."""
import csv,json,re,statistics as st,math
from pathlib import Path
import sys
root=Path(sys.argv[1])
def tags(line):return dict(re.findall(r'([A-Za-z_][A-Za-z_0-9]*)=([^ ]+)',line))
def pct(a,q):return sorted(a)[max(0,math.ceil(len(a)*q)-1)]
data={}
gpu=root
if gpu.exists():
 for mode in ['off','shader','batch']:
  p=gpu/f'race-{mode}.gpu.csv'
  if not p.exists():continue
  rows=list(csv.DictReader(p.open()));valid=[float(r['elapsed_ns'])/1e6 for r in rows if r['disjoint']=='0']
  log=(gpu/f'race-{mode}.log').read_text().splitlines();cpu={}
  for line in log:
   if line.startswith('[SUBMIT-OP]'):
    t=tags(line)
    if t['clock']=='cpu':cpu[t['op']]={k:float(v) for k,v in t.items() if k not in ('op','clock')}
  profiles=[tags(l) for l in log if l.startswith('[PROFILE] total')];full_cpu=[tags(l) for l in log if l.startswith('[SUBMISSION-CPU]')]
  x={'gpu_samples':len(valid),'gpu_ms_median':st.median(valid),'gpu_ms_p95':pct(valid,.95),'gpu_disjoint':sum(r['disjoint']!='0' for r in rows),'CPU_operations':cpu,'profile':profiles[-1] if profiles else {}}
  samples=int(x['profile'].get('samples',0))
  if samples:x['estimated_submission_cpu_ms_per_presentation']=sum(c['estimated_total_ms'] for c in cpu.values() if c is not cpu.get('query'))/samples
  x['complete_submission_CPU']=full_cpu[-1] if full_cpu else {};data[mode]=x
 dense={}
 for mode in ['baseline','candidate']:
  runs=[]
  for p in sorted(gpu.glob(f'dense-{mode}-*.presentations.csv')):
   rows=[r for r in csv.DictReader(p.open()) if 1400<=int(r['frame'])<=1700]
   if len(rows)<2:continue
   swaps=[float(r['presented']) for r in rows];interval=[b-a for a,b in zip(swaps,swaps[1:])]
   logs=p.with_name(p.name.replace('.presentations.csv','.log')).read_text().splitlines();rate=tags(next(l for l in logs if l.startswith('[THREAD-RATE]')))
   runs.append({'fps':(len(rows)-1)/(swaps[-1]-swaps[0]),'low1_fps':1/st.mean(sorted(interval,reverse=True)[:max(1,math.ceil(len(interval)*.01))]),'p99_ms':pct(interval,.99)*1000,'worst_ms':max(interval)*1000,'draws':st.mean(float(r['draws']) for r in rows),'submission_ms':st.mean(float(r['submission_wall']) for r in rows)*1000,'age_median_ms':st.median(float(r['publication_age']) for r in rows)*1000,'age_p95_ms':pct([float(r['publication_age']) for r in rows],.95)*1000,'sim_hz':float(rate['sim_hz'])})
  if runs:dense[mode]={'runs':runs,'medians':{k:st.median(r[k] for r in runs) for k in runs[0]}}
 data['dense']=dense
(root/'gpu-dense-summary.json').write_text(json.dumps(data,indent=2)+'\n')
print(json.dumps(data,indent=2))
