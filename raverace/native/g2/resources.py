#!/usr/bin/env python3
"""Attribute 10 Hz resource samples to the latest presented picture/selector."""
import pathlib,csv,re,json,bisect,collections,sys
r=pathlib.Path(sys.argv[1]);rows=list(csv.DictReader((r/'thread.csv').open()));cov=json.loads((r/'picture-coverage.json').read_text());times=[float(x['presented']) for x in rows];groups=collections.defaultdict(list)
for chunk in (r/'memory.txt').read_text().split('SAMPLE ')[1:]:
 t=float(chunk.split()[0]);idx=bisect.bisect_right(times,t)-1
 if idx<0 or idx>=len(rows) or t>times[-1]:continue
 files=dict(re.findall(r'FILE ([^\n]+)\n(.*?)END\n',chunk,re.S));record={'time':t}
 for k,v in files.items():
  if k.endswith('/temp') or k.endswith('cur_freq') or k.endswith('/cur_state'):record[k]=int(v.strip())
  for name in ['VmRSS','VmHWM','MemAvailable','CmaFree']:
   m=re.search(r'^'+name+r':\s+(\d+)',v,re.M)
   if m:record[name]=int(m[1])
  if k.endswith('/gpu_memory'):
   m=re.search(r'^mali0\s+(\d+)',v,re.M)
   if m:record['Mali_pages']=int(m[1])
 groups[cov[idx]['course']].append(record)
out={}
for group,samples in sorted(groups.items()):
 keys=set().union(*(s.keys() for s in samples));out[group]=dict(samples=len(samples),peak_RSS_MiB=max(s.get('VmRSS',0) for s in samples)/1024,peak_Mali_MiB=max(s.get('Mali_pages',0) for s in samples)/256,min_MemAvailable_MiB=min(s['MemAvailable'] for s in samples if 'MemAvailable' in s)/1024,min_CmaFree_MiB=min(s['CmaFree'] for s in samples if 'CmaFree' in s)/1024,peak_temperature_c=max(s[k]/1000 for s in samples for k in s if k.endswith('/temp')),clock_cooling_counts={k:dict(collections.Counter(s[k] for s in samples if k in s)) for k in keys if k.endswith('cur_freq') or k.endswith('/cur_state')})
(r/'selector-resources.json').write_text(json.dumps(dict(method='Each sample labelled by latest presented picture (up to one picture/queue latency around transitions); peaks/minima retain all in-run samples',per_selector=out),indent=2)+'\n');print(r.name,'resource selectors',list(out))
