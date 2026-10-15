#!/usr/bin/env python3
"""Tabulate the unchanged Stage1 model; no new scheduling assumptions."""
import argparse,pathlib,json,csv
p=argparse.ArgumentParser();p.add_argument('evidence',type=pathlib.Path);a=p.parse_args();rows=[]
for f in sorted(a.evidence.glob('*/pipeline-model.json')):
    x=json.loads(f.read_text())
    for extra in ['0','0.1','0.2']:
        s=x['scenarios'][extra];model=s['fixed_3_period_latency']
        rows.append(dict(run=f.parent.name,cpu_contention=float(extra),period_ms=x['period_ms'],
                         late_pictures=model['late_3_periods'],
                         p99_service_ms=s['ideal_bottleneck_ms']['p99'],
                         p99_service_headroom_ms=x['period_ms']-s['ideal_bottleneck_ms']['p99'],
                         worst_service_ms=s['ideal_bottleneck_ms']['worst'],
                         modeled_output_fps=model['output_fps'],
                         note='Software target only; measured panel is slower. Validation runs are not performance evidence.' if 'validation' in f.parent.name else 'Software target only; measured panel is slower.'))
with (a.evidence/'pipeline-model-comparison.csv').open('w') as f:
    w=csv.DictWriter(f,fieldnames=list(rows[0]));w.writeheader();w.writerows(rows)
print('Model scenarios',len(rows))
