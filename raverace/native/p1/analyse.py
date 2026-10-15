#!/usr/bin/env python3
"""P1 complete populations; no warm-up/outlier trimming. Swap-return age is a proxy."""
import pathlib,argparse,csv,json,math,re,collections,gzip
import numpy as np
p=argparse.ArgumentParser();p.add_argument('run',type=pathlib.Path);a=p.parse_args();b=a.run;T=1000/59.7104

def dist(v):
 s=np.sort(list(v));n=len(s)
 return dict(n=n,median=float(np.median(s)),p95=float(s[math.ceil(.95*n)-1]),p99=float(s[math.ceil(.99*n)-1]),worst=float(s[-1]),minimum=float(s[0]),mean=float(np.mean(s))) if n else None
rows=list(csv.DictReader((b/'thread.csv').open()));phase=[r for r in rows if 1800<=int(r['frame'])<=6006];replay=(b/'states.jsonl').exists();selected=phase if replay else rows
out=dict(presented_pictures=len(rows),ordered_once=all(int(y['frame'])==int(x['frame'])+1 for x,y in zip(rows,rows[1:])),first_frame=int(rows[0]['frame']) if rows else None,last_frame=int(rows[-1]['frame']) if rows else None,analysis_population='1800–6006' if replay else 'whole continuous session',panel_period_ms=T,snapshot_to_swap_return_is_scanout_proxy=True)
metrics={}
for key,scale in [('simulation_ms',1),('simulation_blocked_ms',1),('native_walk_ms',1),('native_compile_ms',1),('prep_wall',1000),('prep_cpu',1000),('cpu',1000),('draw_wall',1000),('swap_wall',1000),('publication_age',1000),('snapshot_copy_wall',1000)]:
 metrics[key]=dist(float(r[key])*scale for r in selected)
out['service_ms']=metrics;out['queue_histogram']=dict(collections.Counter(int(r['occupied_slots']) for r in selected));out['resampler_ratio']=dist(float(r['resampler_ratio']) for r in selected);out['unsupported_pictures']=sum(int(r['frame'])>=1800 and not int(r['payload_native']) for r in selected);out['recorded_drops']=max([int(r['drops_cumulative']) for r in rows],default=0)
intervals=[];repeats=[];stalls=[];previous=None;prevpub=None
for r in rows:
 tick=int(r['frame']);present=float(r['presented']);published=present-float(r['publication_age']);include=not replay or 1800<=tick<=6006
 if previous is not None and include:
  dt=(present-previous)*1000;intervals.append(dt);count=max(0,int(math.floor(dt/T+.5))-1)
  if count:repeats.append(dict(frame=tick,seconds=present,interval_ms=dt,repeated_vblanks_estimate=count))
 if prevpub is not None and (published-prevpub)*1000>50:
  stalls.append(dict(frame=tick,start=prevpub,end=published,gap_ms=(published-prevpub)*1000,simulation_ms=float(r['simulation_ms']),fifo_blocked_ms=float(r['simulation_blocked_ms']),native=int(r['payload_native'])))
 previous=present;prevpub=published
out['presentation_interval_ms']=dist(intervals);out['repeated_vblanks_estimate']=sum(x['repeated_vblanks_estimate'] for x in repeats);out['repeat_events']=repeats;out['repeat_method']='Nearest integral measured-panel periods between every consecutive swap return; no sample exclusion. This is inferred repeated scanouts, not independent physical vblank counters.';out['repeat_histogram_10s']=dict(collections.Counter(int((x['seconds']-float(selected[0]['presented']))//10) for x in repeats)) if selected else {}
run=(b/'run.txt').read_text();frames=[dict(re.findall(r'(\w+)=([^ ]+)',l)) for l in run.splitlines() if l.startswith('FRAME ')];out['diagnostic_gpu_ms']={k:dist(float(r[k]) for r in frames if 1800<=int(r['tick'])<=6006) for k in ['total_ms','finish_ms','cpu_ms']} if frames else None;out['audio_summary']=[l for l in run.splitlines() if l.startswith(('[P1-AUDIO]','[AUDIO-OUTPUT]'))];out['pipeline_summary']=[l for l in run.splitlines() if l.startswith('[P1')]
if selected:
 prior=[r for r in rows if int(r['frame'])==1799];out['native_phase_audio_underruns']=int(selected[-1]['audio_underruns'])-int(prior[-1]['audio_underruns']) if prior else None
# Streaming raw memory/IO snapshots; preserve all raw records independently.
path=b/'memory.txt';f=path.open() if path.exists() else gzip.open(b/'memory.txt.gz','rt');samples=[];chunk=[]
def parse(lines):
 if not lines:return
 text=''.join(lines);stamp=float(text.splitlines()[0].split()[1]);files=dict(re.findall(r'FILE ([^\n]+)\n(.*?)END\n',text,re.S));m={}
 for name in ['VmRSS','VmHWM','MemAvailable','CmaFree']:
  matches=re.findall(r'^'+name+r':\s*(\d+)',text,re.M);m[name]=int(matches[0]) if matches else None
 gpu=files.get('/sys/kernel/debug/mali0/gpu_memory','');g=re.match(r'\S+\s+(\d+)',gpu);m['mali_allocated_pages']=int(g[1]) if g else None;m['temperature_max_c']=max([int(v.strip())/1000 for k,v in files.items() if '/thermal_zone' in k and k.endswith('/temp')],default=0);m['gpu_hz']=int(files.get('/sys/class/devfreq/gpu/cur_freq','0').strip());m['cpu_khz']=int(files.get('/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq','0').strip());m['gpu_cooling']=files.get('/sys/class/thermal/cooling_device1/cur_state','').strip();m['cpu_cooling']=files.get('/sys/class/thermal/cooling_device0/cur_state','').strip();m['time']=stamp
 vm=dict((k,int(v)) for k,v in re.findall(r'^(\w+)\s+(\d+)$',files.get('/proc/vmstat',''),re.M));m['vm']={k:v for k,v in vm.items() if k=='pgmajfault' or k.startswith(('pgscan','pgsteal','allocstall'))};ios=[v for k,v in files.items() if re.fullmatch(r'/proc/\d+/io',k)];m['io']={k:int(v) for k,v in re.findall(r'^(\w+):\s*(\d+)',ios[0],re.M)} if ios else {};threads={}
 for k,v in files.items():
  if re.fullmatch(r'/proc/\d+/task/\d+/stat',k):
   r=re.match(r'(\d+) \((.*?)\) ([A-Z])',v)
   if r:threads[r[1]]=dict(name=r[2],state=r[3],wchan=files.get(k[:-4]+'wchan','').strip(),stack=files.get(k[:-4]+'stack','').strip())
 m['threads']=threads;m['pressure']={k:v for k,v in files.items() if k.startswith('/proc/pressure/')};m['diskstats']=[l for l in files.get('/proc/diskstats','').splitlines() if 'mmcblk' in l];samples.append(m)
for line in f:
 if line.startswith('SAMPLE '):parse(chunk);chunk=[line]
 elif chunk:chunk.append(line)
parse(chunk);f.close()
mem={}
for k in ['VmRSS','VmHWM','MemAvailable','CmaFree','mali_allocated_pages','temperature_max_c']:mem[k]=dist(x[k] for x in samples if x[k] is not None)
mem['samples']=len(samples);mem['sample_gap_ms']=dist((y['time']-x['time'])*1000 for x,y in zip(samples,samples[1:]));mem['gpu_frequency_counts']=dict(collections.Counter(x['gpu_hz'] for x in samples));mem['cpu_frequency_counts']=dict(collections.Counter(x['cpu_khz'] for x in samples));mem['gpu_cooling_counts']=dict(collections.Counter(x['gpu_cooling'] for x in samples));mem['cpu_cooling_counts']=dict(collections.Counter(x['cpu_cooling'] for x in samples));out['memory']=mem
if selected:
 lo=float(selected[0]['presented'])-float(selected[0]['publication_age']);hi=float(selected[-1]['presented'])
 ns=[x for x in samples if lo<=x['time']<=hi]
 out['native_phase_memory']={k:dist(x[k] for x in ns if x[k] is not None) for k in ['VmRSS','MemAvailable','CmaFree','mali_allocated_pages','temperature_max_c']}
 out['native_phase_clock_counts']={k:dict(collections.Counter(x[k] for x in ns)) for k in ['gpu_hz','cpu_khz','gpu_cooling','cpu_cooling']}
 out['main_progress_gaps_native']=sum(lo<=x['end']<=hi for x in stalls)
 out['presentation_intervals_over_16_693ms']=sum(x>16.693 for x in intervals)
 out['audio_underruns_unit']='Missing stereo output sample frames, not callback events; see rr_audio.c audio_cb'
 out['main_fifo_blocked_total_ms']=sum(float(r['simulation_blocked_ms']) for r in selected)
 out['native_phase_span_seconds']=hi-lo
transition=next((float(r['presented'])-float(r['publication_age']) for r in rows if int(r['payload_native'])),None)
if transition:
 out['transition_memory']={label:{k:dist(x[k] for x in samples if lo<=x['time']-transition<=hi and x[k] is not None) for k in ['VmRSS','MemAvailable','CmaFree','mali_allocated_pages']} for label,lo,hi in [('before',-2,-.1),('after_steady',2,5)]}
for s in stalls:
 matches=[x for x in samples if s['start']-.1<=x['time']<=s['end']+.1];s['observations']=matches
 if len(matches)>1:
  s['vm_delta']={k:matches[-1]['vm'].get(k,0)-matches[0]['vm'].get(k,0) for k in matches[-1]['vm']};s['io_delta']={k:matches[-1]['io'].get(k,0)-matches[0]['io'].get(k,0) for k in matches[-1]['io']}
 main_d=any(t['state']=='D' and t['name']=='rr_p1' for x in matches for t in x['threads'].values());worker_d=any(t['state']=='D' and t['name']!='rr_p1' for x in matches for t in x['threads'].values())
 s['main_D_state_observed']=main_d;s['worker_D_state_observed']=worker_d
 s['interpretation']='Main D-state observed; inspect kernel wait/stack and VM/IO deltas' if main_d else 'FIFO backpressure dominates main wait; underlying consumer/preparation delay remains to be attributed' if s['fifo_blocked_ms']>s['gap_ms']*.7 else 'No main D-state captured; CPU/scheduling or service delay remains unproven'
out['stalls_over_50ms']=len(stalls);(b/'stalls.json').write_text(json.dumps(stalls,indent=2)+'\n');(b/'p1-summary.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps({k:out[k] for k in ['presented_pictures','ordered_once','repeated_vblanks_estimate','stalls_over_50ms','memory']},indent=2))
