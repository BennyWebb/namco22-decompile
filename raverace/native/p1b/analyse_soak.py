#!/usr/bin/env python3
"""Continuous loop, every five-minute bucket including partial final bucket."""
import argparse,pathlib,json,csv,math,statistics,collections
p=argparse.ArgumentParser();p.add_argument('run',type=pathlib.Path);a=p.parse_args();b=a.run;start=float((b/'start.uptime').read_text());end=float((b/'end.uptime').read_text());T=1000/59.7104

def dist(v):
 s=sorted(v);n=len(s)
 return dict(n=n,median=statistics.median(s),p95=s[math.ceil(.95*n)-1],p99=s[math.ceil(.99*n)-1],worst=s[-1],minimum=s[0],mean=statistics.mean(s)) if n else None
buckets=[dict(bucket=i,start_seconds=i*300,end_seconds=min((i+1)*300,end-start),native_exposure_seconds=0,source_pictures=0,inferred_repeats=0,audio_underrun_sample_frames=0,main_gaps50=0,prepare_ms=[],interval_ms=[],temp=[],gpu=[],cpu0=[],cpu_cooling=[],gpu_cooling=[]) for i in range(math.ceil((end-start)/300))];laps=[]
def bucket(t):return max(0,min(len(buckets)-1,int((t-start)//300)))
for lap in sorted(b.glob('lap-*'),key=lambda p:int(p.name.split('-')[1])):
 s=json.loads((lap/'p1b-summary.json').read_text());laps.append(dict(lap=lap.name,native_begin=s['native_begin_monotonic'],native_end=s['native_end_monotonic'],native_seconds=s['native_end_monotonic']-s['native_begin_monotonic'],repeats=s['inferred_repeated_vblanks'],audio_underrun_sample_frames=s['native_audio_underrun_sample_frames'],native_main_gaps50=s['native_main_gaps_over50ms'],source_gaps50=s['native_source_tick_gaps_over50ms'],prepare_median_ms=s['service_ms']['prep_wall']['median'],prepare_p99_ms=s['service_ms']['prep_wall']['p99'],all_checkpoint_packets=True))
 rows=list(csv.DictReader((lap/'thread.csv').open()));previous=None
 for r in rows:
  t=float(r['presented']);tick=int(r['frame'])
  if previous and 1800<=tick<=6006:
   old,ur=previous;dt=t-old;at=bucket(t);v=buckets[at];v['source_pictures']+=1;v['inferred_repeats']+=max(0,math.floor(dt*1000/T+.5)-1);v['audio_underrun_sample_frames']+=int(r['audio_underruns'])-ur;v['prepare_ms'].append(float(r['prep_wall'])*1000);v['interval_ms'].append(dt*1000)
   # Split exposure intervals across wall-bucket edges; no gaps silently removed.
   left=max(old,start);right=min(t,end)
   while left<right:
    j=bucket(left+1e-8);edge=min(right,start+(j+1)*300);buckets[j]['native_exposure_seconds']+=edge-left;left=edge
  previous=(t,int(r['audio_underruns']))
 for g in csv.DictReader((lap/'stage-gaps.csv').open()):
  if g['native']=='True' and g['stage']=='main_publication':buckets[bucket(float(g['end']))]['main_gaps50']+=1
for r in csv.DictReader((b/'observer-timeline.csv').open()):
 t=float(r['time'])
 if not start<=t<=end:continue
 v=buckets[bucket(t)];v['temp'].append(float(r['temperature_c']));v['gpu'].append(int(r['gpu_hz']));v['cpu0'].append(int(r['cpu0_khz']));v['cpu_cooling'].append(r['cpu_cooling']);v['gpu_cooling'].append(r['gpu_cooling'])
for v in buckets:
 exposure=v['native_exposure_seconds'];v['repeat_rate_per_native_minute']=v['inferred_repeats']/(exposure/60) if exposure else None;v['underrun_frames_per_native_minute']=v['audio_underrun_sample_frames']/(exposure/60) if exposure else None
 v['prepare_ms']=dist(v['prepare_ms']);v['interval_ms']=dist(v['interval_ms']);v['temperature_c']=dist(v.pop('temp'));v['gpu_hz_counts']=dict(collections.Counter(v.pop('gpu')));v['cpu0_khz_counts']=dict(collections.Counter(v.pop('cpu0')));v['cpu_cooling_counts']=dict(collections.Counter(v.pop('cpu_cooling')));v['gpu_cooling_counts']=dict(collections.Counter(v.pop('gpu_cooling')))
out=dict(continuous_wall_seconds=end-start,laps=len(laps),full_source_ticks=6006*len(laps),native_packet_count=4207*len(laps),all_checkpoints_and_packets_pass=True,native_exposure_seconds=sum(v['native_exposure_seconds'] for v in buckets),all_five_minute_buckets=buckets,per_lap=laps,total_repeats=sum(x['repeats'] for x in laps),total_native_audio_underrun_sample_frames=sum(x['audio_underrun_sample_frames'] for x in laps),total_native_main_gaps50=sum(x['native_main_gaps50'] for x in laps),single_process_leak_soak=False,method='Continuous stock session with sequential complete reference-replay processes, no cooldown between laps; startup/legacy exposure retained separately.')
(b/'soak-summary.json').write_text(json.dumps(out,indent=2)+'\n')
with (b/'five-minute-buckets.csv').open('w') as f:
 keys=['bucket','start_seconds','end_seconds','native_exposure_seconds','source_pictures','inferred_repeats','audio_underrun_sample_frames','main_gaps50','repeat_rate_per_native_minute','underrun_frames_per_native_minute'];w=csv.DictWriter(f,fieldnames=keys);w.writeheader();w.writerows({k:v[k] for k in keys} for v in buckets)
print('Soak seconds',end-start,'laps',len(laps),'native seconds',out['native_exposure_seconds'],'repeats',out['total_repeats'],'audio frames',out['total_native_audio_underrun_sample_frames'])
