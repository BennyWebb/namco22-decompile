#!/usr/bin/env python3
"""Comparable traced A/B pacing, stage, queue and memory measurements.
Usage: summarise_texture_replay.py RESULTS_DIRECTORY FIRST LAST
"""
import csv,json,math,pathlib,re,statistics as st,sys
root=pathlib.Path(sys.argv[1]);first,last=map(int,sys.argv[2:])
def percentile(v,p):return sorted(v)[max(0,math.ceil(len(v)*p)-1)]
def fields(line):return dict(re.findall(r'(\w+)=([^ ]+)',line))
results=[]
for path in sorted(root.glob('*.presentations.csv')):
 name=path.name.removesuffix('.presentations.csv');variant,pair=name.rsplit('-',1)
 ticks={int(r['frame']):float(r['completed']) for r in csv.DictReader((root/f'{name}.ticks.csv').open())}
 begin,end=ticks[first-1],ticks[last]
 rows=list(csv.DictReader(path.open()))
 chosen=[r for r in rows if begin<float(r['presented'])<=end and int(r['frame'])>=first]
 dt=[float(b['presented'])-float(a['presented']) for a,b in zip(rows,rows[1:]) if begin<float(b['presented'])<=end and int(a['frame'])>=first]
 assert dt and min(dt)>0
 worst=sorted(dt,reverse=True)[:math.ceil(len(dt)*.01)]
 log=(root/f'{name}.log').read_text().splitlines()
 rate=fields(next(l for l in log if l.startswith('[THREAD-RATE]')))
 audio=fields(next(l for l in log if l.startswith('[AUDIO-OUTPUT]')))
 state=json.loads((root/f'{name}.json').read_text())
 assert not state['traps'] and not state['dsp_fault'] and not state['sound_fault']
 r={'name':name,'variant':variant,'pair':int(pair),'fps':len(chosen)/(end-begin),'simulation_hz':(last-first+1)/(end-begin),
    'low1_fps':1/st.mean(worst),'mean_interval_ms':st.mean(dt)*1000,
    'median_interval_ms':st.median(dt)*1000,'p99_ms':percentile(dt,.99)*1000,
    'max_ms':max(dt)*1000,'intervals_over50ms':sum(d>.05 for d in dt),
    'resyncs':int(rate['resyncs']),'discarded_lag_ms':float(rate['discarded_lag_ms']),
    'snapshot_drops':int(rate['dropped']),
    'audio_underrun_samples':int(audio['underrun_samples']),
    'audio_overrun_samples':int(audio['overrun_samples']),
    'audio_latency_resets':int(audio['latency_resets'])}
 for k in ('cpu','scene_wall','draw_wall','submission_wall','swap_wall','lookup_decode_wall','decode_wall','upload_wall','eviction_wall','publication_age'):
  r[k+'_ms']=st.mean(float(x[k]) for x in chosen)*1000
 for k in ('draws','triangles','binds','queue_ready'):
  r[k]=st.mean(float(x[k]) for x in chosen)
 for k in ('misses','uploads','evictions','clock_probes','delete_calls','decoded_texels'):
  if k in chosen[0]:r[k]=sum(int(x[k]) for x in chosen)
 r['texture_peak_mib']=max(int(x['texture_bytes']) for x in chosen)/1048576
 r['queue_peak']=max(int(x['queue_ready']) for x in chosen)
 r['worst_frames']=sorted([dict(x,interval_ms=(float(x['presented'])-float(a['presented']))*1000) for a,x in zip(rows,rows[1:]) if begin<float(x['presented'])<=end and int(a['frame'])>=first],key=lambda x:x['interval_ms'],reverse=True)[:10]
 mem=(root/f'{name}.memory')
 if mem.exists():
  t=mem.read_text();rss=list(map(int,re.findall(r'VmRSS:\s*(\d+)',t)));available=list(map(int,re.findall(r'MemAvailable:\s*(\d+)',t)))
  if rss:r['rss_peak_mib']=max(rss)/1024
  if available:r['available_min_mib']=min(available)/1024
 results.append(r)
assert results
variants={}
for v in ('baseline','candidate'):
 runs=[r for r in results if r['variant']==v]
 variants[v]={k:st.median(r[k] for r in runs) for k in runs[0] if isinstance(runs[0][k],(float,int)) and k!='pair'}
 variants[v]['fps_range']=[min(r['fps'] for r in runs),max(r['fps'] for r in runs)]
(root/'summary.json').write_text(json.dumps({'runs':results,'medians':variants},indent=2)+'\n')
for metric in ('fps','simulation_hz','low1_fps','p99_ms','max_ms','cpu_ms','decode_wall_ms','upload_wall_ms','eviction_wall_ms','submission_wall_ms','misses','evictions','delete_calls','rss_peak_mib'):
 print(metric,*(round(variants[v].get(metric,float('nan')),3) for v in variants))
print('Detailed report:',root/'summary.json')
