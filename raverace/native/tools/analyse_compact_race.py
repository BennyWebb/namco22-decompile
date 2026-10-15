#!/usr/bin/env python3
"""Validate complete v3 timing windows and report offline world+HUD capacity."""
import argparse,json,math,pathlib,re,statistics
p=argparse.ArgumentParser();p.add_argument('logs',nargs='+',type=pathlib.Path);p.add_argument('--out',required=True,type=pathlib.Path);a=p.parse_args()
def stats(v):
 s=sorted(v);return dict(samples=len(s),median=statistics.median(s),p95=s[math.ceil(len(s)*.95)-1],p99=s[math.ceil(len(s)*.99)-1],worst=s[-1])
runs=[]
for log in a.logs:
 text=log.read_text();rows=[{k:float(v) for k,v in re.findall(r'(\w+)=([\d.]+)',l)} for l in text.splitlines() if l.startswith('FRAME ')]
 assert len(rows)==3840 and [int(r['tick']) for r in rows]==list(range(1800,5640)),str(log)
 assert all(r['primitives']==r['visible']+r['culled'] for r in rows)
 assert sum(r['primitives'] for r in rows)==5152196
 stages={k:stats([r[k] for r in rows]) for k in rows[0] if k.endswith('_ms')}
 run=dict(file=str(log),frames=len(rows),source_commands=int(sum(r['primitives'] for r in rows)),visible=int(sum(r['visible'] for r in rows)),stages=stages,draws=stats([r['draws'] for r in rows]),dynamic_bytes=stats([r['dynamic_bytes'] for r in rows]),over_14ms=sum(r['total_ms']>14 for r in rows),over_deadline=sum(r['total_ms']>16.693 for r in rows),interval_over_deadline=sum(r['interval_ms']>16.693 for r in rows),offline_capacity_fps=3840000/sum(r['interval_ms'] for r in rows),low_1pct_capacity_fps=1000/statistics.mean(sorted([r['interval_ms'] for r in rows],reverse=True)[:39]),peak_rss_kib=int(re.search(r'RSS max_kib=(\d+)',text)[1]),startup=[l for l in text.splitlines() if l.startswith(('LOAD ','PREFILL '))],slowest=sorted(rows,key=lambda r:r['total_ms'],reverse=True)[:10])
 run['headroom_eligible']=('stage_sync=0' in text and not re.search(r'^(?:PROFILE.* mode=[1-9]|HUDPROFILE.* control=[1-9])',text,re.M));run['headroom_pass']=(stages['total_ms']['median']<=12 and stages['total_ms']['p95']<=14) if run['headroom_eligible'] else None;runs.append(run)
a.out.write_text(json.dumps(dict(scope='Uncapped offline scene playback, one final world+HUD glFinish. Queue readiness wait is timed; producer read/inflate and worker CPU overlap and cannot be summed with main-thread stages. Submission stages are not exclusive GPU timings. No simulation/audio, swap or presentation proof.',runs=runs),indent=2)+'\n')
for r in runs:print(r['file'],r['stages']['total_ms'],'headroom_pass',r['headroom_pass'],'misses',r['over_deadline'])
