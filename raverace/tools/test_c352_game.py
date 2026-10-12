#!/usr/bin/env python3
"""Actual translated game: four-channel PCM + per-interval C352 state and pixels."""
import json
import pathlib
import re
import subprocess
import sys

baseline, fast, headless, roms, out = (str(pathlib.Path(p).resolve()) for p in sys.argv[1:6])
root = pathlib.Path(out)
root.mkdir(parents=True, exist_ok=True)
for frame in (600, 1700, 3600):
    script = ['--frames', str(frame), '--coin', '180', '--gas', '200',
              '--steer', '300:2304', '--steer', '700:2048']
    ref = subprocess.run([headless, roms, *script], capture_output=True, text=True, check=True)
    images, digests = [], []
    for name, binary in [('baseline', baseline), ('fast', fast)]:
        image = root / f'{frame}-{name}.ppm'
        # All samples since reset (plus ranged audio checks in physical benchmarks).
        args = [binary, roms, *script, '--audio-checksum-from', '1',
                '--timing', 'independent', '--test-present-every', '3',
                '--skip-geometry-on-dropped-frames', 'on', '--texture-budget-mib', '64',
                '--vbo-upload', 'frame', '--capture-frame', str(frame), '--capture', str(image)]
        r = subprocess.run(args, capture_output=True, text=True, timeout=240)
        (root / f'{frame}-{name}.log').write_text(r.stderr)
        (root / f'{frame}-{name}.json').write_text(r.stdout)
        assert r.returncode == 0, r.stderr
        state = json.loads(r.stdout)
        assert state == json.loads(ref.stdout)
        assert state['traps'] == state['unmapped'] == state['romwrites'] == 0
        assert not state['dsp_fault'] and not state['sound_fault']
        assert f'[C352] inactive_fastpath={"on" if name=="fast" else "off"} counters=off' in r.stderr
        assert '[MEM-READER] implementation=split' in r.stderr
        images.append(image.read_bytes())
        digests.append((next(l for l in r.stderr.splitlines() if l.startswith('[AUDIO-CHECK]')),
                        next(l for l in r.stderr.splitlines() if l.startswith('[BOARD-STATE]')),
                        next(l.split(' ticks=')[1] for l in r.stderr.splitlines() if l.startswith('[GEOMETRY]'))))
    assert images[0] == images[1], frame
    assert digests[0] == digests[1], frame
    print(frame, 'identical native state, all PCM/state checks, pixels, board/video digests', flush=True)
print('Real game C352 equivalence passed')
