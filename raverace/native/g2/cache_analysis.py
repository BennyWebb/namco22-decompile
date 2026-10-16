#!/usr/bin/env python3
"""Full-population cache/pacing attribution, including transitions and all selectors."""
import pathlib,csv,re,json,statistics,math,collections,sys
r=pathlib.Path(sys.argv[1]);rows=list(csv.DictReader((r/'thread.csv').open()));log=(r/'run.txt').read_text();T=1000/59.7104

def dist(v):
 a=sorted(v);n=len(a);return dict(n=n,median=statistics.median(a),p95=a[math.ceil(.95*n)-1],p99=a[math.ceil(.99*n)-1],worst=a[-1],minimum=a[0]) if n else None
cache={int(t):dict(zip(['occupancy','lookups','probes','max_probe','full','clears','generation','capacity'],map(int,v))) for t,*v in re.findall(r'G1C-CACHE\] tick=(\d+) occupancy=(\d+) lookups=(\d+) probes=(\d+) max_probe=(\d+) full=(\d+) clears=(\d+) generation=(\d+) capacity=(\d+)',log)};cov=json.loads((r/'picture-coverage.json').read_text());assert len(cache)==len(rows)==len(cov);assert [int(x['frame']) for x in rows]==[x['tick'] for x in cov];release=int(re.search(r'P1B-RELEASE\] completed_tick=(\d+)',log)[1]);dyn={x['tick']:x for x in json.loads((r/'dynamic-per-picture.json').read_text())['prepare']}
groups=collections.defaultdict(list);events=[];gaps=[]
for i,(x,c) in enumerate(zip(rows,cov)):
 tick=int(x['frame']);cl=cache[tick];assert cl['occupancy']<=min(cl['capacity']//2,65535) and cl['max_probe']<=64
 dt=(float(x['presented'])-float(rows[i-1]['presented']))*1000 if i else 0;n=max(0,math.floor(dt/T+.5)-1);under=int(x['audio_underruns'])-int(rows[i-1]['audio_underruns']) if i else 0;gap=(float(x['presented'])-float(x['publication_age'])-float(rows[i-1]['presented'])+float(rows[i-1]['publication_age']))*1000 if i else 0
 record=dict(tick=tick,native=c['native'],course=c['course'],main=c['main'],repeats=n if c['native'] else 0,underrun_frames=under if c['native'] and tick>release else 0,main_gap_ms=gap,**cl,evictions=dyn.get(tick,{}).get('evictions',0),dynamic_misses=dyn.get(tick,{}).get('misses',0),interval_ms=dt,prepare_ms=float(x['prep_wall'])*1000,compiler_ms=float(x['native_compile_ms']),walk_ms=float(x['native_walk_ms']),simulation_ms=float(x['simulation_ms']),blocked_ms=float(x['simulation_blocked_ms']),draw_ms=float(x['draw_wall'])*1000,swap_ms=float(x['swap_wall'])*1000,age_ms=float(x['publication_age'])*1000,presented=float(x['presented']))
 groups[c['course']].append(record)
 if c['native'] and n:events.append(record)
 if c['native'] and gap>50:gaps.append(record)
def aggregate(a):
 native=[x for x in a if x['native']];looks=sum(x['lookups'] for x in a);return dict(pictures=len(a),native=len(native),legacy=len(a)-len(native),repeats=sum(x['repeats'] for x in a),settled_underrun_frames=sum(x['underrun_frames'] for x in a),main_gaps50=sum(x['native'] and x['main_gap_ms']>50 for x in a),peak_occupancy=max(x['occupancy'] for x in a),probes_per_lookup=sum(x['probes'] for x in a)/looks if looks else 0,max_probe=max(x['max_probe'] for x in a),probe_bound_fallbacks=sum(x['full'] for x in a),generational_resets=sum(x['clears'] for x in a),dynamic_misses=sum(x['dynamic_misses'] for x in a),dynamic_evictions=sum(x['evictions'] for x in a),stage_ms={k:dist(x[k] for x in native) for k in ['prepare_ms','compiler_ms','walk_ms','simulation_ms','blocked_ms','draw_ms','swap_ms','interval_ms','age_ms']})
allrows=[x for a in groups.values() for x in a];start=float(rows[0]['presented']);buckets=collections.defaultdict(list)
for x in allrows:buckets[int((x['presented']-start)//60)].append(x)
visits=[]
for stretch in json.loads((r/'coverage-summary.json').read_text())['native_stretches']:
 a=[x for x in allrows if stretch['first_tick']<=x['tick']<=stretch['last_tick']];visits.append(dict(first_tick=stretch['first_tick'],last_tick=stretch['last_tick'],course=stretch['course'],complete=stretch['last_tick']<int(rows[-1]['frame']),**aggregate(a)))
out=dict(visits=visits,full=aggregate(allrows),release_tick=release,per_selector={k:aggregate(a) for k,a in sorted(groups.items())},per_minute={k:aggregate(a) for k,a in sorted(buckets.items())},repeat_events=events,main_gaps=gaps,definition='All pictures, all transitions retained; settled audio after actual legacy release. Main gap is publication-to-publication. Inferred repeats nearest panel-period count, not hardware vblank events.')
(r/'cache-analysis.json').write_text(json.dumps(out,indent=2)+'\n');print(r.name,'pictures',len(rows),'repeats',out['full']['repeats'],'underruns',out['full']['settled_underrun_frames'],'gaps',len(gaps),'maxprobe',out['full']['max_probe'],'evictions',out['full']['dynamic_evictions'])
