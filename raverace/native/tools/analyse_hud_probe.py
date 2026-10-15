#!/usr/bin/env python3
"""Per-picture pen/final attribution and paired omission controls, not acceptance."""
import argparse,pathlib,json,re,math,statistics
p=argparse.ArgumentParser();p.add_argument('logs',type=pathlib.Path,nargs='+');p.add_argument('--out',type=pathlib.Path,required=True);a=p.parse_args()
def fields(l):return {k:float(v) for k,v in re.findall(r'(\w+)=([\d.]+)',l)}
def stats(v):
 s=sorted(v);return dict(median=statistics.median(s),p95=s[math.ceil(.95*len(s))-1],p99=s[math.ceil(.99*len(s))-1],worst=max(s))
runs=[];source=[]
for path in a.logs:
 text=path.read_text();rows=[fields(l) for l in text.splitlines() if l.startswith('FRAME ')];hud=[fields(l) for l in text.splitlines() if l.startswith('HUDPROFILE ')][-3840:]
 assert len(rows)==len(hud)==3840 and [int(r['tick']) for r in rows]==[int(r['tick']) for r in hud]==list(range(1800,5640));assert sum(r['primitives'] for r in rows)==5152196
 world=[r['world_submit_ms']+r['world_sync_ms'] for r in rows];pen=[r['pen_wall_ms'] for r in hud];final=[r['hud_submit_ms']+r['finish_ms']-h['pen_wall_ms'] for r,h in zip(rows,hud)];assert min(final)>0
 source.append(final)
 runs.append(dict(file=str(path),control=int(hud[0]['control']),frames=3840,world=stats(world),pen=stats(pen),pen_cpu=stats([h['pen_cpu_ms'] for h in hud]),final_composition=stats(final),total=stats([r['total_ms'] for r in rows]),peak_rss_kib=int(re.search(r'RSS max_kib=(\d+)',text)[1])))
for i in range(1,len(runs)):runs[i]['paired_final_reduction_from_first']=stats([x-y for x,y in zip(source[0],source[i])])
a.out.write_text(json.dumps(dict(scope='Controlled serial fences perturb scheduling; pen includes preparation/upload/render completion, final includes palette/LUT uploads and fullscreen composition/completion. Shadow/LUT omissions are diagnostic only. Separate stock-governor processes; paired differences are sensitivities, not exclusive GPU times.',runs=runs),indent=2)+'\n')
for r in runs:print(r)
