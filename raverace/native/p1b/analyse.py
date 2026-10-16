#!/usr/bin/env python3
"""P1b complete retained populations, nearest-rank quantiles, no trimming."""
import argparse,pathlib,csv,json,math,statistics,re,hashlib,collections
p=argparse.ArgumentParser();p.add_argument('run',type=pathlib.Path);p.add_argument('--images',type=pathlib.Path);a=p.parse_args();b=a.run
root=pathlib.Path(__file__).resolve().parents[3];T=1000/59.7104

def dist(v):
 s=sorted(v);n=len(s)
 return dict(n=n,median=statistics.median(s),p95=s[math.ceil(.95*n)-1],p99=s[math.ceil(.99*n)-1],worst=s[-1],minimum=s[0],mean=statistics.mean(s)) if n else None
rows=list(csv.DictReader((b/'thread.csv').open()));phase=[r for r in rows if 1800<=int(r['frame'])<=6006];log=(b/'run.txt').read_text()
ref=root/'raverace/benchmarks/native-p1-20261015/validation-q3'
assert (b/'exit.txt').read_text().strip()=='0'
assert [json.loads(l) for l in (b/'states.jsonl').read_text().splitlines()]==[json.loads(l) for l in (ref/'states.jsonl').read_text().splitlines()]
def receipts(f):
 rr=list(csv.DictReader(f.open()));assert len(rr)==4207
 ans={int(r['tick']):(int(r['bytes']),r['sha256']) for r in rr};assert len(ans)==4207 and set(ans)==set(range(1800,6007));return ans
assert receipts(b/'command-sha256.csv')==receipts(ref/'command-sha256.csv')
ids=[int(r['frame']) for r in rows];assert ids==sorted(set(ids))
replaced=[int(x) for x in re.findall(r'\[P1B-REPLACE\].*?replaced=(\d+)',log)]
missing=sorted(set(range(1,6007))-set(ids));assert missing==sorted(replaced)
assert not missing or '[P1B-VALVE] enabled=1' in log
io=re.search(r'\[P1B-IO\] (.*)',log);assert io
io=dict(re.findall(r'(\w+)=([^ ]+)',io[1]));assert int(io['errors'])==0 and int(io['dropped_records'])==0 and int(io['packets'])==4207
warm=re.findall(r'\[P1B-WARM\] tick=(\d+) resource_copy=(\d+) complete_ms=([0-9.]+)',log);assert len(warm)==3 and [int(x[1]) for x in warm]==[0,1,2] and all(int(x[0])<1800 for x in warm)
for n in ['clocks','production']:
 if (b/(n+'.before')).exists():assert (b/(n+'.before')).read_bytes()==(b/(n+'.after')).read_bytes()
metrics={}
for key,scale in [('simulation_ms',1),('simulation_blocked_ms',1),('native_walk_ms',1),('native_compile_ms',1),('prep_wall',1000),('prep_cpu',1000),('hud_cpu',1000),('cpu',1000),('draw_wall',1000),('swap_wall',1000),('publication_age',1000),('snapshot_copy_wall',1000),('prepared_wait',1000),('prep_queue_age',1000)]:metrics[key]=dist(float(r[key])*scale for r in phase)
events=[];audio_events=[];gaps=[];intervals=[];prev=None
for r in rows:
 tick=int(r['frame']);present=float(r['presented']);pub=present-float(r['publication_age']);prepare=pub+float(r['prep_queue_age'])-float(r['prep_wall']);render=float(r['start'])
 if prev:
  delta=(present-prev['present'])*1000
  if 1800<=tick<=6006:
   intervals.append(delta);n=max(0,math.floor(delta/T+.5)-1)
   coincident=dict(frame=tick,previous_frame=prev['frame'],monotonic=present,interval_ms=delta,simulation_ms=float(r['simulation_ms']),main_fifo_wait_ms=float(r['simulation_blocked_ms']),walk_ms=float(r['native_walk_ms']),compiler_ms=float(r['native_compile_ms']),prepare_wall_ms=float(r['prep_wall'])*1000,prepare_cpu_ms=float(r['prep_cpu'])*1000,gl_cpu_ms=float(r['cpu'])*1000,draw_ms=float(r['draw_wall'])*1000,swap_ms=float(r['swap_wall'])*1000,snapshot_age_ms=float(r['publication_age'])*1000,draws=int(r['draws']),triangles=int(r['triangles']),audio_underrun_delta=int(r['audio_underruns'])-prev['audio'],main_progress_gap_ms=(pub-prev['pub'])*1000)
   if n:events.append(dict(repeats=n,**coincident))
   if coincident['audio_underrun_delta']:audio_events.append(coincident)
  for stage,end,start in [('main_publication',pub,prev['pub']),('prepare_start',prepare,prev['prepare']),('present_start',render,prev['render'])]:
   if end-start>.05:gaps.append(dict(stage=stage,frame=tick,native=1800<=tick<=6006,start=start,end=end,gap_ms=(end-start)*1000,simulation_ms=float(r['simulation_ms']),fifo_wait_ms=float(r['simulation_blocked_ms']),prepare_ms=float(r['prep_wall'])*1000,prepare_cpu_ms=float(r['prep_cpu'])*1000,present_cpu_ms=float(r['cpu'])*1000,draw_ms=float(r['draw_wall'])*1000,swap_ms=float(r['swap_wall'])*1000))
 prev=dict(frame=tick,present=present,pub=pub,prepare=prepare,render=render,audio=int(r['audio_underruns']))
source_ticks=list(csv.DictReader((b/'ticks.csv').open())) if (b/'ticks.csv').exists() else []
source_gaps=[]
if source_ticks:
 assert [int(r['frame']) for r in source_ticks]==list(range(1,6007))
 for x,y in zip(source_ticks,source_ticks[1:]):
  dt=(float(y['completed'])-float(x['completed']))*1000
  if dt>50:source_gaps.append(dict(stage='source_tick_completion',frame=int(y['frame']),native=1800<=int(y['frame'])<=6006,start=float(x['completed']),end=float(y['completed']),gap_ms=dt))
 gaps.extend(source_gaps)
byid={int(r['frame']):r for r in rows};native_underruns=int(byid[6006]['audio_underruns'])-int(byid[1799]['audio_underruns'])
valve_match=re.search(r'\[P1B-VALVE\] (.*)',log);valve=dict(re.findall(r'(\w+)=([^ ]+)',valve_match[1])) if valve_match else {}
native_main=[g for g in gaps if g['native'] and g['stage']=='main_publication'];transition=[(float(r['presented'])-float(r['publication_age']),int(r['frame'])) for r in rows if 1790<=int(r['frame'])<=1810];transition_gaps=[(y[0]-x[0])*1000 for x,y in zip(transition,transition[1:])]
cl=re.search(r'\[P1B-CLOCK\] monotonic=([\d.]+) realtime=([\d.]+)',log)
out=dict(native_source_tick_gaps_over50ms=sum(g['native'] for g in source_gaps),native_source_tick_max_gap_ms=max([(float(y['completed'])-float(x['completed']))*1000 for x,y in zip(source_ticks,source_ticks[1:]) if 1800<=int(y['frame'])<=6006],default=0),analysis_window=[1800,6006],source_packets=4207,authoritative_checkpoint_sets=21,all_state_and_packets_identical=True,presented_total=len(rows),presented_native=len(phase),presented_in_order_without_duplicates=True,replaced_ticks=missing,service_ms=metrics,presentation_interval_ms=dist(intervals),inferred_repeated_vblanks=sum(e['repeats'] for e in events),repeat_events=len(events),repeat_events_all_single=all(e['repeats']==1 for e in events),repeat_events_isolated_60ticks=all(y['frame']-x['frame']>=60 for x,y in zip(events,events[1:])),native_audio_underrun_sample_frames=native_underruns,native_main_gaps_over50ms=len(native_main),native_main_gap_total_ms=sum(g['gap_ms'] for g in native_main),all_native_gaps_by_stage=dict(collections.Counter(g['stage'] for g in gaps if g['native'])),transition_window=[1790,1810],transition_max_main_gap_ms=max(transition_gaps),transition_under_two_periods=max(transition_gaps)<=2*T,queue_occupancy=dict(collections.Counter(r['occupied_slots'] for r in phase)),native_resampler_ratio=dist(float(r['resampler_ratio']) for r in phase),valve=valve,io=io,warmup=warm,audio_shutdown=[l for l in log.splitlines() if l.startswith(('[AUDIO-OUTPUT]','[P1B-RELEASE]','[P1]'))],native_begin_monotonic=float(phase[0]['presented'])-float(phase[0]['publication_age']),native_end_monotonic=float(phase[-1]['presented']),realtime_minus_monotonic=float(cl[2])-float(cl[1]) if cl else None,scanout_age_is_swap_return_proxy=True,repeat_count_is_inferred=True,all_samples_retained=True)
out['audio_underrun_event_count']=len(audio_events)
out['audio_by_region']={name:sum(e['audio_underrun_delta'] for e in audio_events if lo<=e['frame']<=hi) for name,lo,hi in [('handover1800_1830',1800,1830),('dense3550_3590',3550,3590),('all_other',1831,3549)]}
out['audio_by_region']['all_other']+=sum(e['audio_underrun_delta'] for e in audio_events if e['frame']>3590)
out['dense3550_3590_service_ms']={k:dist(float(r[k])*scale for r in phase if 3550<=int(r['frame'])<=3590) for k,scale in [('native_walk_ms',1),('native_compile_ms',1),('prep_wall',1000),('prep_cpu',1000),('prepared_wait',1000),('draw_wall',1000),('swap_wall',1000)]}
if (b/'temperature.start').exists():out['start_temperature_c']=int((b/'temperature.start').read_text())/1000;assert out['start_temperature_c']<50
if (b/'temperature.end').exists():out['end_temperature_c']=int((b/'temperature.end').read_text())/1000
if a.images:
 images={f"tick-{r['tick']}.{r['suffix']}.rgba":r['sha256']==hashlib.sha256((a.images/f"tick-{r['tick']}.{r['suffix']}.rgba").read_bytes()).hexdigest() for r in csv.DictReader((b/'image-sha256.csv').open())};assert len(images)==36 and all(images.values());out['image_hashes_match']=36;(b/'image-comparison.json').write_text(json.dumps(images,indent=2)+'\n')
for name,data in [('repeat-events.csv',events),('audio-events.csv',audio_events),('stage-gaps.csv',gaps)]:
 with (b/name).open('w') as f:
  keys=list(dict.fromkeys(k for r in data for k in r)) if data else ['frame'];w=csv.DictWriter(f,fieldnames=keys);w.writeheader();w.writerows(data)
(b/'p1b-summary.json').write_text(json.dumps(out,indent=2)+'\n')
print(b.name,'repeats',out['inferred_repeated_vblanks'],'audio',native_underruns,'main>50',len(native_main),'transition',round(max(transition_gaps),3),'valve',valve)
