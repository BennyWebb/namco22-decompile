"""Supplement the existing benchmark summary with burst/lifetime/memory data."""
import csv,json,statistics,re,sys
from pathlib import Path
from collections import defaultdict
root=Path(sys.argv[1]);summary=json.loads((root/'summary.json').read_text());runs=[]
for run in summary['runs']:
 name=run['name'];rows=list(csv.DictReader((root/(name+'.presentations.csv')).open()))
 selected=[(i,r) for i,r in enumerate(rows) if 1960<=int(r['frame'])<=1970]
 i,r=max(selected,key=lambda pair:int(pair[1]['misses']))
 burst={k:float(r[k])*1000 for k in ['upload_wall','eviction_wall','decode_wall','submission_wall','cpu','publication_age']}
 burst.update(tick=int(r['frame']),misses=int(r['misses']),interval_ms=(float(r['presented'])-float(rows[i-1]['presented']))*1000)
 atlas=run['world_atlas'];lifetime={k:int(atlas[k]) for k in ['physical_bytes','peak_bytes','allocations','uploads','hole_reuses'] if k in atlas}
 text=(root/(name+'.log')).read_text();match=re.search(r'\[WORLD-RETAIN\] enabled=(\d+) idle_reclaims=(\d+) blocking_fence_polls=(\d+)',text)
 if match:lifetime.update(idle_reclaims=int(match[2]),blocking_fence_polls=int(match[3]))
 if 'physical_bytes' in lifetime:lifetime['logical_payload_occupancy_percent']=int(rows[-1]['texture_bytes'])/lifetime['physical_bytes']*100
 snapshots=[];current={}
 for line in (root/(name+'.monitor')).read_text().splitlines():
  if line.startswith('SAMPLE '):
   if current:snapshots.append(current)
   current={'time':float(line.split()[1])}
  elif line.startswith(('MemAvailable:','MemFree:','Shmem:','CmaFree:')):current[line.split(':')[0]]=int(line.split()[1])
 if current:snapshots.append(current)
 burst_time=float(r['presented']);near=[s for s in snapshots if abs(s['time']-burst_time)<=3]
 runs.append({'name':name,'variant':run['variant'],'pair':run['pair'],'burst':burst,'lifetime':lifetime,'memory_near_burst':near})
medians={}
for variant in ['baseline','original','candidate']:
 subset=[r for r in runs if r['variant']==variant];burst={k:statistics.median(r['burst'][k] for r in subset) for k in subset[0]['burst']}
 lifetime={k:statistics.median(r['lifetime'][k] for r in subset) for k in subset[0]['lifetime']}
 medians[variant]={'burst':burst,'lifetime':lifetime}
(root/'details.json').write_text(json.dumps({'runs':runs,'medians':medians},indent=2)+'\n')
print(json.dumps(medians,indent=2))
