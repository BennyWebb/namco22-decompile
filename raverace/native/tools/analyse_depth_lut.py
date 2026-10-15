#!/usr/bin/env python3
"""Full-window component and inter-picture attribution; no frame removal."""
import argparse,pathlib,json,re,math,statistics
p=argparse.ArgumentParser();p.add_argument('logs',type=pathlib.Path,nargs='+');p.add_argument('--out',type=pathlib.Path,required=True);a=p.parse_args()
def fields(l):return {k:float(v) for k,v in re.findall(r'(\w+)=(-?[\d.]+)',l)}
def stats(v):
 s=sorted(v);return dict(samples=len(s),median=statistics.median(s),p95=s[math.ceil(.95*len(s))-1],p99=s[math.ceil(.99*len(s))-1],worst=max(s),minimum=min(s))
runs=[]
for path in a.logs:
 text=path.read_text();frames=[fields(l) for l in text.splitlines() if l.startswith('FRAME ')];gaps=[fields(l) for l in text.splitlines() if l.startswith('GAP ')];hud=[fields(l) for l in text.splitlines() if l.startswith('HUDPROFILE ')][-3840:]
 assert len(frames)==len(hud)==3840 and [int(r['tick']) for r in frames]==list(range(1800,5640));assert sum(r['primitives'] for r in frames)==5152196 and sum(r['visible'] for r in frames)==3677980
 assert len(gaps)==3839 and [int(g['tick']) for g in gaps]==list(range(1800,5639))
 for g,f in zip(gaps,frames[1:]):assert abs(g['gap_ms']-(f['interval_ms']-f['total_ms']))<.002
 metrics={k:stats([g[k] for g in gaps]) for k in gaps[0] if k.endswith('_ms')};world=[f['world_submit_ms']+f['world_sync_ms'] for f in frames];final=[f['hud_submit_ms']+f['finish_ms']-h['pen_wall_ms'] for f,h in zip(frames,hud)]
 runs.append(dict(file=str(path),frames=3840,gaps=3839,options=next(l for l in text.splitlines() if l.startswith('NATIVE_OPTIONS ')),depth=next(l for l in text.splitlines() if l.startswith('DEPTH ')),gap_stages=metrics,context_switches=dict(voluntary=sum(g['voluntary'] for g in gaps),involuntary=sum(g['involuntary'] for g in gaps)),worst_gaps=sorted(gaps,key=lambda g:g['gap_ms'],reverse=True)[:12],world=stats(world),pen=stats([h['pen_wall_ms'] for h in hud]),final=stats(final),completion=stats([f['total_ms'] for f in frames]),interval=stats([f['interval_ms'] for f in frames]),completion_misses=sum(f['total_ms']>16.693 for f in frames),interval_misses=sum(f['interval_ms']>16.693 for f in frames),drain=next(l for l in text.splitlines() if l.startswith('TRACE_DRAIN ')),peak_rss_kib=int(re.search(r'RSS max_kib=(\d+)',text)[1])))
a.out.write_text(json.dumps(dict(scope='Every frame and 3839 end-to-next-start gaps retained. Component fences are diagnostic only. Gap residual includes instrumentation, bookkeeping and scheduling; context switches are attribution evidence, not exclusive scheduler timers. RAM drain is outside lap and reported, not discarded.',runs=runs),indent=2)+'\n')
for r in runs:print(r['file'],r['options'],'world',r['world']['median'],'final',r['final']['median'],'gap-worst',r['gap_stages']['gap_ms']['worst'])
