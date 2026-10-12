#!/usr/bin/env python3
"""Validate audio/voice-state equivalence before reporting unprofiled C352 medians."""
import csv
import hashlib
import json
import pathlib
import re
import statistics
import sys

root = pathlib.Path(sys.argv[1])
rows, states, boards, videos, audio, activity = [], {}, {}, {}, {}, {}
expected = {(w, phase, trial, mode)
            for w in ('city', 'race')
            for phase, trials, modes in (('activity', (1,), ('baseline', 'fast')),
                                         ('performance', (1, 2, 3), ('baseline', 'fast')),
                                         ('profile', (1,), ('baseline', 'fast')))
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
    assert fields('[MEM-READER] ')['implementation'] == 'split'
    assert memory['fast'] == 'on' and memory['counters'] == 'off'
    chip = fields('[C352] ')
    assert chip['inactive_fastpath'] == ('on' if mode=='fast' else 'off')
    assert chip['counters'] == ('on' if phase=='activity' else 'off')
    assert boards.setdefault(workload, fields('[BOARD-STATE] ')) == fields('[BOARD-STATE] ')
    video = fields('[GEOMETRY] ')
    assert video['effective'] == 'on' and video['test_present_every'] == '0'
    digest = {k: video[k] for k in ('ticks', 'walks', 'direct_consumed', 'digest')}
    assert videos.setdefault(workload, digest) == digest
    row = dict(workload=workload, phase=phase, trial=trial, mode=mode, **rate,
               resyncs=clock['resyncs'], discarded_lag_ms=clock['discarded_lag_ms'],
               board_fnv1a32=fields('[BOARD-STATE] ')['fnv1a32'], state_sha256=hashlib.sha256(raw).hexdigest())
    row['RSS_peak_mib'] = float(fields('[MEMORY] ')['RSS_peak_kib']) / 1024
    if phase == 'activity':
        check = fields('[AUDIO-CHECK] ')
        assert audio.setdefault(workload, check) == check
        a = fields('[C352-ACTIVITY] ')
        assert int(a['intervals']) == (frame-1399)*16
        assert a['samples'] == check['samples'] and a['intervals'] == check['intervals']
        assert int(a['iterations']) == int(a['samples'])*32
        assert int(a['active'])+int(a['inactive']) == int(a['iterations'])
        assert int(a['mix_iterations']) == int(a['active'] if mode=='fast' else a['iterations'])
        assert int(a['zero_source_iterations']) == int(a['inactive'])+int(a['active_zero_source'])
        same = {k:v for k,v in a.items() if k!='mix_iterations'}
        hist = fields('[C352-HIST] ')
        for key,total in (('entry_active',int(a['intervals'])),('sample_active',int(a['samples']))):
            hs = [int(n) for n in hist[key].split(',')]
            assert len(hs)==33 and sum(hs)==total
        hs = [int(n) for n in hist['sample_active'].split(',')]
        assert sum(i*n for i,n in enumerate(hs)) == int(a['active'])
        assert activity.setdefault(workload,(same,hist)) == (same,hist)
        for key,value in a.items(): row['voice_'+key] = value
        for key,value in check.items(): row['audio_'+key] = value
        row.update(hist)
    else:
        assert not any(l.startswith(('[AUDIO-CHECK]', '[C352-ACTIVITY]')) for l in lines)
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
    for mode in ('baseline', 'fast'):
        rs = [r for r in rows if r['workload'] == workload and r['phase'] == 'performance' and r['mode'] == mode]
        print(workload, mode,
              *(f'{statistics.median(float(r[k]) for r in rs):.3f}' for k in ('sim_hz', 'render_fps', 'resyncs', 'discarded_lag_ms')),
              *(f'{min(float(r[k]) for r in rs):.3f}..{max(float(r[k]) for r in rs):.3f}' for k in ('sim_hz', 'render_fps')))
print('All 20 runs: PCM/voice state, activity distributions, simulation/board/video digests, zero faults and timing reconciliation validated:', root / 'comparison.tsv')
