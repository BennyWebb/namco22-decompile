#!/usr/bin/env python3
"""Retain and summarise every sample from isolated 4A runs, including cold tails."""
import argparse,pathlib,re,json,csv,math,numpy as np
p=argparse.ArgumentParser();p.add_argument('run',type=pathlib.Path);a=p.parse_args();out={};deadline=16.693
def distribution(v):
 v=np.array(v,dtype=float);assert len(v) and np.all(np.isfinite(v));s=np.sort(v);n=len(v)
 return dict(n=n,median_ms=float(np.median(s)),p95_ms=float(s[math.ceil(n*.95)-1]),p99_ms=float(s[math.ceil(n*.99)-1]),worst_ms=float(s[-1]),mean_ms=float(s.mean()),missed_16_693_ms=int(np.count_nonzero(v>deadline)))
log=(a.run/'run.txt').read_text();frames=[dict(re.findall(r'(\w+)=([^ ]+)',l)) for l in log.splitlines() if l.startswith('FRAME ')];live=[dict(re.findall(r'(\w+)=([^ ]+)',l)) for l in log.splitlines() if l.startswith('LIVE ')]
if frames:
 assert [int(x['tick']) for x in frames]==list(range(1800,6007)),('native sample gaps',len(frames));out['native']={k:distribution([float(x[k]) for x in frames]) for k in frames[0] if k.endswith('_ms')};out['native_source_commands']=sum(int(x['primitives']) for x in frames);out['first_native_frame']=frames[0]
 with (a.run/'native-frames.csv').open('w') as f:w=csv.DictWriter(f,fieldnames=list(frames[0]));w.writeheader();w.writerows(frames)
if live:out['live']={k:distribution([float(x[k]) for x in live]) for k in ['walk_ms','compile_ms','renderer_ms']}
if (a.run/'telemetry.csv').exists():
 rows=list(csv.DictReader((a.run/'telemetry.csv').open()));assert [int(r['tick']) for r in rows]==list(range(1,6007));out['telemetry']={}
 for name,pred in [('all',lambda t:True),('legacy',lambda t:t<1800),('native',lambda t:t>=1800),('m3_window',lambda t:1800<=t<=5639)]:
  selected=[r for r in rows if pred(int(r['tick']))];d={k:distribution([float(r[k]) for r in selected]) for k in ['simulation_ms','scene_ms','draw_ms','swap_ms','host_wait_ms']};d['presentation_interval']=distribution([(float(rows[i]['present_seconds'])-float(rows[i-1]['present_seconds']))*1000 for i in range(1,len(rows)) if pred(int(rows[i]['tick']))]);out['telemetry'][name]=d
 out['audio_final']={k:int(rows[-1][k]) for k in ['audio_queued','audio_underruns','audio_overruns','audio_resets']}
if (a.run/'simulation.csv').exists():
 rows=list(csv.DictReader((a.run/'simulation.csv').open()));assert len(rows)==6006;out['simulation']={}
 for name,selected in [('all',rows),('native',[r for r in rows if int(r['tick'])>=1800])]:out['simulation'][name]={k:distribution([float(r[k]) for r in selected]) for k in rows[0] if k.endswith('_ms')}
if (a.run/'ticks.csv').exists():
 rows=list(csv.reader((a.run/'ticks.csv').open()));rows=[r for r in rows if r and r[0].isdigit()];assert [int(r[0]) for r in rows]==list(range(1,6007));out['completed_tick_intervals']={}
 for name,pred in [('all',lambda t:True),('native',lambda t:t>=1800),('m3_window',lambda t:1800<=t<=5639)]:out['completed_tick_intervals'][name]=distribution([(float(rows[i][1])-float(rows[i-1][1]))*1000 for i in range(1,len(rows)) if pred(int(rows[i][0]))])
mem=[]
for block in (a.run/'memory-samples.txt').read_text().split('SAMPLE ')[1:]:
 try:
  row={'uptime_seconds':float(block.split()[0])}
  for k in ['VmRSS','VmHWM','RssAnon','RssFile','MemAvailable','CmaFree']:
   m=re.search(r'^'+k+r':\s*(\d+)',block,re.M)
   if m:row[k+'_kib']=int(m[1])
  m=re.search(r'^mali0\s+(\d+)',block,re.M)
  if m:row['mali_pages']=int(m[1]);row['mali_bytes']=int(m[1])*4096
  mem.append(row)
 except (ValueError,IndexError):pass
if mem:
 out['memory']={'samples':len(mem),'max_rss_kib':max(r.get('VmRSS_kib',0) for r in mem),'max_hwm_kib':max(r.get('VmHWM_kib',0) for r in mem),'max_mali_bytes':max(r.get('mali_bytes',0) for r in mem),'min_available_kib':min(r.get('MemAvailable_kib',10**10) for r in mem),'last':mem[-1]};(a.run/'memory-samples.json').write_text(json.dumps(mem,indent=2)+'\n')
if (a.run/'telemetry.csv').exists():
 records=list(csv.DictReader((a.run/'telemetry.csv').open()));render={int(r['tick']):float(r['total_ms']) for r in frames}
 with (a.run/'deadline-misses.csv').open('w') as f:
  w=csv.writer(f);w.writerow(['tick','presentation_interval_ms','presentation_miss','world_hud_completion_ms','world_hud_miss'])
  for i in range(1,len(records)):
   tick=int(records[i]['tick']);interval=(float(records[i]['present_seconds'])-float(records[i-1]['present_seconds']))*1000;v=render.get(tick);w.writerow([tick,interval,int(interval>deadline),'' if v is None else v,'' if v is None else int(v>deadline)])
out['definition']='Nearest-rank p95/p99, standard median, all samples retained. Native interval includes previous completion to this completion, including transition. One final glFinish; finish_ms is CPU wait, not isolated GPU execution. RSS and Mali backing overlap; do not add.'
(a.run/'summary.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
