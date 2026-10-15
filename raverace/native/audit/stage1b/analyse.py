#!/usr/bin/env python3
"""Stage1b evidence summaries; no trimming and no runtime policy changes."""
import csv,json,re,math,argparse,collections,gzip
from pathlib import Path
import numpy as np
p=argparse.ArgumentParser();p.add_argument('run',type=Path);a=p.parse_args();b=a.run

def dist(v):
 s=np.sort(np.asarray(list(v),float));n=len(s)
 if not n:return None
 return dict(n=n,median=float(np.median(s)),p95=float(s[math.ceil(.95*n)-1]),p99=float(s[math.ceil(.99*n)-1]),worst=float(s[-1]),mean=float(s.mean()),stddev=float(s.std()),missed_16_693=int((s>16.693).sum()))
def rows(n):
 f=b/n
 return list(csv.DictReader(f.open())) if f.exists() else []
def picture_intervals(rr,frame,time):
 result=[];previous=None
 for r in rr:
  value=float(r[time])
  if previous is not None and 1800<=int(r[frame])<=6006:result.append((value-previous)*1000)
  previous=value
 return dist(result)
def fields(rr,names,factor=1):return {n:dist(float(r[n])*factor for r in rr) for n in names if rr and n in rr[0]}
out={};ticks=rows('ticks.csv');times={int(r['frame']):float(r['completed']) for r in ticks}
if ticks:
 intervals=[(int(ticks[i]['frame']),(float(ticks[i]['completed'])-float(ticks[i-1]['completed']))*1000) for i in range(1,len(ticks))]
 out['whole_replay_tick_intervals_ms']=dist(v for _,v in intervals)
 out['largest_whole_replay_tick_intervals']=[{'frame':f,'interval_ms':v} for f,v in sorted(intervals,key=lambda x:x[1],reverse=True)[:5]]
if 1799 in times and 6006 in times:
 lo,hi=times[1799],times[6006];out['window_seconds']=[lo,hi]
else:lo,hi=-float('inf'),float('inf')
for name,key in [('simulation.csv','simulation'),('audit.csv','audit'),('presentation.csv','presentation')]:
 allrows=rows(name)
 rr=[r for r in allrows if 1800<=int(r['tick'])<=6006]
 if rr:
  nums=[n for n in rr[0] if n.endswith('_ms')];out[key]=fields(rr,nums)
  if key=='presentation':
   out[key]['interval_ms']=picture_intervals(allrows,'tick','present_seconds')
   for n in ['audio_underruns','audio_overruns','audio_resets']:
    out[key][n+'_at_end']=int(rr[-1][n]);prior=[r for r in allrows if int(r['tick'])==1799];out[key][n+'_native_phase_delta']=int(rr[-1][n])-int(prior[-1][n]) if prior else None
run=(b/'run.txt').read_text() if (b/'run.txt').exists() else ''
frames=[dict(re.findall(r'(\w+)=([^ ]+)',l)) for l in run.splitlines() if l.startswith('FRAME ')]
frames=[r for r in frames if 1800<=int(r['tick'])<=6006]
if frames:out['native_renderer']=fields(frames,['total_ms','cpu_ms','finish_ms','world_submit_ms','hud_submit_ms'])
threadrows=rows('thread.csv');rr=[r for r in threadrows if 1800<=int(r['frame'])<=6006]
if rr:
 out['production_thread']=fields(rr,['cpu','scene_wall','draw_wall','submission_wall','swap_wall','prep_cpu','hud_cpu','snapshot_copy_wall','prep_wall'],1000)
 out['production_thread']['interval_ms']=picture_intervals(threadrows,'frame','presented')
 out['production_thread']['native_walk_compile_fence']='Not applicable: original GLES; independent final GPU fence is not exported.'
for tag in ['SIM-STAGE','SIM-RATE','THREAD-RATE','AUDIO','AUDIO-OUTPUT','TIMING','GEOMETRY-RATE']:
 out[tag]=[l for l in run.splitlines() if l.startswith('['+tag+']')]
# Raw sampler files contain driver text and numeric delta vectors, with timestamps.
whole_temps=[];whole_cooling=collections.defaultdict(collections.Counter);samples=[];thread={};temps=[];rss=[];available=[];cooling=collections.defaultdict(collections.Counter);caps=[];owncpu=[];zones=collections.defaultdict(list);freqs=collections.defaultdict(list);gpugovs=collections.Counter();gpu=collections.Counter();trans=[]
clock=b/'clocks.txt'
rawclock=clock.read_text() if clock.exists() else gzip.open(b/'clocks.txt.gz','rt').read() if (b/'clocks.txt.gz').exists() else ''
hz=int(re.search(r'^HZ (\d+)',rawclock,re.M).group(1)) if rawclock else 100
for chunk in rawclock.split('SAMPLE ')[1:]:
 t=float(chunk.splitlines()[0]);
 fs=dict(re.findall(r'FILE ([^\n]+)\n(.*?)END\n',chunk,re.S))
 for path,val in fs.items():
  try:
   if '/thermal_zone' in path and path.endswith('/temp'):whole_temps.append(int(val.strip())/1000)
   if '/cooling_device' in path and path.endswith('/cur_state'):whole_cooling[path][int(val.strip())]+=1
  except ValueError:pass
 if not lo<=t<=hi:continue
 samples.append(t)
 for path,val in fs.items():
  if path.endswith('/cur_freq'):
   try:gpu[int(val.strip())]+=1
   except ValueError:pass
  elif path.endswith('/governor'):gpugovs[val.strip()]+=1
  elif '/cooling_device' in path and path.endswith('/cur_state'):
   try:cooling[path][int(val.strip())]+=1
   except ValueError:pass
  elif path.endswith('/scaling_max_freq'):
   try:caps.append(int(val.strip()))
   except ValueError:pass
  elif path.endswith('/scaling_cur_freq'):
   try:freqs[path].append(int(val.strip()))
   except ValueError:pass
  elif '/thermal_zone' in path and path.endswith('/temp'):
   try:temps.append(int(val.strip())/1000);zones[path].append(int(val.strip())/1000)
   except ValueError:pass
  elif '/task/' in path and path.endswith('/stat'):
   m=re.match(r'(\d+) \((.*)\) (.*)',val)
   if not m:continue
   tid,name,rest=m.groups();v=rest.split();cpu=int(v[11])+int(v[12]);core=int(v[36]);x=thread.setdefault(tid,dict(name=name,first=t,first_cpu=cpu,last=t,last_cpu=cpu,cores=collections.Counter(),status_first=None,status_last=None));x['last']=t;x['last_cpu']=cpu;x['cores'][core]+=1;x['observed_core_changes']=x.get('observed_core_changes',0)+int(x.get('last_core',core)!=core);x['last_core']=core
  elif '/task/' in path and path.endswith('/status'):
   tid=path.split('/')[-2];x=thread.get(tid)
   if x is not None:
    mask=re.search(r'^Cpus_allowed_list:\s*(.*)',val,re.M)
    if mask:x.setdefault('allowed_mask_counts',collections.Counter())[mask.group(1).strip()]+=1
    cs={k:int(v) for k,v in re.findall(r'^(voluntary_ctxt_switches|nonvoluntary_ctxt_switches):\s*(\d+)',val,re.M)}
    if x['status_first'] is None:x['status_first']=cs
    x['status_last']=cs
 for path,val in fs.items():
  if re.fullmatch(r'/proc/\d+/status',path):
   m=re.search(r'^VmRSS:\s*(\d+)',val,re.M)
   if m:rss.append(int(m.group(1)))
  if path=='/proc/meminfo':
   m=re.search(r'^MemAvailable:\s*(\d+)',val,re.M)
   if m:available.append(int(m.group(1)))
 m=re.search(r'^SAMPLER_CPU ([\d.]+)',chunk,re.M)
 if m:owncpu.append(float(m.group(1)))
 m=re.search(r'^TRANS_DELTA valid=1 count=\d+(.*)',chunk,re.M)
 if m:trans.append([int(x) for x in m.group(1).split()])
def physical(v):
 r=dist(v)
 if r:r.pop('missed_16_693',None);r['minimum']=float(min(v))
 return r
out['whole_run_thermal']={'peak_temperature_c':max(whole_temps) if whole_temps else None,'cooling_state_counts':{k:dict(v) for k,v in whole_cooling.items()}}
out['clocks']={'cooling_state_counts':{k:dict(v) for k,v in cooling.items()},'cpu_policy_max_khz':physical(caps),'sampler_core_equivalents':(owncpu[-1]-owncpu[0])/(samples[-1]-samples[0]) if len(owncpu)>1 else None,'peak_rss_kib':max(rss) if rss else None,'minimum_mem_available_kib':min(available) if available else None,'samples':len(samples),'sample_interval_ms':physical(np.diff(samples)*1000),'gpu_frequency_hz_counts':dict(gpu),'gpu_governor_counts':dict(gpugovs),'cpu_frequency_khz':{k:physical(v) for k,v in freqs.items()},'cpu_frequency_khz_counts':{k:dict(collections.Counter(v)) for k,v in freqs.items()},'temperature_c':physical(temps),'thermal_zones_c':{k:physical(v) for k,v in zones.items()},'gpu_stays_240mhz_in_samples':bool(gpu) and set(gpu)=={240000000},'trans_stat_numeric_delta_sums':[sum(v[i] for v in trans) for i in range(len(trans[0]))] if trans and all(len(v)==len(trans[0]) for v in trans) else None,'throttling_note':'Frequency/temperature observations alone cannot establish thermal-cause throttling; retain raw driver evidence.'}
for x in thread.values():
 x['cpu_seconds']=(x['last_cpu']-x['first_cpu'])/hz
 x['core_equivalents']=x['cpu_seconds']/max(x['last']-x['first'],1e-9)
 x['context_switch_deltas']={k:x['status_last'].get(k,0)-x['status_first'].get(k,0) for k in (x['status_first'] or {})} if x['status_last'] else {}
 x['cores']=dict(x['cores'])
out['threads']=thread
(b/'stage1b-summary.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps(out,indent=2))
