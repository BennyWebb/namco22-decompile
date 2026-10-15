#!/usr/bin/env python3
"""Long-form per-run distributions/counters. Retain all quantiles and outliers."""
import pathlib,argparse,json,csv,re
p=argparse.ArgumentParser();p.add_argument('evidence',type=pathlib.Path);a=p.parse_args();b=a.evidence;metrics=[];counters=[];clocks=[]
for path in sorted(b.glob('*/stage1b-summary.json')):
 r=json.loads(path.read_text());name=path.parent.name;kind='matched_performance' if re.match(r'^[BDE]-[123]-',name) else 'logging_supplement' if name.startswith('B-supplement-') else 'validation' if 'validation' in name else 'attribution' if name.startswith('F-') else 'standalone';seen=set()
 for group in ['simulation','audit','native_renderer','presentation','production_thread']:
  for key,val in r.get(group,{}).items():
   if isinstance(val,dict) and 'median' in val:
    metric=group+'.'+key;metrics.append(dict(run=name,run_class=kind,metric=metric,**val));seen.add(metric)
 if 'simulation' not in r:
  for line in r.get('SIM-STAGE',[]):
   values=dict(re.findall(r'(\w+)=([^ ]+)',line));key=values.get('name','')
   metrics.append(dict(run=name,run_class=kind,metric='original_simulation.'+key,n=int(values['n']),median=float(values['median_ms']),p95=float(values['p95_ms']),p99=None,worst=float(values['max_ms']),mean=float(values['avg_ms']),note='Unchanged executable does not export raw records/p99. Separate logging supplements are labelled.'))
 audio=dict(re.findall(r'(\w+)=([^ ]+)',(r.get('AUDIO-OUTPUT') or [''])[-1]));rate=dict(re.findall(r'(\w+)=([^ ]+)',(r.get('THREAD-RATE') or r.get('TIMING') or [''])[-1]));c=r.get('clocks',{})
 counters.append(dict(run=name,run_class=kind,scope='full replay except clock/window fields explicitly native phase',**audio,**rate,temperature_peak_c=c.get('temperature_c',{}).get('worst') if c.get('temperature_c') else None,minimum_mem_available_kib=c.get('minimum_mem_available_kib'),peak_rss_kib=c.get('peak_rss_kib'),clock_samples=c.get('samples'),largest_tick_interval_ms=r.get('whole_replay_tick_intervals_ms',{}).get('worst')))
 hist=c.get('gpu_frequency_hz_counts',{});total=sum(hist.values())
 for freq,count in hist.items():clocks.append(dict(run=name,run_class=kind,frequency_hz=int(freq),samples=count,sample_fraction=count/total,scope='ticks1800-6006'))

def write(name,rr):
 keys=list(dict.fromkeys(k for row in rr for k in row))
 with (b/name).open('w') as f:
  w=csv.DictWriter(f,fieldnames=keys);w.writeheader();w.writerows(rr)
write('all-run-metrics.csv',metrics);write('all-run-counters.csv',counters);write('gpu-frequency-distributions.csv',clocks)
print('Runs',len(counters),'metric rows',len(metrics),'GPU histogram rows',len(clocks))
