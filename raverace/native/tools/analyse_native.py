#!/usr/bin/env python3
"""Summarise matched reference/native frozen runs; never calls them playable FPS."""
import argparse,hashlib,json,math,pathlib,re,statistics
p=argparse.ArgumentParser();p.add_argument('paired',type=pathlib.Path);p.add_argument('second',type=pathlib.Path);p.add_argument('out',type=pathlib.Path);a=p.parse_args();a.out.mkdir(parents=True,exist_ok=True)
def stats(v):
 v=sorted(v)
 return dict(samples=len(v),median=statistics.median(v),p95=v[math.ceil(.95*len(v))-1],p99=v[math.ceil(.99*len(v))-1],worst=max(v))
def run(folder):
 text=(folder/'run.log').read_text();rows=re.findall(r'TIMING sample=(\d+) mode=(\d+) cpu_ms=([\d.]+) submission_ms=([\d.]+) completion_ms=([\d.]+)',text);modes={}
 for mode in sorted({int(r[1]) for r in rows}):
  sel=[r for r in rows if int(r[1])==mode];assert len(sel)==30 and sorted(int(r[0]) for r in sel)==list(range(30))
  modes[str(mode)]={k:stats([float(r[i]) for r in sel]) for i,k in enumerate(['thread_cpu_ms','submission_ms','completion_ms'],2)}
 return dict(modes=modes,peak_rss_kib=int(re.search(r'RSS max_kib=(\d+)',text)[1]),capture_sha256={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in sorted(folder.glob('*.rgba'))},load_ms=float(re.search(r'LOAD wall_ms=([\d.]+)',text)[1]))
rounds=[dict(round=f.name,reference=run(f/'reference'),native=run(f/'native')) for f in sorted(a.paired.glob('round*'))];assert len(rounds)==3
second=[dict(round=f.name,native=run(f)) for f in sorted(a.second.glob('run*'))];assert len(second)==3
for collection in [rounds,second]:
 assert len({r['native']['capture_sha256']['native.rgba'] for r in collection})==1,'native pixels change between identical process repeats'
med={}
for name,component,mode in [('streamed_incumbent','reference','0'),('resident_original','reference','1'),('rejected_grouped','reference','2'),('revised_native','native','1')]:
 med[name]={k:statistics.median(r[component]['modes'][mode][k]['median'] for r in rounds) for k in ['thread_cpu_ms','submission_ms','completion_ms']}
report=dict(matched_rounds=rounds,additional_scene_rounds=second,median_of_round_medians=med,
 tick2500_budget_pass=all(r['native']['modes']['1']['completion_ms']['median']<=12 and r['native']['modes']['1']['completion_ms']['p95']<=14 for r in rounds),
 tick3600_budget_pass=all(r['native']['modes']['1']['completion_ms']['median']<=12 and r['native']['modes']['1']['completion_ms']['p95']<=14 for r in second),
 scope='Frozen world only. Completion includes CPU feeding, driver work and glFinish; not exclusive GPU time, full-frame cadence or playable FPS.')
(a.out/'summary.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(med,indent=2));print('Budget',report['tick2500_budget_pass'],report['tick3600_budget_pass'])
