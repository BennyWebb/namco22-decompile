#!/usr/bin/env python3
"""Stage1 distributions and /proc observations; no runtime policy changes."""
import pathlib,argparse,csv,json,re,math,collections,numpy as np
p=argparse.ArgumentParser();p.add_argument('run',type=pathlib.Path);a=p.parse_args();b=a.run

def stats(v):
 v=np.asarray(v,dtype=float);s=np.sort(v);n=len(s)
 if not n:return None
 return dict(n=n,median=float(np.median(s)),p95=float(s[math.ceil(n*.95)-1]),p99=float(s[math.ceil(n*.99)-1]),worst=float(s[-1]),mean=float(s.mean()),over_deadline=int((v>16.693359375).sum()))
out={}
if (b/'audit.csv').exists():
 rows=list(csv.DictReader((b/'audit.csv').open()));assert [int(r['tick']) for r in rows]==list(range(1800,6007));out['audit']={k:stats([float(r[k]) for r in rows]) for k in rows[0] if k.endswith('_ms')};out['paired_frontend_overhead_ms']=stats([float(r['walk_cpu_ms'])+float(r['compile_cpu_ms'])-float(r['original_cpu_ms']) for r in rows]);out['native_prep_cpu_ms']=stats([float(r['walk_cpu_ms'])+float(r['compile_cpu_ms']) for r in rows]);out['order']={order:{k:stats([float(r[k]) for r in rows if r['original_first']==order]) for k in ['original_cpu_ms','walk_cpu_ms','compile_cpu_ms']} for order in ['0','1']};out['cores']={k:dict(collections.Counter(r[k] for r in rows)) for k in ['native_core','original_core','native_end_core','original_end_core']};out['native_migrations_between_probes']=sum(r['native_core']!=r['native_end_core'] for r in rows)
 parts=list(rows[0])[23:];totals={k:sum(float(r[k]) for r in rows) for k in parts};total=sum(totals.values());out['compiler_bucket_time_share']={k:100*v/total if total else 0 for k,v in totals.items()};out['counter_reconciliation_ms']=stats([sum(float(r[k]) for k in parts)-float(r['compile_wall_ms']) for r in rows]) if total>0 else None;out['source_commands']=sum(int(r['n']) for r in rows);out['max_instances']=max(int(r['instances']) for r in rows)
if (b/'thread.csv').exists():
 rows=list(csv.DictReader((b/'thread.csv').open()));out['production_thread']={}
 for phase,rs in [('all',rows),('race',[r for r in rows if int(r['frame'])>=1800])]:
  result={k:stats([1000*float(r[k]) for r in rs]) for k in ['cpu','scene_wall','prep_cpu','hud_cpu','prep_wall','snapshot_copy_wall','snapshot_queue_wait','prepared_wait','publication_age','draw_wall','swap_wall'] if k in rs[0]};result['presents']=len(rs);result['render_core_counts']=dict(collections.Counter(r['core'] for r in rs));result['world_prep_cpu_ms']=stats([1000*(float(r['prep_cpu'])-float(r['hud_cpu'])) for r in rs]);result['presentation_interval_ms']=stats([1000*(float(rs[i]['presented'])-float(rs[i-1]['presented'])) for i in range(1,len(rs))]);out['production_thread'][phase]=result
if (b/'tasks.txt').exists():
 text=(b/'tasks.txt').read_text();hz=int(re.search(r'HZ=(\d+)',text)[1]);threads={};samples=[]
 for block in text.split('SAMPLE ')[1:]:
  try:t=float(block.splitlines()[0])
  except ValueError:continue
  sample={'time':t}
  for k in ['VmRSS','VmHWM','MemAvailable','CmaFree']:
   m=re.search(r'^'+k+r':\s*(\d+)',block,re.M)
   if m:sample[k]=int(m[1])
  for l in block.splitlines():
   m=re.match(r'^TASK (\d+) \((.*)\) (.*)$',l)
   if not m:continue
   tid,name,rest=m.groups();f=rest.split()
   if len(f)<37:continue
   # rest starts stat field3; utime14/stime15/processor39.
   ut,st,core=int(f[11]),int(f[12]),int(f[36]);v=threads.setdefault(tid,{'name':name,'first':t,'first_ticks':ut+st,'last':t,'last_ticks':ut+st,'cores':collections.Counter(),'samples':0});v['last']=t;v['last_ticks']=ut+st;v['cores'][core]+=1;v['samples']+=1
  samples.append(sample)
 out['thread_observations']={tid:dict(name=v['name'],sampled_core_counts=dict(v['cores']),samples=v['samples'],cpu_seconds=(v['last_ticks']-v['first_ticks'])/hz,elapsed_seconds=v['last']-v['first'],average_core_utilization=(v['last_ticks']-v['first_ticks'])/hz/max(v['last']-v['first'],1e-9)) for tid,v in threads.items()};out['memory']=dict(samples=len(samples),peak_rss_kib=max(r.get('VmRSS',0) for r in samples),min_available_kib=min(r.get('MemAvailable',1e20) for r in samples));(b/'proc-samples.json').write_text(json.dumps(samples,indent=2)+'\n')
if (b/'tasks.txt').exists() and (b/'ticks.csv').exists():
 ticks={int(r[0]):float(r[1]) for r in csv.reader((b/'ticks.csv').open()) if r and r[0].isdigit()};start=ticks.get(1800);end=ticks.get(6006);threads={}
 if start and end:
  for block in text.split('SAMPLE ')[1:]:
   try:t=float(block.splitlines()[0])
   except ValueError:continue
   if not start<=t<=end:continue
   for l in block.splitlines():
    m=re.match(r'^TASK (\d+) \((.*)\) (.*)$',l)
    if not m:continue
    tid,name,rest=m.groups();f=rest.split()
    if len(f)<37:continue
    ut,st,core=int(f[11]),int(f[12]),int(f[36]);v=threads.setdefault(tid,{'name':name,'first':t,'first_ticks':ut+st,'last':t,'last_ticks':ut+st,'cores':collections.Counter(),'samples':0,'priority':f[15],'nice':f[16]});v['last']=t;v['last_ticks']=ut+st;v['cores'][core]+=1;v['samples']+=1
  out['race_thread_observations']={tid:dict(name=v['name'],sampled_core_counts=dict(v['cores']),samples=v['samples'],cpu_seconds=(v['last_ticks']-v['first_ticks'])/hz,elapsed_seconds=v['last']-v['first'],average_core_utilization=(v['last_ticks']-v['first_ticks'])/hz/max(v['last']-v['first'],1e-9),priority=v['priority'],nice=v['nice']) for tid,v in threads.items()}
(b/'summary.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
