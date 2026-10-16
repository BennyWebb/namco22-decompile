#!/usr/bin/env python3
"""Generate review tables from complete retained P2 populations, never guessed values."""
import argparse,pathlib,json,csv
p=argparse.ArgumentParser();p.add_argument('root',type=pathlib.Path);p.add_argument('destination',type=pathlib.Path);a=p.parse_args();o=a.destination;o.mkdir(exist_ok=True,parents=True)
names=[n for i in [1,2,3] for n in [f'baseline-{i}',f'v2-play-{i}',f'final-{i}']]
def summary(n):return json.loads((a.root/n/'p2-summary.json').read_text())
def num(x):return f'{x:.3f}'
lines=['| Run | Native prep median/p95/p99/worst ms | Dense prep median/p95/p99/worst ms |','|---|---|---|']
status=['| Run | Full native / settled underrun frames | Inferred repeats | Main gaps >50ms | Release tick | Peak °C | CPU flag |','|---|---:|---:|---:|---:|---:|---|']
for n in names:
 s=summary(n);v=s['metrics_ms'];lines.append('| '+n+' | '+' | '.join('/'.join(num(v[w]['prep_wall'][q]) for q in ['median','p95','p99','worst']) for w in ['native','dense'])+' |')
 status.append(f"| {n} | {s['native_underrun_frames']} / {s['settled_underrun_frames']} | {s['inferred_repeats']} | {s['main_gaps50']} | {s['release_tick']} | {s['peak_native_temperature_c']:.2f} | {s['clock_flagged']} |")
(o/'headline-preparation.md').write_text('\n'.join(lines)+'\n');(o/'headline-outcomes.md').write_text('\n'.join(status)+'\n')
# Compact stage tails, retaining per-run/per-window full data in service-timings.csv.
lines=['| Run | Window | Compiler median/p95/p99/worst ms | Walk median/p95/p99/worst ms |','|---|---|---|---|']
for n in names:
 s=summary(n)
 for w in ['native','dense']:lines.append('| '+n+' | '+w+' | '+' | '.join('/'.join(num(s['metrics_ms'][w][k][q]) for q in ['median','p95','p99','worst']) for k in ['native_compile_ms','native_walk_ms'])+' |')
(o/'headline-compiler-walk.md').write_text('\n'.join(lines)+'\n')
lines=['| Run | Swap intervals mean/median/p95/p99/worst ms | Snapshot→swap median/p95/p99/worst ms |','|---|---|---|']
for n in names:
 s=summary(n);lines.append('| '+n+' | '+' / '.join(num(s['swap_interval_ms'][q]) for q in ['mean','median','p95','p99','worst'])+' | '+' / '.join(num(s['metrics_ms']['native']['publication_age'][q]) for q in ['median','p95','p99','worst'])+' |')
(o/'headline-presentation.md').write_text('\n'.join(lines)+'\n')
# Thermal/resource table for each complete soak, including a required lower-clock repeat.
for path in sorted(a.root.glob('soak-*/soak-summary.json')):
 s=json.loads(path.read_text());stem=path.parent.name
 lines=['| 5-minute bucket (s) | Native exposure s | Repeats | Underrun frames native/settled | Main gaps >50ms | Peak °C |','|---|---:|---:|---:|---:|---:|']
 for b in s['buckets']:lines.append(f"| {b['start_seconds']:.0f}–{b['end_seconds']:.0f} | {b['native_seconds']:.1f} | {b['repeats']} | {b['native_underrun_frames']} / {b['settled_underrun_frames']} | {b['main_gaps50']} | {b['temperature']['worst']:.2f} |")
 (o/(stem+'-buckets.md')).write_text('\n'.join(lines)+'\n')
 rows=[]
 for l in s['laps']:
  rows.append(dict(lap=l['lap'],peak_rss_kib=l['observed_peak_rss_kib'],peak_hwm_kib=l['observed_peak_hwm_kib'],minimum_available_kib=l['minimum_available_kib'],minimum_cma_free_kib=l['minimum_cma_free_kib'],pgmajfault=l['vm_delta'].get('pgmajfault'),pgscan_deltas=json.dumps({k:v for k,v in l['vm_delta'].items() if k.startswith('pgscan')}),pgsteal_deltas=json.dumps({k:v for k,v in l['vm_delta'].items() if k.startswith('pgsteal')}),allocstall_deltas=json.dumps({k:v for k,v in l['vm_delta'].items() if k.startswith('allocstall')}),process_read_bytes=l['process_io_delta'].get('read_bytes'),native_repeats=l['inferred_repeats'],native_settled_underrun_frames=l['settled_underrun_frames'],main_gaps50=l['main_gaps50'],clock_flagged=l['clock_flagged']))
 with (o/(stem+'-per-lap.csv')).open('w') as f:w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
print('Generated complete-population report tables')
