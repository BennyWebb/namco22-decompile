#!/usr/bin/env python3
"""Five-second emulated sections using monotonic swap-return intervals.
No physical scanout, GPU execution or input-to-photon timing is implied.
Usage: analyse_race_replay.py PRESENTATIONS.csv TICKS.csv RACE_FIRST RACE_LAST [LOG]
"""
import csv
import math
import pathlib
import re
import statistics as st
import sys

def percentile(values, p):
    return sorted(values)[max(0, math.ceil(len(values)*p)-1)] if values else float('nan')
def mean(values):
    return st.mean(values) if values else float('nan')
rows=list(csv.DictReader(open(sys.argv[1])))
ticks={int(r['frame']):float(r['completed']) for r in csv.DictReader(open(sys.argv[2]))}
first,last=map(int,sys.argv[3:5])
assert 1<first<=last and first-1 in ticks and last in ticks, 'Range needs predecessor tick and final tick'
for r in rows:
    assert float(r['start'])<=float(r['presented'])<=float(r['end']), 'Mixed timestamp clocks'
# A trace starts at profile-from, but sampled drop counters include startup.
# The log exposes exact total/range counts, allowing that prefix to be removed.
prefix_drops=int(rows[0]['drops_cumulative']) if rows else 0
if len(sys.argv)>5:
    log=pathlib.Path(sys.argv[5]).read_text().splitlines()
    def log_fields(tag):
        return dict(re.findall(r"(\w+)=([^ ]+)",next(l for l in log if l.startswith(tag))))
    prefix_drops=int(log_fields('[THREAD-PERIOD]')['total_dropped'])-int(log_fields('[THREAD-RATE]')['dropped'])
intervals=[]
for previous,current in zip(rows,rows[1:]):
    intervals.append((int(current['frame']),float(current['presented'])-float(previous['presented']),
                      int(previous['frame']),float(current['presented'])))
# 300 ticks is 5.008 s, not exactly five wall-clock seconds. Tick-completion
# timestamps measure actual simulation rate; swap timestamps measure pacing.
writer=csv.writer(sys.stdout)
writer.writerow(['first','last','simulation_hz','presented_fps','interval_mean_ms','interval_median_ms',
                 'low1_fps','p99_ms','max_ms','over33ms','presented','unpresented_ticks',
                 'presentation_cpu_ms','scene_wall_ms','draw_wall_ms','submission_wall_ms','swap_wall_ms',
                 'draws','triangles','misses','uploads','evictions','texture_peak_mib','snapshot_age_ms','queue_ready_mean','queue_ready_peak','dropped_snapshots'])
def sections(first,last):
    a=first
    while a<=last:
        b=min(a+299,last)
        if last-b<150:b=last  # avoid a one-tick final percentile window
        yield a,b
        a=b+1
for a,b in sections(first,last):
    begin,end=ticks[a-1],ticks[b]
    chosen=[r for r in rows if begin<float(r['presented'])<=end and int(r['frame'])>=first]
    # Drop a boundary interval if its predecessor belongs to loading/outside
    # the explicitly selected race. Internal section boundaries retain it.
    dt=[d for f,d,p,t in intervals if begin<t<=end and p>=first and d>0]
    elapsed=ticks[b]-ticks[a-1]
    worst=sorted(dt,reverse=True)[:math.ceil(len(dt)*.01)]
    def column(name, scale=1): return mean([float(r[name])*scale for r in chosen])
    writer.writerow([a,b,round((b-a+1)/elapsed,3),round(len(chosen)/elapsed,3),
        round(mean(dt)*1000,3),round(st.median(dt)*1000,3) if dt else 'N/A',
        round(1/mean(worst),3) if worst else 'N/A',round(percentile(dt,.99)*1000,3),
        round(max(dt)*1000,3) if dt else 'N/A',sum(d>.033 for d in dt),len(chosen),b-a+1-len(chosen),
        round(column('cpu',1000),3),round(column('scene_wall',1000),3),round(column('draw_wall',1000),3),
        round(column('submission_wall',1000),3),round(column('swap_wall',1000),3),
        round(column('draws'),1),round(column('triangles'),1),sum(int(r['misses']) for r in chosen),
        sum(int(r['uploads']) for r in chosen),sum(int(r['evictions']) for r in chosen),
        round(max((int(r['texture_bytes']) for r in chosen),default=0)/1048576,2),
        round(column('publication_age',1000),3),round(column('queue_ready'),2),
        max((int(r['queue_ready']) for r in chosen),default=0),
        max((int(r['drops_cumulative']) for r in chosen),default=prefix_drops)-
        max((int(r['drops_cumulative']) for r in rows if float(r['presented'])<=begin),default=prefix_drops)])
