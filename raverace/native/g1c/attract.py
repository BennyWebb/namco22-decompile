#!/usr/bin/env python3
"""Retain every attract picture and sample; no reference-window assumptions."""
import pathlib,csv,json,re,collections,math,statistics,sys
r=pathlib.Path(sys.argv[1]); rows=list(csv.DictReader((r/'thread.csv').open())); log=(r/'run.txt').read_text(); stdout=(r/'stdout.txt').read_text(); assert [int(x['frame']) for x in rows]==list(range(1,len(rows)+1))
def dist(v):
 s=sorted(v);n=len(s);return dict(n=n,median=statistics.median(s),p95=s[math.ceil(.95*n)-1],p99=s[math.ceil(.99*n)-1],worst=s[-1],minimum=s[0]) if n else None
observer_receipts=re.findall(r'\[P1B-IO\] ([^\n]+)',(r/'memory.txt').read_text());assert observer_receipts
for receipt in observer_receipts:
 d=dict(re.findall(r'(\w+)=([^ ]+)',receipt));assert int(d['errors'])==int(d['dropped_records'])==0 and d['fully_drained'].strip()=='1'
samples=[]
for part in (r/'memory.txt').read_text().split('SAMPLE ')[1:]:
 f=dict(re.findall(r'FILE ([^\n]+)\n(.*?)END\n',part,re.S)); out={'time':float(part.split()[0])}
 for key,text in f.items():
  if key.endswith('scaling_cur_freq') or key.endswith('/cur_state') or key.endswith('/temp') or key=='/sys/class/devfreq/gpu/cur_freq':out[key]=int(text.strip())
  if key=='/proc/meminfo' or key.endswith('/status'):
   for name in ['VmRSS','VmHWM','MemAvailable','CmaFree']:
    m=re.search(r'^'+name+r':\s+(\d+)',text,re.M)
    if m:out[name]=int(m[1])
  if key=='/sys/kernel/debug/mali0/gpu_memory':
   m=re.search(r'^mali0\s+(\d+)',text,re.M)
   if m:out['Mali_pages']=int(m[1])
 samples.append(out)
keys=set().union(*(x.keys() for x in samples)); memory={k:dist(x[k] for x in samples if k in x) for k in ['VmRSS','VmHWM','MemAvailable','CmaFree','Mali_pages']}; clocks={k:dict(collections.Counter(x[k] for x in samples if k in x)) for k in keys if 'freq' in k or 'cur_state' in k};temps={k:dist(x[k]/1000 for x in samples if k in x) for k in keys if k.endswith('/temp')}
io=re.findall(r'\[P1B-IO\] ([^\n]+)',log); assert io, 'missing writer drain receipt'
for line in io:
 d=dict(re.findall(r'(\w+)=([^ ]+)',line));assert int(d['errors'])==int(d['dropped_records'])==0,line
assert (r/'restoration-failed.txt').read_text().strip()=='0';assert (r/'exit.txt').read_text().strip() in ['0','124'];assert (r/'production.before').read_bytes()==(r/'production.after').read_bytes();assert (r/'clocks.before').read_bytes()==(r/'clocks.after').read_bytes()
frames=re.findall(r'C1E_FRAME tick=(\d+)',stdout); cov=json.loads((r/'coverage-summary.json').read_text());assert len(frames)==cov['native'], (len(frames),cov['native'])
out=dict(pictures=len(rows),elapsed_present_seconds=float(rows[-1]['presented'])-float(rows[0]['presented']),exit_code=int((r/'exit.txt').read_text()),memory_KiB_except_Mali_pages=memory,clock_counts=clocks,temperature_c=temps,sampler_intervals_ms=dist((y['time']-x['time'])*1000 for x,y in zip(samples,samples[1:])),writer_receipts=io,observer_receipts=observer_receipts,trace_frames=len(frames),load_receipts=re.findall(r'\[C1E-LOAD\] (.*)',log),start_temperature_mC=(r/'temperature.start').read_text().strip(),native_stage_ms={k:dist(float(x[k])*scale for x in rows if x['payload_native']=='1') for k,scale in [('prep_wall',1000),('native_compile_ms',1),('native_walk_ms',1),('simulation_ms',1),('draw_wall',1000),('swap_wall',1000),('publication_age',1000)]})
(r/'attract-summary.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
