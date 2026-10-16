#!/usr/bin/env python3
"""Timestamped low-impact observer; no pressure files, no invented gap coverage."""
import argparse,pathlib,re,json,math,statistics,collections,csv
p=argparse.ArgumentParser();p.add_argument('run',type=pathlib.Path);a=p.parse_args();b=a.run

def dist(v):
 s=sorted(v);n=len(s)
 return dict(n=n,median=statistics.median(s),p95=s[math.ceil(.95*n)-1],p99=s[math.ceil(.99*n)-1],worst=s[-1],minimum=s[0],mean=statistics.mean(s)) if n else None
samples=[];reads=[];lines=[];cpu=[];rss=[];footer=None;banner=None
def parse(lines):
 if not lines:return
 text=''.join(lines);time=float(text.splitlines()[0].split()[1]);files=dict(re.findall(r'FILE ([^\n]+)\n(.*?)END\n',text,re.S));bounds=[dict(start=float(x),end=float(y),path=z) for x,y,z in re.findall(r'READ_BOUNDS ([\d.]+) ([\d.]+) ([^\n]+)',text)];reads.extend(bounds)
 target=re.search(r'TARGET_PID (\d+)',text);pid=int(target[1]) if target else None
 s=dict(time=time,pid=pid,threads={},read_bounds=bounds)
 for k,v in files.items():
  if k.endswith('/stat') and '/task/' in k:
   m=re.match(r'(\d+) \((.*?)\) ([A-Za-z])',v)
   if m:s['threads'][m[1]]=dict(name=m[2],state=m[3],wchan=files.get(k[:-4]+'wchan','').strip(),stack=files.get(k[:-4]+'stack','').strip())
 for key,path,pattern in [('rss_kib',f'/proc/{pid}/status',r'VmRSS:\s+(\d+)'),('available_kib','/proc/meminfo',r'MemAvailable:\s+(\d+)'),('cma_kib','/proc/meminfo',r'CmaFree:\s+(\d+)'),('mali_pages','/sys/kernel/debug/mali0/gpu_memory',r'^\S+\s+(\d+)')]:
  m=re.search(pattern,files.get(path,''));s[key]=int(m[1]) if m else None
 s['gpu_hz']=int(files.get('/sys/class/devfreq/gpu/cur_freq','0').strip());s['cpu_khz']=[int(files.get(f'/sys/devices/system/cpu/cpu{i}/cpufreq/scaling_cur_freq','0').strip()) for i in range(4)];s['temperature_c']=max([int(v.strip())/1000 for k,v in files.items() if '/thermal_zone' in k and k.endswith('/temp')],default=0);s['cooling']={k:v.strip() for k,v in files.items() if '/cooling_device' in k};s['vm']=files.get('/proc/vmstat','');s['io']=files.get(f'/proc/{pid}/io','');samples.append(s)
 for c,r in re.findall(r'SAMPLER_CPU ([\d.]+) SAMPLER_MAXRSS_KiB (\d+)',text):cpu.append(float(c));rss.append(int(r))
with (b/'memory.txt').open() as f:
 for l in f:
  if l.startswith('[P1B-IO]'):footer=l
  if l.startswith('P1B_OBSERVER'):banner=l
  if l.startswith('SAMPLE '):parse(lines);lines=[l]
  elif lines:lines.append(l)
parse(lines)
assert footer and banner
io=dict(re.findall(r'(\w+)=([^ ]+)',footer));assert int(io['dropped_records'])==0 and int(io['errors'])==0 and int(io['fully_drained'])==1
assert 'SCHED_IDLE_result=0 nice=19' in banner
gaps=[dict(start=x['time'],end=y['time'],gap_ms=(y['time']-x['time'])*1000) for x,y in zip(samples,samples[1:]) if y['time']-x['time']>.2]
gamegaps=[]
for path in [b/'stage-gaps.csv',*b.glob('lap-*/stage-gaps.csv')]:
 if path.exists():gamegaps.extend(dict(start=float(r['start']),end=float(r['end']),stage=r['stage'],frame=int(r['frame'])) for r in csv.DictReader(path.open()) if r['native']=='True')
coincidences=[dict(observer=g,game=h) for g in gaps for h in gamegaps if g['start']<h['end'] and h['start']<g['end']]
# Full low-frequency samples and read timestamps retained; detailed game gap associations separate.
associations=[]
for g in gamegaps:associations.append(dict(gap=g,observations=[s for s in samples if g['start']-.1<=s['time']<=g['end']+.1]))
summary=dict(io=io,priority_banner=banner.strip(),samples=len(samples),sample_interval_ms=dist((y['time']-x['time'])*1000 for x,y in zip(samples,samples[1:])),slow_sample_gaps_over200ms=gaps,read_duration_ms=dist((r['end']-r['start'])*1000 for r in reads),slow_reads_over50ms=[r for r in reads if r['end']-r['start']>.05],observer_game_gap_coincidences=coincidences,sampler_cpu_seconds=cpu[-1] if cpu else None,sampler_cpu_core_equivalent=cpu[-1]/(samples[-1]['time']-samples[0]['time']) if cpu and len(samples)>1 else None,sampler_maxrss_kib=max(rss,default=None),gpu_hz_counts=dict(collections.Counter(s['gpu_hz'] for s in samples)),cpu_khz_counts=[dict(collections.Counter(s['cpu_khz'][i] for s in samples)) for i in range(4)],peak_temperature_c=max((s['temperature_c'] for s in samples),default=None),cooling_counts={k:dict(collections.Counter(s['cooling'].get(k) for s in samples)) for k in sorted(set(k for s in samples for k in s['cooling']))},pressure_reads=0,memory={k:dist(s[k] for s in samples if s[k] is not None) for k in ['rss_kib','available_kib','cma_kib','mali_pages']})
summary['samples_with_game_thread_records']=sum(bool(s['threads']) for s in samples)
summary['game_process_coverage_valid']=any(s['threads'] for s in samples)
summary['process_coverage_warning']=None if summary['game_process_coverage_valid'] else 'Observer followed supervisor, not replay child; process RSS, process I/O, thread states/stacks are not game evidence. Global VM/memory/clocks/thermal remain usable.'
(b/'observer-summary.json').write_text(json.dumps(summary,indent=2)+'\n');(b/'gap-attribution.json').write_text(json.dumps(associations,indent=2)+'\n')
# Compact derived timeline supports soak buckets without re-reading bulky kernel stacks.
with (b/'observer-timeline.csv').open('w') as f:
 keys=['time','pid','temperature_c','gpu_hz','cpu0_khz','cpu1_khz','cpu2_khz','cpu3_khz','cpu_cooling','gpu_cooling','rss_kib','available_kib','cma_kib','mali_pages'];w=csv.DictWriter(f,fieldnames=keys);w.writeheader()
 for s in samples:w.writerow({**{k:s[k] for k in ['time','pid','temperature_c','gpu_hz','rss_kib','available_kib','cma_kib','mali_pages']},**{f'cpu{i}_khz':s['cpu_khz'][i] for i in range(4)},'cpu_cooling':s['cooling'].get('/sys/class/thermal/cooling_device0/cur_state'),'gpu_cooling':s['cooling'].get('/sys/class/thermal/cooling_device1/cur_state')})
print(b.name,'samples',len(samples),'maxgap',summary['sample_interval_ms']['worst'] if samples else None,'gap_coincidences',len(coincidences),'CPU core',summary['sampler_cpu_core_equivalent'])
