#!/usr/bin/env python3
"""Offline feasibility bounds, not an implemented scheduler or FPS prediction."""
import pathlib,argparse,csv,json,math,numpy as np
p=argparse.ArgumentParser();p.add_argument('run',type=pathlib.Path);a=p.parse_args();b=a.run
read=lambda n:list(csv.DictReader((b/n).open()))
rows=read('audit.csv');sim={int(r['tick']):r for r in read('simulation.csv')};native=[]
import re
for l in (b/'run.txt').read_text().splitlines():
 if l.startswith('FRAME '):native.append(dict(re.findall(r'(\w+)=([^ ]+)',l)))
gpu={int(r['tick']):r for r in native};assert len(rows)==4207 and len(gpu)==4207
S=np.array([float(sim[int(r['tick'])]['wall_ms']) for r in rows]);C=np.array([float(r['copy_wall_ms']) for r in rows]);P=np.array([float(r['walk_wall_ms'])+float(r['compile_wall_ms']) for r in rows]);G=np.array([float(gpu[int(r['tick'])]['total_ms']) for r in rows]);RC=np.array([float(gpu[int(r['tick'])]['cpu_ms']) for r in rows]);T=814*525/25600000*1000

def dist(x):
 x=np.sort(x);n=len(x);return dict(n=n,median=float(np.median(x)),p95=float(x[math.ceil(.95*n)-1]),p99=float(x[math.ceil(.99*n)-1]),worst=float(x[-1]),mean=float(x.mean()),over_16_693=int((x>16.693).sum()))

def pipeline(s,c,p,g,latency_periods=None,capacity=3):
 n=len(s);se=pe=ge=swap=0.;done=[];present=[];back=[];lag=[]
 for i in range(n):
  arrival=i*T;work_end=max(arrival,se)+s[i];reserve=max(work_end,done[i-capacity] if i>=capacity else 0);back.append(reserve-work_end);se=reserve+c[i];pe=max(se,pe)+p[i];ge=max(pe,swap)+g[i];done.append(ge);swap=max(ge,arrival+latency_periods*T) if latency_periods else ge;present.append(swap);lag.append(se-arrival)
 delays=np.array(present)-np.arange(n)*T;interval=np.diff(present)
 return dict(completion_latency_ms=dist(np.array(done)-np.arange(n)*T),presentation_latency_ms=dist(delays),presentation_interval_ms=dist(interval),cpu_publication_lag_ms=dist(lag),late_2_periods=int((np.array(done)>(np.arange(n)+2)*T+1e-7).sum()),late_3_periods=int((np.array(done)>(np.arange(n)+3)*T+1e-7).sum()),backpressure_events=int((np.array(back)>1e-7).sum()),backpressure_total_ms=float(sum(back)),output_fps=float((n-1)*1000/(present[-1]-present[0])),strict_pacing_late_intervals=int((interval>T+1e-6).sum()))
out=dict(target_hz=1000/T,period_ms=T,strict_deadline_ms=16.693,source='Current control run: joint per-tick simulation/copy/preparation/GPU completion. Sequential shadow-original work is excluded from native product model, never CPU work genuinely needed by the renderer.',assumptions='Separate cores and stages, FIFO no omitted snapshots/ticks, finite3-slot ownership, no bandwidth/thermal penalty except explicit CPU slowdown.0.1ms assumed per-stage handoff/scheduling cost, not measured native parallel overhead. GPU completion adds slowdown only to measured render-thread CPU component. No prediction of real scanout or production readiness. Existing6ms preparation coalescing and latest-picture dropping are NOT assumed in this strict FIFO model; they would require explicit policy review in a separately authorised prototype.',service_ms={k:dist(v) for k,v in [('simulation',S),('snapshot_copy',C),('native_preparation',P),('renderer',G),('renderer_cpu',RC),('two_stage_cpu_producer',S+C+P),('two_stage_preparation_plus_render',P+G),('ideal_three_stage_bottleneck',np.maximum.reduce([S+C,P,G]))]},scenarios={})
for extra in [0,.1,.2,.3]:
 f=1+extra;s=S*f+.1;c=C*f;p=P*f+.1;g=G+RC*extra+.1
 out['scenarios'][str(extra)]=dict(ideal_bottleneck_ms=dist(np.maximum.reduce([s+c,p,g])),unpaced=pipeline(s,c,p,g),fixed_2_period_latency=pipeline(s,c,p,g,2),fixed_3_period_latency=pipeline(s,c,p,g,3))
out['gpu_contention_stress']={}
for extra in [.1,.2]:
 f=1+extra;s=S*f+.1;c=C*f;p=P*f+.1;g=G*f+.1
 out['gpu_contention_stress'][str(extra)]=dict(renderer_service_ms=dist(g),unpaced=pipeline(s,c,p,g),fixed_3_period_latency=pipeline(s,c,p,g,3))
out['hypothetical_preparation_savings']={}
for extra in [.1,.2]:
 for saved in [.5,1.,2.]:
  f=1+extra;s=S*f+.1;c=C*f;p=np.maximum(P*f-saved,.01)+.1;g=G+RC*extra+.1
  out['hypothetical_preparation_savings'][f'cpu_penalty={extra},saved_ms={saved}']=pipeline(s,c,p,g,3)
out['observed_proxy_stress']=dict(assumption='Original production all-tick simulation wall-mean ratio1.1252 and paired snapshot-copy median ratio1.4308 are applied to native measurements; preparation/GPU unchanged. This is NOT measured native concurrency and is not a causal contention estimate.',result=pipeline(S*1.1252+.1,C*1.4308,P+.1,G+.1,3))
# Conservative copy-budget stress: measured production p99 copy charged on every tick.
out['production_p99_copy_stress']=pipeline(S+.1,np.full(len(C),1.167208),P+.1,G+.1,3)
(b/'pipeline-model.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
