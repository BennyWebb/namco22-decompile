#!/usr/bin/env python3
"""Fixed full-race repeatability results; gate every candidate process separately."""
import pathlib,sys,json,re,numpy as np,subprocess,argparse
p=argparse.ArgumentParser();p.add_argument('device',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);a=p.parse_args();logs=[a.device/f'repeat-{i}-{v}'/'run.txt' for i in [1,2,3] for v in ['baseline','candidate']]
subprocess.run([sys.executable,str(pathlib.Path(__file__).with_name('analyse_depth_lut.py')),*map(str,logs),'--out',str(a.out)],check=True)
r=json.loads(a.out.read_text());prev=json.loads((a.out.parent/'slow-analysis.json').read_text())['slow_ticks'];samples=[]
for run,path in zip(r['runs'],logs):
 t=path.read_text();assert 'stage_sync=0' in t;assert run['options']=='NATIVE_OPTIONS front=1 folded=0 log=ram cull=1 trace_cap=16777216';run['tail_options']=next(l for l in t.splitlines() if l.startswith('TAIL_OPTIONS '));assert run['tail_options']=='TAIL_OPTIONS pen_rg=0 discard_depth='+('1' if 'candidate' in path.parent.name else '0')+' discard_color=0';run['load']=next(l for l in t.splitlines() if l.startswith('LOAD '));run['playback']=next(l for l in t.splitlines() if l.startswith('PLAYBACK '));f=[{k:float(v) for k,v in re.findall(r'(\w+)=(-?[\d.]+)',l)} for l in t.splitlines() if l.startswith('FRAME ')];samples.append(f);run['slow_ticks']=[int(x['tick']) for x in f if x['total_ms']>16.693];run['previous_69_still_missed']=len(set(prev)&set(run['slow_ticks']));run['cpu_ms_median']=float(np.median([x['cpu_ms'] for x in f]));run['gate_pass']=run['completion']['median']<=12 and run['completion']['p95']<=14
for b,c in zip(samples[::2],samples[1::2]):
 for x,y in zip(b,c):
  for k in ['tick','primitives','visible','culled','draws','vertices','instances','dynamic_bytes']:assert x[k]==y[k],(k,x['tick'])
pairs=[]
for i in range(3):
 b,c=samples[i*2:i*2+2];pairs.append(dict(pair=i+1,paired_completion_reduction_median=float(np.median([x['total_ms']-y['total_ms'] for x,y in zip(b,c)])),paired_interval_reduction_median=float(np.median([x['interval_ms']-y['interval_ms'] for x,y in zip(b,c)]))))
r['pairs']=pairs;r['candidate_gate_pass']=all(x['gate_pass'] for x in r['runs'][1::2]);r['frames_retained']=23040;r['component_warning']='World/pen/final subspans in one-fence runs are overlapping submissions/waits, not independent GPU completion timings. Use the separately fenced component controls for attribution.';r['gate']='Every candidate process must satisfy median <=12 ms and p95 <=14 ms. No sample removal; completion and interval misses use >16.693 ms. No pooled pass replaces a failing process.'
a.out.write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(dict(pairs=pairs,candidate_gate_pass=r['candidate_gate_pass']),indent=2))
