#!/usr/bin/env python3
import argparse,pathlib,csv,re,json,math
import numpy as np
p=argparse.ArgumentParser();p.add_argument('run',type=pathlib.Path);a=p.parse_args();raw=(a.run/'run.txt').read_text();lines=raw.splitlines();header=next(l for l in lines if l.startswith('swap,begin_seconds'));rows=list(csv.DictReader([header]+[l for l in lines if re.match(r'^\d+,',l)]));assert len(rows)>=3001,len(rows)
def stats(v):
 x=np.sort(np.array(v,float));n=len(x);med=float(np.median(x));mean=float(x.mean());return dict(n=n,mean_ms=mean,median_ms=med,stddev_ms=float(x.std()),p95_ms=float(x[math.ceil(.95*n)-1]),p99_ms=float(x[math.ceil(.99*n)-1]),worst_ms=float(x[-1]),mean_hz=1000/mean,median_hz=1000/med,shorter_than_half_median=int((x<med*.5).sum()),longer_than_1_5_median=int((x>med*1.5).sum()),nearest_multiple_missed_vsync_estimate=int(np.maximum(np.floor(x/med+.5)-1,0).sum()))
period=stats([float(r['interval_ms']) for r in rows[1:]]);blocked=stats([float(r['blocked_ms']) for r in rows]);vsync=[float(m[2]) for m in re.findall(r'^# VSYNC (\d+) ([\d.]+) ([\d.]+)',raw,re.M)][1:];r=dict(swap_returns=len(rows),all_between_return_intervals=period,swap_blocking=blocked,metadata=[l for l in lines if l.startswith('#') and not l.startswith('# VSYNC')],vsync_wait_intervals=stats(vsync) if vsync else None,note='Every between-return interval retained; no warmup trimming. Return rate is not automatically physical refresh when delayed/dropped returns occur. Timestamps cannot prove absence of visible tearing.')
returns=np.array([float(v['return_seconds']) for v in rows]);index=np.arange(len(returns));slope,intercept=np.polyfit(index,returns-returns[0],1)
r['all_return_linear_fit']={'samples':len(rows),'period_ms':float(slope*1000),'hz':float(1/slope),'all_samples_used':True,'residual_min_ms':float(np.min(returns-returns[0]-(intercept+slope*index))*1000),'residual_max_ms':float(np.max(returns-returns[0]-(intercept+slope*index))*1000),'note':'All return timestamps used, including startup queue filling. Fit separates long-run cadence from first queued immediate return; no outlier exclusion.'}
r['fb_vsync_wait_blocks']=bool(vsync) and float(np.mean(vsync))>5
if not r['fb_vsync_wait_blocks']:r['fb_vsync_wait_rejection']='IOCTL reports success but returns in microseconds: not a physical vsync measurement.'
arcade=25600000/(814*525);r['arcade_hz']=arcade
for name,data in [('swap_mean',period),('fitted_panel',{'mean_hz':float(1/slope)}),('fb_vsync_mean',r['vsync_wait_intervals'] if r['fb_vsync_wait_blocks'] else None)]:
 if data:
  hz=data['mean_hz'];delta=hz-arcade;r[name+'_cadence_difference']={'hz':delta,'one_repeat_or_drop_seconds':1/abs(delta) if delta else None,'policy':'panel faster: repeat a source picture' if delta>0 else 'panel slower: omit a presentation while preserving authoritative ticks'}
(a.run/'panel-summary.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
