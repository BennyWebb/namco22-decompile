#!/usr/bin/env python3
"""P2 retained full populations; no timing/validation mixing or outlier removal."""
import pathlib,json,csv,math,statistics,re,argparse,collections
p=argparse.ArgumentParser();p.add_argument('run',type=pathlib.Path);p.add_argument('--frequency',type=int,default=1512000);p.add_argument('--gpu-hz',type=int);p.add_argument('--soak-parent',type=pathlib.Path);a=p.parse_args();b=a.run;root=pathlib.Path(__file__).resolve().parents[3];T=1000/59.7104

def dist(v):
 s=sorted(v);n=len(s);return dict(n=n,mean=statistics.mean(s),median=statistics.median(s),p95=s[math.ceil(.95*n)-1],p99=s[math.ceil(.99*n)-1],worst=s[-1],minimum=s[0]) if n else None
r=list(csv.DictReader((b/'thread.csv').open()));ids=[int(x['frame']) for x in r];assert ids==list(range(1,6007));assert (b/'exit.txt').read_text().strip()=='0';ref=root/'raverace/benchmarks/native-p1-20261015/validation-q3';assert [json.loads(x) for x in (b/'states.jsonl').read_text().splitlines()]==[json.loads(x) for x in (ref/'states.jsonl').read_text().splitlines()]
log=(b/'run.txt').read_text()+((b/'stdout.txt').read_text() if (b/'stdout.txt').exists() else '');release=int(re.search(r'P1B-RELEASE\] completed_tick=(\d+)',log)[1]);io=dict(re.findall(r'(\w+)=([^ ]+)',re.search(r'\[P1B-IO\] (.*)',log)[1]));assert int(io['errors'])==int(io['dropped_records'])==0
if int(io['packets']):
 mode=re.search(r'\[C1E-LOAD\] mode=(rgba|index)',log)[1];adjusted=root/('raverace/native/generated/g2/colour-'+mode)/'command-sha256.csv';assert (b/'command-sha256.csv').read_bytes()==adjusted.read_bytes()
else:assert not (b/'image-sha256.csv').exists()
session=a.soak_parent or b
for name in ['clocks','production']:assert (session/(name+'.before')).read_bytes()==(session/(name+'.after')).read_bytes()
assert int((session/'temperature.start').read_text())<=55000 or len((session/'cooldown.txt').read_text().splitlines())>=37
byid={int(x['frame']):x for x in r};phase=[x for x in r if int(x['frame'])>=1800];assert all(int(x['payload_native'])==1 for x in phase)
dense=[x for x in phase if 3550<=int(x['frame'])<=3590]
metrics={}
for label,rows in [('native',phase),('dense',dense)]:
 metrics[label]={k:dist(float(x[k])*scale for x in rows) for k,scale in [('prep_wall',1000),('prep_cpu',1000),('native_compile_ms',1),('native_walk_ms',1),('simulation_ms',1),('cpu',1000),('draw_wall',1000),('swap_wall',1000),('publication_age',1000)]}
repeats=[];gaps=[];intervals=[];dense_intervals=[]
for x,y in zip(r,r[1:]):
 tick=int(y['frame'])
 if tick<1800:continue
 dt=(float(y['presented'])-float(x['presented']))*1000;intervals.append(dt)
 if 3550<=tick<=3590:dense_intervals.append(dt)
 repeat=max(0,math.floor(dt/T+.5)-1)
 if repeat:repeats.append(dict(tick=tick,repeats=repeat,interval_ms=dt,**{k:float(y[k]) for k in ['simulation_ms','simulation_blocked_ms','native_walk_ms','native_compile_ms','prep_wall','prep_cpu','draw_wall','swap_wall']}))
 for stage,key in [('main','publication_age'),('prepare','prep_queue_age'),('present','start')]:
  def at(row):
   pub=float(row['presented'])-float(row['publication_age'])
   return pub if stage=='main' else pub+float(row['prep_queue_age'])-float(row['prep_wall']) if stage=='prepare' else float(row['start'])
  delta=(at(y)-at(x))*1000
  if delta>50:gaps.append(dict(stage=stage,tick=tick,start=at(x),end=at(y),gap_ms=delta))
clock_begin=float(byid[1800]['presented'])-float(byid[1800]['publication_age'])-float(byid[1800]['simulation_ms'])/1000
clock=[];memory=[];allmemory=[];lines=[]
def parse(lines):
 if not lines:return
 text=''.join(lines);t=float(text.splitlines()[0].split()[1]);files=dict(re.findall(r'FILE ([^\n]+)\n(.*?)END\n',text,re.S))
 memtext=files.get('/proc/meminfo','');rssfiles=[v for k,v in files.items() if k.startswith('/proc/') and k.endswith('/status')];rss=rssfiles[0] if rssfiles else '';mali=files.get('/sys/kernel/debug/mali0/gpu_memory','');
 def number(text,key):
  m=re.search(r'^'+key+r':?\s+(\d+)',text,re.M);return int(m[1]) if m else None
 record=dict(time=t,RSS_KiB=number(rss,'VmRSS'),RSS_HWM_KiB=number(rss,'VmHWM'),MemAvailable_KiB=number(memtext,'MemAvailable'),CmaFree_KiB=number(memtext,'CmaFree'),Mali_pages=number(mali,'mali0'));allmemory.append(record)
 if not clock_begin<=t<=float(byid[6006]['presented']):return
 memory.append(record)
 cpu=[int(files.get(f'/sys/devices/system/cpu/cpu{i}/cpufreq/scaling_cur_freq','0')) for i in range(4)];cool={k:int(v.strip()) for k,v in files.items() if '/cooling_device' in k};temp=max(int(v.strip())/1000 for k,v in files.items() if '/thermal_zone' in k and k.endswith('/temp'));clock.append(dict(time=t,cpu=cpu,cooling=cool,temperature_c=temp,gpu_hz=int(files.get('/sys/class/devfreq/gpu/cur_freq','0'))))
for line in (session/'memory.txt').open():
 if line.startswith('SAMPLE '):parse(lines);lines=[line]
 elif lines:lines.append(line)
 if line.startswith('[P1B-IO]'): observer_footer=line
parse(lines);assert clock
observer_io=dict(re.findall(r'(\w+)=([^ ]+)',observer_footer));assert int(observer_io['errors'])==int(observer_io['dropped_records'])==0 and observer_io['fully_drained'].strip()=='1'
flags=[c for c in clock if any(f!=a.frequency for f in c['cpu']) or c['cooling'].get('/sys/class/thermal/cooling_device0/cur_state',0)!=0 or (a.gpu_hz is not None and c['gpu_hz']!=a.gpu_hz)]
out=dict(clock_begin_monotonic=clock_begin,native_begin_monotonic=float(byid[1799]['presented']),native_end_monotonic=float(byid[6006]['presented']),full_ticks=6006,checkpoints=21,packet_receipts=int(io['packets']),release_tick=release,settled_underrun_definition='counter(6006)-counter(actual legacy-release tick); matched1800..6006 also reported separately',native_underrun_frames=int(byid[6006]['audio_underruns'])-int(byid[1799]['audio_underruns']),settled_underrun_frames=int(byid[6006]['audio_underruns'])-int(byid[release]['audio_underruns']),metrics_ms=metrics,swap_interval_ms=dist(intervals),dense_swap_interval_ms=dist(dense_intervals),inferred_repeats=sum(x['repeats'] for x in repeats),repeat_events=repeats,gaps_over50ms=gaps,main_gaps50=sum(x['stage']=='main' for x in gaps),clock_samples=len(clock),clock_sample_interval_ms=dist((y['time']-x['time'])*1000 for x,y in zip(clock,clock[1:])),clock_flags=flags,clock_flagged=bool(flags),peak_native_temperature_c=max(c['temperature_c'] for c in clock),cpu_khz_counts=[dict(collections.Counter(c['cpu'][i] for c in clock)) for i in range(4)],gpu_hz_counts=dict(collections.Counter(c['gpu_hz'] for c in clock)),cooling_counts={k:dict(collections.Counter(c['cooling'].get(k) for c in clock)) for k in clock[0]['cooling']},log_drops=int(io['dropped_records']))
out['memory_whole_run']={k:dist(x[k] for x in allmemory if x[k] is not None) for k in ['RSS_KiB','RSS_HWM_KiB','MemAvailable_KiB','CmaFree_KiB','Mali_pages']}
out['memory_settled']={k:dist(x[k] for x in memory if x[k] is not None and x['time']>=float(byid[release]['presented'])) for k in ['RSS_KiB','RSS_HWM_KiB','MemAvailable_KiB','CmaFree_KiB','Mali_pages']}
out['memory_native']={k:dist(x[k] for x in memory if x[k] is not None) for k in ['RSS_KiB','RSS_HWM_KiB','MemAvailable_KiB','CmaFree_KiB','Mali_pages']}
frames=[dict(tick=int(m[1]),upload_ms=float(m[2]),upload_cpu_ms=float(m[3]),groups=int(m[4]),bytes=int(m[5]),render_ms=float(m[6]),fence=int(m[7])) for m in re.finditer(r'C1E_FRAME tick=(\d+) upload_ms=([\d.]+) upload_cpu_ms=([\d.]+) groups=(\d+) bytes=(\d+) render_ms=([\d.]+) fence=(\d+)',log) if int(m[1])>=1800]
out['c1e_frames']={k:dist(x[k] for x in frames) for k in ['upload_ms','upload_cpu_ms','groups','bytes','render_ms']};out['c1e_frame_count']=len(frames);assert len(frames)==4207
extra=[(float(m[1]),float(m[2]),float(m[3])) for m in re.finditer(r'world_ms=([\d.]+) hud_ms=([\d.]+) wait_ms=([\d.]+)',log)];out['fence_components_ms']={k:dist(x[i] for x in extra) for i,k in enumerate(['world_submit','hud_submit','completion_wait'])}
out['load_receipts']=re.findall(r'\[C1E-LOAD\] (.*)',log)
(b/'summary.json').write_text(json.dumps(out,indent=2)+'\n');(b/'repeat-events.json').write_text(json.dumps(repeats,indent=2)+'\n');(b/'stage-gaps.csv').write_text('stage,frame,native,start,end,gap_ms\n'+''.join(f"{g['stage']},{g['tick']},True,{g['start']},{g['end']},{g['gap_ms']}\n" for g in gaps));print(b.name,'dense p99',metrics['dense']['prep_wall']['p99'],'underruns',out['native_underrun_frames'],out['settled_underrun_frames'],'repeats',out['inferred_repeats'],'clockflag',bool(flags))
