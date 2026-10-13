#!/usr/bin/env python3
"""Summarise repeated human-replay benchmarks and the five-second sections.
Usage: summarise_race_replay.py RESULTS_DIRECTORY FIRST LAST
"""
import csv
import io
import json
import math
import pathlib
import re
import statistics as st
import subprocess
import sys

root=pathlib.Path(sys.argv[1]);first,last=map(int,sys.argv[2:4])
reference=json.loads((root/'benchmark-reference.json').read_text())
def fields(line):return dict(re.findall(r'(\w+)=([^ ]+)',line))
def percentile(v,p):return sorted(v)[max(0,math.ceil(len(v)*p)-1)]
runs=[];sections=[]
for path in sorted(root.glob('*.log')):
    name=path.stem
    if not (name.startswith('run-') or name.startswith('untraced-')):continue
    assert json.loads(path.with_suffix('.json').read_text())==reference, f'{name}: simulation divergence'
    log=path.read_text().splitlines()
    rate=fields(next(l for l in log if l.startswith('[THREAD-RATE]')))
    assert int(rate['from'])==first and int(rate['steps'])==last-first+1
    row={'name':name,'traced':name.startswith('run-'),'rate':rate}
    audio=fields(next(l for l in log if l.startswith('[AUDIO-OUTPUT]')))
    row['audio']=audio
    assert all(int(audio[k])==0 for k in ['underrun_samples','overrun_samples','latency_resets'])
    if row['traced']:
        presentations=list(csv.DictReader((root/f'{name}.presentations.csv').open()))
        ticks={int(r['frame']):float(r['completed']) for r in csv.DictReader((root/f'{name}.ticks.csv').open())}
        start,end=ticks[first-1],ticks[last]
        chosen=[r for r in presentations if start<float(r['presented'])<=end and int(r['frame'])>=first]
        # Discard only a startup-crossing interval, not ordinary stutters.
        intervals=[float(b['presented'])-float(a['presented']) for a,b in zip(presentations,presentations[1:])
            if start<float(b['presented'])<=end and int(a['frame'])>=first]
        assert intervals and min(intervals)>0
        worst=sorted(intervals,reverse=True)[:math.ceil(len(intervals)*.01)]
        row['intervals']={'samples':len(intervals),'mean_ms':st.mean(intervals)*1000,
            'median_ms':st.median(intervals)*1000,'p95_ms':percentile(intervals,.95)*1000,
            'p99_ms':percentile(intervals,.99)*1000,'max_ms':max(intervals)*1000,'low1_fps':1/st.mean(worst),
            'over33ms':sum(i>.033 for i in intervals),'over50ms':sum(i>.050 for i in intervals),
            'over100ms':sum(i>.100 for i in intervals),'wall_window_fps':len(chosen)/(end-start)}
        row['presentation']={k:st.mean(float(r[k]) for r in chosen) for k in
            ['cpu','scene_wall','draw_wall','submission_wall','swap_wall','draws','triangles','binds','publication_age','queue_ready']}
        row['presentation'].update({k:sum(int(r[k]) for r in chosen) for k in ['misses','uploads','evictions']})
        row['presentation']['texture_peak_mib']=max(int(r['texture_bytes']) for r in chosen)/1048576
        # Keep individual large intervals with their emulated scene identifiers.
        pairs=[{'frame':int(b['frame']),'previous_frame':int(a['frame']),
                'interval_ms':(float(b['presented'])-float(a['presented']))*1000,
                'draws':int(b['draws']),'misses':int(b['misses']),'uploads':int(b['uploads']),
                'scene_ms':float(b['scene_wall'])*1000,'submission_ms':float(b['submission_wall'])*1000,
                'swap_ms':float(b['swap_wall'])*1000,'cpu_ms':float(b['cpu'])*1000}
            for a,b in zip(presentations,presentations[1:]) if start<float(b['presented'])<=end and int(a['frame'])>=first]
        row['largest_intervals']=sorted(pairs,key=lambda p:p['interval_ms'],reverse=True)[:10]
        result=subprocess.check_output([sys.executable,str(pathlib.Path(__file__).with_name('analyse_race_replay.py')),
            str(root/f'{name}.presentations.csv'),str(root/f'{name}.ticks.csv'),str(first),str(last),str(path)],text=True)
        (root/f'{name}.sections.csv').write_text(result)
        for section in csv.DictReader(io.StringIO(result)):
            sections.append({'run':name,**{k:float(v) if v!='N/A' else None for k,v in section.items()}})
    runs.append(row)
assert len(runs)>=6, 'Expected at least three traced/untraced pairs'
keys=sorted({(r['first'],r['last']) for r in sections})
medians=[]
for a,b in keys:
    rows=[s for s in sections if s['first']==a and s['last']==b]
    medians.append({k:(st.median([r[k] for r in rows if r[k] is not None]) if any(r[k] is not None for r in rows) else None)
                    for k in rows[0] if k!='run'})
output={'first':first,'last':last,'runs':runs,'section_medians':medians}
(root/'summary.json').write_text(json.dumps(output,indent=2)+'\n')
with (root/'section-medians.csv').open('w') as f:
    writer=csv.DictWriter(f,fieldnames=list(medians[0]));writer.writeheader();writer.writerows(medians)
for traced in (False,True):
    group=[r for r in runs if r['traced']==traced]
    print('traced' if traced else 'untraced', {k:round(st.median(float(r['rate'][k]) for r in group),3)
        for k in ['sim_hz','render_fps','sim_cpu_ms','presentation_cpu_ms','scene_wall_ms','publication_age_ms']})
print('Worst sections by median FPS:',[(int(r['first']),int(r['last']),r['presented_fps']) for r in sorted(medians,key=lambda r:r['presented_fps'])[:4]])
print('Results:',root/'summary.json',root/'section-medians.csv')
