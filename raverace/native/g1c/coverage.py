#!/usr/bin/env python3
"""Count every picture, preserving raw bytes and reconstructing shutdown stream splits."""
import pathlib,re,csv,json,collections,argparse,math,statistics
p=argparse.ArgumentParser();p.add_argument('run',type=pathlib.Path);a=p.parse_args();r=a.run;raw=(r/'run.txt').read_text();clean=re.sub(r'\{"schema":[^\n]*\n|C1E_FRAME [^\n]*\n|\[C1E-CAPTURE\][^\n]*\n','',raw)
rows=list(csv.DictReader((r/'thread.csv').open()));coverage=[]
for m in re.finditer(r'\[G1-COVERAGE\] tick=(\d+) main=(\d+) sub=(\d+) screen=(\d+) course=(\d+) major=(\d+) walk=(\d+) direct=(\d+) native=(\d+) reason=([^\n]+)',clean):coverage.append(dict(zip(['tick','main','sub','screen','route','course','walk','direct','native'],map(int,m.groups()[:9])),reason=m[10]))
assert [x['tick'] for x in coverage]==[int(x['frame']) for x in rows],(len(coverage),len(rows))
prep=[dict(zip(['tick','hits','misses','evictions','pending_bytes'],map(int,m.groups()))) for m in re.finditer(r'\[G1B-PREP\] tick=(\d+) hits=(\d+) misses=(\d+) evictions=(\d+) pending_bytes=(\d+)',clean)]
upload=[dict(zip(['tick','copy','bytes','materials','hits','misses','evictions'],map(int,m.groups()))) for m in re.finditer(r'\[G1B-UPLOAD\] tick=(\d+) copy=(\d+) bytes=(\d+) materials=(\d+) hits=(\d+) misses=(\d+) evictions=(\d+)',clean)]
bytick={int(x['frame']):x for x in rows};assert len(upload)==sum(x['native'] or x['reason']=='prewarm' for x in coverage)
falls=re.findall(r'\[G1B-FALLBACK\] ([^\n]+)',clean)
T=1/59.7104;groups=collections.defaultdict(lambda:dict(native=0,fallback=0,reasons=collections.Counter(),inferred_repeats_in_native=0,underrun_frames_in_native=0))
prev=None;repeat=[];stretches=[];stretch=None;main_gaps=[]
for x in coverage:
 g=groups[(x['main'],x['course'])];g['native' if x['native'] else 'fallback']+=1
 if not x['native']:g['reasons'][x['reason']]+=1
 rr=bytick[x['tick']]
 if x['native'] and prev:
  gap=(float(rr['presented'])-float(rr['publication_age'])-float(prev['presented'])+float(prev['publication_age']))*1000
  if gap>50:main_gaps.append(dict(tick=x['tick'],gap_ms=gap,prepare_ms=float(rr['prep_wall'])*1000,simulation_ms=float(rr['simulation_ms'])))
 if x['native']:
  if stretch is not None and (stretch['course']!=x['course'] or stretch['main']!=x['main']):stretches.append(stretch);stretch=None
  if stretch is None:stretch=dict(first_tick=x['tick'],last_tick=x['tick'],course=x['course'],main=x['main'],repeats=0,underrun_frames=0)
  stretch['last_tick']=x['tick']
  if prev:
   interval=float(rr['presented'])-float(prev['presented']);n=max(0,math.floor(interval/T+.5)-1);under=int(rr['audio_underruns'])-int(prev['audio_underruns']);g['inferred_repeats_in_native']+=n;g['underrun_frames_in_native']+=under;stretch['repeats']+=n;stretch['underrun_frames']+=under
   if n:repeat.append(dict(tick=x['tick'],repeats=n,interval_ms=interval*1000,main_ms=float(rr['simulation_ms']),prepare_ms=float(rr['prep_wall'])*1000,draw_ms=float(rr['draw_wall'])*1000,swap_ms=float(rr['swap_wall'])*1000))
 elif stretch is not None:stretches.append(stretch);stretch=None
 prev=rr
if stretch is not None:stretches.append(stretch)
def aggregate(seq,keys):return {k:sum(x[k] for x in seq) for k in keys}
out=dict(pictures=len(rows),native=sum(x['native'] for x in coverage),fallback=sum(not x['native'] for x in coverage),groups=[dict(main=k[0],course=k[1],**v) for k,v in sorted(groups.items())],fallback_exact_records=falls,cpu_dynamic=aggregate(prep,['hits','misses','evictions','pending_bytes']),gpu_dynamic_pen_bytes=sum(x['bytes'] for x in upload),dynamic_UV_bytes=sum(x['materials']*16 for x in upload),native_window_cpu_dynamic=aggregate([x for x in prep if x['tick']>=1800],['hits','misses','evictions','pending_bytes']),native_window_gpu_pen_bytes=sum(x['bytes'] for x in upload if x['tick']>=1800),native_window_dynamic_UV_bytes=sum(x['materials']*16 for x in upload if x['tick']>=1800),native_stretches=stretches,repeat_events_native=repeat,main_gaps_actual_native=main_gaps,stream_reconstruction='Remove separately retained shutdown stdout JSON/native trace and direct capture footer to rejoin intact stderr fragments; require every picture and every upload')
(r/'coverage-summary.json').write_text(json.dumps(out,indent=2)+'\n');(r/'picture-coverage.json').write_text(json.dumps(coverage,indent=2)+'\n');(r/'dynamic-per-picture.json').write_text(json.dumps(dict(prepare=prep,uploads=upload),indent=2)+'\n');print(r.name,len(rows),'native',out['native'],'fallback',out['fallback'],'CPU',out['cpu_dynamic'],'pen bytes',out['gpu_dynamic_pen_bytes'])
