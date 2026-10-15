#!/usr/bin/env python3
"""Offline completion capacity, never live gameplay FPS or display cadence."""
import argparse,pathlib,re,json,statistics,math
p=argparse.ArgumentParser();p.add_argument('logs',nargs='+',type=pathlib.Path);p.add_argument('--out',type=pathlib.Path,required=True);a=p.parse_args()
def stats(v):
 s=sorted(v);return dict(samples=len(s),median=statistics.median(s),p95=s[math.ceil(len(s)*.95)-1],p99=s[math.ceil(len(s)*.99)-1],worst=max(s))
runs=[]
for log in a.logs:
 text=re.sub(r'\[(?:VIDEO|GPU-HUD[^]]*)\][^\n]*\n','',log.read_text());rows=[{k:float(v) for k,v in re.findall(r'(\w+)=([\d.]+)',line)} for line in text.splitlines() if line.startswith('FRAME ')];assert len(rows)>=3840
 passes=sorted({int(r['pass']) for r in rows});assert all([int(r['tick']) for r in rows if r['pass']==s]==list(range(1800,5640)) for s in passes)
 runs.append(dict(file=str(log),frames=len(rows),stages={k:stats([r[k] for r in rows]) for k in ['load_ms','upload_ms','submit_ms','world_ms','hud_ms','total_ms','cpu_ms','interval_ms']},draws=stats([r['draws'] for r in rows]),dynamic_bytes=stats([r['dynamic_bytes'] for r in rows]),over_deadline=sum(r['total_ms']>16.693 for r in rows),over_14ms=sum(r['total_ms']>14 for r in rows),slowest=sorted(rows,key=lambda r:r['total_ms'],reverse=True)[:12],tick1965=[r for r in rows if r['tick']==1965],peak_rss_kib=int(re.search(r'RSS max_kib=(\d+)',text)[1]),load_line=next(l for l in text.splitlines() if l.startswith('LOAD ')),offline_capacity_fps=len(rows)*1000/sum(r['interval_ms'] for r in rows),low_1pct_capacity_fps=1000/statistics.mean(sorted([r['interval_ms'] for r in rows],reverse=True)[:math.ceil(len(rows)*.01)])))
a.out.write_text(json.dumps(dict(runs=runs,scope='Offline scene playback. Per-frame total includes file read, command upload, world glFinish and HUD glFinish. No simulation/audio, vsync/swap or display cadence. Readback/capture diagnostic work is outside timed span.'),indent=2)+'\n')
for r in runs:print(r['file'],r['stages']['total_ms'],'misses',r['over_deadline'])
