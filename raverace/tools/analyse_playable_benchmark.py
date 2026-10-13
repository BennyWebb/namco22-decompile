#!/usr/bin/env python3
"""Summarise integrated-feature overhead; never treat the wall tail as racing FPS.
Usage: analyse_playable_benchmark.py RESULTS_DIRECTORY
"""
import csv
import json
import math
import pathlib
import re
import statistics as stats
import sys

root = pathlib.Path(sys.argv[1])
def fields(line):
    return dict(re.findall(r"(\w+)=([^ ]+)", line))
def percentile(values, p):
    return sorted(values)[max(0, math.ceil(len(values)*p)-1)]
def monitored(path, begin, end):
    samples = []
    for line in path.read_text().splitlines():
        if line.startswith('SAMPLE '):
            samples.append({'time': float(line.split()[1]), 'cores': {}, 'rss': 0,
                            'available': 0, 'frequency': [], 'temperatures': []})
        elif samples:
            s = samples[-1]
            if re.match(r'cpu[0-3] ', line):
                words = line.split(); values = list(map(int, words[1:9]))
                s['cores'][words[0]] = (sum(values), values[3]+values[4])
            elif line.startswith('VmRSS:'): s['rss'] = int(line.split()[1])
            elif line.startswith('MemAvailable:'): s['available'] = int(line.split()[1])
            elif 'scaling_cur_freq ' in line: s['frequency'].append(int(line.split()[-1]))
            elif '/thermal_zone' in line and '/temp ' in line: s['temperatures'].append(int(line.split()[-1]))
    totals = {f'cpu{i}': [0, 0] for i in range(4)}
    selected = [s for s in samples if begin <= s['time'] <= end]
    for a, b in zip(samples, samples[1:]):
        if not begin <= (a['time']+b['time'])/2 <= end: continue
        for core, sums in totals.items():
            if core in a['cores'] and core in b['cores']:
                for j in range(2): sums[j] += b['cores'][core][j]-a['cores'][core][j]
    return {'core_cpu_percent': [100*(1-idle/total) if total else None for total, idle in totals.values()],
            'rss_peak_kib': max((s['rss'] for s in selected), default=None),
            'mem_available_min_kib': min((s['available'] for s in selected), default=None)}
def load(name):
    base = root/name
    assert base.with_suffix('.exit').read_text().strip() == '0', name
    state = json.loads(base.with_suffix('.json').read_text())
    assert state['traps'] == state['unmapped'] == state['romwrites'] == 0, name
    assert not state['dsp_fault'] and not state['sound_fault'], name
    result = {'state': state}
    for line in base.with_suffix('.log').read_text().splitlines():
        for prefix, key in (('[THREAD-RATE]', 'rate'), ('[THREAD-PERIOD]', 'period'),
                            ('[PROFILE] total', 'profile'), ('[PROFILE-CACHE] total', 'cache'),
                            ('[VIDEO-PROFILE]', 'video'), ('[THREAD-DETAIL]', 'detail')):
            if line.startswith(prefix): result[key] = fields(line)
    period = result['period']; begin, end = float(period['start']), float(period['end'])
    rows = [r for r in csv.DictReader(base.with_suffix('.csv').open()) if float(r['end']) <= end]
    for row in rows:
        if 'presented' in row:
            assert float(row['start']) <= float(row['presented']) <= float(row['end']), (name, 'mixed presentation clock')
    result['presentation_cpu_ms'] = {
        'mean': stats.mean(float(r['cpu'])*1000 for r in rows),
        'median': stats.median(float(r['cpu'])*1000 for r in rows),
        'p95': percentile([float(r['cpu'])*1000 for r in rows], .95)}
    times = [float(r.get('presented', r['end'])) for r in rows]
    intervals = [b-a for a, b in zip(times, times[1:])]
    if intervals:
        worst = sorted(intervals, reverse=True)[:math.ceil(len(intervals)*.01)]
        result['presentation_intervals'] = {'samples': len(intervals), 'low1_fps': 1/stats.mean(worst),
            'mean_ms': stats.mean(intervals)*1000, 'median_ms': stats.median(intervals)*1000,
            'p95_ms': percentile(intervals,.95)*1000, 'p99_ms': percentile(intervals,.99)*1000,
            'timestamp': 'swap_return' if 'presented' in rows[0] else 'old_render_completion_proxy'}
    result['monitor'] = monitored(base.with_suffix('.monitor'), begin, end)
    return result

runs = {}; reference = None
for trial in range(1, 6):
    for mode in ('previous', 'playable', 'compact', 'expanded'):
        name = f'dense-{trial}-{mode}'; r = runs[name] = load(name)
        reference = r['state'] if reference is None else reference
        assert r['state'] == reference, name
print('| Dense opening: five-run median | Simulation Hz | Render FPS | Present CPU ms | 1% low FPS | p95 interval ms |')
print('|---|---:|---:|---:|---:|---:|')
summary = {}
for mode in ('previous', 'playable', 'compact', 'expanded'):
    group = [runs[f'dense-{n}-{mode}'] for n in range(1,6)]
    def median(key, field): return stats.median(float(r[key][field]) for r in group)
    values = {'sim_hz': median('rate','sim_hz'), 'render_fps': median('rate','render_fps'),
              'presentation_cpu_ms': stats.median(r['presentation_cpu_ms']['mean'] for r in group),
              'low1_fps': median('presentation_intervals','low1_fps'),
              'p95_interval_ms': median('presentation_intervals','p95_ms'),
              'render_fps_range': [min(float(r['rate']['render_fps']) for r in group),
                                   max(float(r['rate']['render_fps']) for r in group)]}
    summary[mode] = values
    print(f"| {mode} | {values['sim_hz']:.3f} | {values['render_fps']:.3f} | {values['presentation_cpu_ms']:.3f} | {values['low1_fps']:.2f} | {values['p95_interval_ms']:.2f} |")
for name in ('dense-video-profile', 'extended-playable', 'stability-playable'):
    if (root/f'{name}.exit').exists(): runs[name] = load(name)
(root/'summary.json').write_text(json.dumps({'summary': summary, 'runs': runs}, indent=2)+'\n')
print('Previous interval metrics use its older completion timestamp; integrated runs use recorded swap returns.')
print('Dense 1% lows use available intervals in the 301-tick window, not a full live 300-interval window.')
print('Extended/stability runs are regression workloads and contain stationary wall-facing periods.')
