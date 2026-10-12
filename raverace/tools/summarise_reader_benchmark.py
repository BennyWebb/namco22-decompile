#!/usr/bin/env python3
"""Validate all three readers; report unprofiled A/B medians separately from samples."""
import csv
import hashlib
import json
import pathlib
import re
import statistics
import sys

root = pathlib.Path(sys.argv[1])
rows, states, boards, videos = [], {}, {}, {}
expected = {(w, phase, trial, mode)
            for w in ('city', 'race')
            for phase, trials, modes in (('reference', (1,), ('original',)),
                                         ('performance', (1, 2, 3), ('existing', 'split')),
                                         ('profile', (1,), ('existing', 'split')))
            for trial in trials for mode in modes}
seen = set()
for path in sorted(root.glob('*.log')):
    parts = path.stem.split('-')
    if len(parts) != 4:
        continue
    workload, phase, trial, mode = parts
    key = workload, phase, int(trial), mode
    if key not in expected:
        continue
    assert key not in seen
    seen.add(key)
    lines = path.read_text().splitlines()
    def fields(prefix):
        return dict(re.findall(r'(\w+)=([^ ]+)', next(l for l in reversed(lines) if l.startswith(prefix))))
    frame = 1700 if workload == 'city' else 3600
    raw = path.with_suffix('.json').read_bytes()
    state = json.loads(raw)
    assert path.with_suffix('.exit').read_text().strip() == '0'
    assert state['frames'] == frame and state['traps'] == state['unmapped'] == state['romwrites'] == 0
    assert not state['dsp_fault'] and not state['sound_fault']
    assert states.setdefault(workload, raw) == raw
    rate, clock, memory = fields('[SIM-RATE] '), fields('[GEOMETRY-RATE] '), fields('[MEM-READS] ')
    assert int(rate['steps']) == frame - 1399
    assert int(rate['rendered']) + int(rate['skipped']) == int(rate['steps'])
    assert fields('[MEM-READER] ')['implementation'] == mode
    assert memory['fast'] == ('off' if mode == 'original' else 'on') and memory['counters'] == 'off'
    assert boards.setdefault(workload, fields('[BOARD-STATE] ')) == fields('[BOARD-STATE] ')
    video = fields('[GEOMETRY] ')
    assert video['effective'] == 'on' and video['test_present_every'] == '0'
    digest = {k: video[k] for k in ('ticks', 'walks', 'direct_consumed', 'digest')}
    assert videos.setdefault(workload, digest) == digest
    row = dict(workload=workload, phase=phase, trial=trial, mode=mode, **rate,
               resyncs=clock['resyncs'], discarded_lag_ms=clock['discarded_lag_ms'],
               board_fnv1a32=fields('[BOARD-STATE] ')['fnv1a32'], state_sha256=hashlib.sha256(raw).hexdigest())
    row['RSS_peak_mib'] = float(fields('[MEMORY] ')['RSS_peak_kib']) / 1024
    if phase == 'profile':
        stats = fields('[SIM] ')
        assert int(stats['ticks']) == frame - 1399 and stats['overflow'] == '0'
        assert float(stats['reconciliation_max_us']) < 0.01
        for line in lines:
            if line.startswith('[SIM-STAGE] '):
                s = dict(re.findall(r'(\w+)=([^ ]+)', line))
                for k in ('avg_ms', 'median_ms', 'p95_ms', 'max_ms', 'percent'):
                    row[s['name'] + '_' + k] = s[k]
        assert fields('[SIM-SAMPLES] ')['overflow'] == '0'
    else:
        assert not any(l.startswith('[SIM-STAGE]') for l in lines)
    rows.append(row)
assert seen == expected, expected - seen
# Optional actual native JSON references (city, race); no hardcoded state oracle.
for workload, reference in zip(('city', 'race'), sys.argv[2:4]):
    assert json.loads(states[workload]) == json.loads(pathlib.Path(reference).read_bytes())
keys = list(dict.fromkeys(k for row in rows for k in row))
with (root / 'comparison.tsv').open('w') as f:
    writer = csv.DictWriter(f, keys, delimiter='\t')
    writer.writeheader()
    writer.writerows(rows)
print('workload mode median_sim_Hz median_render_FPS resyncs discarded_ms sim_Hz_range FPS_range')
for workload in ('city', 'race'):
    for mode in ('existing', 'split'):
        rs = [r for r in rows if r['workload'] == workload and r['phase'] == 'performance' and r['mode'] == mode]
        print(workload, mode,
              *(f'{statistics.median(float(r[k]) for r in rs):.3f}' for k in ('sim_hz', 'render_fps', 'resyncs', 'discarded_lag_ms')),
              *(f'{min(float(r[k]) for r in rs):.3f}..{max(float(r[k]) for r in rs):.3f}' for k in ('sim_hz', 'render_fps')))
print('All 18 runs: state, board/video digests, zero faults, configuration and timing reconciliation validated:', root / 'comparison.tsv')
