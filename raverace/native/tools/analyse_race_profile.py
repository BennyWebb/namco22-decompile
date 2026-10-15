#!/usr/bin/env python3
"""Controlled diagnostic attribution; omissions are never headroom acceptance."""
import argparse,pathlib,json,re,math,statistics
p=argparse.ArgumentParser();p.add_argument('logs',type=pathlib.Path,nargs='+');p.add_argument('--out',type=pathlib.Path,required=True);a=p.parse_args()
def stats(v):
 s=sorted(v);return dict(median=statistics.median(s),p95=s[math.ceil(.95*len(s))-1],p99=s[math.ceil(.99*len(s))-1],worst=max(s))
def fields(l):return {k:float(v) for k,v in re.findall(r'(\w+)=([\d.]+)',l)}
runs=[]
for path in a.logs:
 text=path.read_text();rows=[fields(l) for l in text.splitlines() if l.startswith('FRAME ')];prof=[fields(l) for l in text.splitlines() if l.startswith('PROFILE ')][-3840:]
 assert len(rows)==3840 and [int(r['tick']) for r in rows]==list(range(1800,5640))
 assert sum(r['primitives'] for r in rows)==5152196 and sum(r['visible'] for r in rows)==3677980
 derived={name:stats([fn(r) for r in rows]) for name,fn in dict(world_submit_plus_sync_ms=lambda r:r['world_submit_ms']+r['world_sync_ms'],hud_submit_plus_finish_ms=lambda r:r['hud_submit_ms']+r['finish_ms']).items()}
 stages={k:stats([r[k] for r in rows]) for k in rows[0] if k.endswith('_ms')}
 ps={k:stats([r[k] for r in prof]) for k in prof[0] if k.endswith('_ms')} if prof else {}
 if prof:assert [int(r['tick']) for r in prof]==list(range(1800,5640))
 runs.append(dict(file=str(path),frames=len(rows),mode=int(prof[0]['mode']) if prof else 0,gpu=next(l for l in text.splitlines() if l.startswith('GPU ')),stages=stages,derived=derived,thread_cpu=ps,over_deadline=sum(r['total_ms']>16.693 for r in rows),peak_rss_kib=int(re.search(r'RSS max_kib=(\d+)',text)[1])))
a.out.write_text(json.dumps(dict(scope='Full-window diagnostic attribution. Split fences perturb tile scheduling. World raster discard and skipped draw controls omit rendering and cannot satisfy headroom/visual gates. CPU spans include application and driver work; wall minus CPU is not exclusive GPU time.',runs=runs),indent=2)+'\n')
for r in runs:print(r['file'],r['derived'],r['thread_cpu'])
