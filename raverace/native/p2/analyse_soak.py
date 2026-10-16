#!/usr/bin/env python3
"""All laps and wall-time buckets; explicit child coverage and reclaim evidence."""
import argparse,pathlib,subprocess,json,csv,math,statistics,collections,re
p=argparse.ArgumentParser();p.add_argument('run',type=pathlib.Path);p.add_argument('--frequency',type=int,default=1512000);a=p.parse_args();b=a.run;here=pathlib.Path(__file__).resolve().parent
for lap in sorted(b.glob('lap-*'),key=lambda x:int(x.name.split('-')[1])):
 subprocess.run(['python3',str(here/'analyse.py'),str(lap),'--frequency',str(a.frequency),'--soak-parent',str(b)],check=True)
subprocess.run(['python3',str(here.parent/'p1b/analyse_observer.py'),str(b)],check=True)
start=float((b/'start.uptime').read_text());end=float((b/'end.uptime').read_text());assert end-start>=1800
T=1000/59.7104

def dist(v):
 s=sorted(v);n=len(s);return dict(n=n,median=statistics.median(s),p95=s[math.ceil(.95*n)-1],p99=s[math.ceil(.99*n)-1],worst=s[-1],minimum=s[0]) if n else None
buckets=[dict(bucket=i,start_seconds=i*300,end_seconds=min((i+1)*300,end-start),native_seconds=0,pictures=0,repeats=0,native_underrun_frames=0,settled_underrun_frames=0,main_gaps50=0,prepare=[],interval=[],temperature=[],cpu=[[] for _ in range(4)],gpu=[],cpu_cooling=[],gpu_cooling=[]) for i in range(math.ceil((end-start)/300))]
def bucket(t):return max(0,min(len(buckets)-1,int((t-start)//300)))
laps=[]
for lap in sorted(b.glob('lap-*'),key=lambda x:int(x.name.split('-')[1])):
 s=json.loads((lap/'p2-summary.json').read_text());rows=list(csv.DictReader((lap/'thread.csv').open()));pid_samples=[]
 laps.append(dict(lap=lap.name,**{k:s[k] for k in ['native_begin_monotonic','native_end_monotonic','inferred_repeats','native_underrun_frames','settled_underrun_frames','main_gaps50','clock_flagged','peak_native_temperature_c','release_tick']},dense_prepare_p99_ms=s['metrics_ms']['dense']['prep_wall']['p99']))
 for old,r in zip(rows,rows[1:]):
  tick=int(r['frame']);t=float(r['presented'])
  if tick<1800:continue
  v=buckets[bucket(t)];dt=t-float(old['presented']);ur=int(r['audio_underruns'])-int(old['audio_underruns']);v['pictures']+=1;v['repeats']+=max(0,math.floor(dt*1000/T+.5)-1);v['native_underrun_frames']+=ur
  if tick>s['release_tick']:v['settled_underrun_frames']+=ur
  v['prepare'].append(float(r['prep_wall'])*1000);v['interval'].append(dt*1000)
  left=max(float(old['presented']),start);right=min(t,end)
  while left<right:
   j=bucket(left+1e-8);edge=min(right,start+(j+1)*300);buckets[j]['native_seconds']+=edge-left;left=edge
 for g in s['gaps_over50ms']:
  if g['stage']=='main':buckets[bucket(g['end'])]['main_gaps50']+=1
# Keep process-targeted status/IO/VM deltas per lap; observed counters are not causal proof.
samples=[];lines=[]
def parse(lines):
 if not lines:return
 text=''.join(lines);time=float(text.splitlines()[0].split()[1]);files=dict(re.findall(r'FILE ([^\n]+)\n(.*?)END\n',text,re.S));pid=int(re.search(r'TARGET_PID (\d+)',text)[1]);status=files.get(f'/proc/{pid}/status','')
 def field(name):
  m=re.search(name+r':\s+(\d+)',status);return int(m[1]) if m else None
 mem=files.get('/proc/meminfo','')
 def memfield(name):
  m=re.search(name+r':\s+(\d+)',mem);return int(m[1]) if m else None
 samples.append(dict(time=time,pid=pid,rss_kib=field('VmRSS'),hwm_kib=field('VmHWM'),available_kib=memfield('MemAvailable'),cma_free_kib=memfield('CmaFree'),vm={k:int(v) for k,v in re.findall(r'^(\w+) (\d+)$',files.get('/proc/vmstat',''),re.M)},io={k:int(v) for k,v in re.findall(r'^(\w+): (\d+)$',files.get(f'/proc/{pid}/io',''),re.M)}))
for line in (b/'memory.txt').open():
 if line.startswith('SAMPLE '):parse(lines);lines=[line]
 elif lines:lines.append(line)
parse(lines)
for lap,info in zip(sorted(b.glob('lap-*'),key=lambda x:int(x.name.split('-')[1])),laps):
 lo=float((lap/'start.uptime').read_text());hi=float((lap/'end.uptime').read_text());ss=[x for x in samples if lo<=x['time']<=hi and x['pid']>0 and x['rss_kib'] is not None];assert ss, 'Missing per-lap process coverage'
 info['observed_pids']=sorted(set(x['pid'] for x in ss));info['observed_peak_rss_kib']=max(x['rss_kib'] for x in ss);info['observed_peak_hwm_kib']=max(x['hwm_kib'] for x in ss);info['minimum_available_kib']=min(x['available_kib'] for x in ss if x['available_kib'] is not None);info['minimum_cma_free_kib']=min((x['cma_free_kib'] for x in ss if x['cma_free_kib'] is not None),default=None);info['vm_delta']={k:ss[-1]['vm'].get(k,0)-ss[0]['vm'].get(k,0) for k in ss[0]['vm']};info['process_io_delta']={k:ss[-1]['io'].get(k,0)-ss[0]['io'].get(k,0) for k in ss[0]['io']}
for r in csv.DictReader((b/'observer-timeline.csv').open()):
 t=float(r['time'])
 if not start<=t<=end:continue
 v=buckets[bucket(t)];v['temperature'].append(float(r['temperature_c']));v['gpu'].append(int(r['gpu_hz']));v['cpu_cooling'].append(int(r['cpu_cooling']));v['gpu_cooling'].append(int(r['gpu_cooling']))
 for i in range(4):v['cpu'][i].append(int(r[f'cpu{i}_khz']))
for v in buckets:
 exposure=v['native_seconds'];v['repeats_per_native_minute']=v['repeats']/(exposure/60) if exposure else None;v['underrun_frames_per_native_minute']=v['native_underrun_frames']/(exposure/60) if exposure else None
 for key in ['prepare','interval','temperature']:v[key]=dist(v[key])
 for key in ['gpu','cpu_cooling','gpu_cooling']:v[key+'_counts']=dict(collections.Counter(v.pop(key)))
 v['cpu_khz_counts']=[dict(collections.Counter(x)) for x in v.pop('cpu')]
out=dict(wall_seconds=end-start,laps=laps,buckets=buckets,single_process=False,reason='Lifted reset entry never returns; replay completion exits process. No safe in-process authoritative reset API exists; no simulation reset rewrite authorised.',clock_flagged=any(x['clock_flagged'] for x in laps) or any(any(int(k)>0 and v>0 for k,v in z['cpu_cooling_counts'].items()) for z in buckets),gpu_cooling_active=any(any(int(k)>0 and v>0 for k,v in z['gpu_cooling_counts'].items()) for z in buckets),total_repeats=sum(x['inferred_repeats'] for x in laps),total_settled_underrun_frames=sum(x['settled_underrun_frames'] for x in laps),total_main_gaps50=sum(x['main_gaps50'] for x in laps))
(b/'soak-summary.json').write_text(json.dumps(out,indent=2)+'\n');print('SOAK',b.name,out['wall_seconds'],len(laps),'flagged',out['clock_flagged'],'repeats',out['total_repeats'],'settled audio',out['total_settled_underrun_frames'],'gaps',out['total_main_gaps50'])
