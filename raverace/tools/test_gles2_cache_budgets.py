#!/usr/bin/env python3
"""Real-ROM runtime-budget/accounting/state and screenshot regression."""
import json
import contextlib
import pathlib
import re
import subprocess
import sys
import tempfile

gles, headless, roms = (str(pathlib.Path(p).resolve()) for p in sys.argv[1:4])
script = ['--frames', '1700', '--coin', '180', '--gas', '200', '--steer', '300:2304', '--steer', '700:2048']
def run(binary, args):
    return subprocess.run([binary, roms, *args], capture_output=True, text=True, timeout=120)
reference = run(headless, script)
assert reference.returncode == 0, reference.stderr
state = json.loads(reference.stdout)
assert state['traps'] == 0 and not state['dsp_fault'] and not state['sound_fault']
output = pathlib.Path(sys.argv[4]).resolve() if len(sys.argv) > 4 else None
if output: output.mkdir(parents=True, exist_ok=True)
with (contextlib.nullcontext(str(output)) if output else tempfile.TemporaryDirectory(prefix='rr-cache-budget-test-')) as directory:
    images = []
    for budget in (32, 64, 96, 128):
        capture = pathlib.Path(directory) / f'{budget}.ppm'
        result = run(gles, [*script, '--timing', 'coupled', '--unpaced', '--texture-budget-mib', str(budget),
                            '--profile-memory', '--capture-frame', '1700', '--capture', str(capture)])
        assert result.returncode == 0, result.stderr
        assert json.loads(result.stdout) == state
        reports = re.findall(r'\[TEX\] decoded=(\d+) uploads=(\d+) GPU=(\d+) peak=(\d+) budget=(\d+)', result.stderr)
        assert reports and '[MEMORY] RSS_kib=' in result.stderr
        for _, _, resident, peak, limit in reports:
            assert int(limit) == budget * 1048576 and int(resident) <= int(peak) <= int(limit)
        images.append(capture.read_bytes())
        print(f'{budget} MiB: identical state, bounded GPU allocation; {reports[-1]}', flush=True)
    # Existing sticky resolution tiers can survive longer at larger budgets.
    # Preserve that policy; quantify differences instead of assuming equal LOD.
    for budget, image in zip((32, 64, 96, 128), images):
        delta = sum(image[i:i+3] != images[1][i:i+3] for i in range(15, len(image), 3))
        print(f'{budget} MiB: {delta} frame-1700 pixels differ from 64 MiB', flush=True)
    assert images[2] == images[3], '96/128 no-eviction workloads differ'
    default_capture = pathlib.Path(directory) / 'default.ppm'
    default_scene = run(gles, [*script, '--unpaced', '--capture-frame', '1700', '--capture', str(default_capture)])
    assert default_scene.returncode == 0 and json.loads(default_scene.stdout) == state
    assert default_capture.read_bytes() == images[1], 'Default rendering differs from explicit 64 MiB'

default = run(gles, ['--frames', '120', '--unpaced'])
assert default.returncode == 0 and re.search(r'\[TEX\] decoded=.*budget=67108864 ', default.stderr), default.stderr
for invalid in ('0', '129', '-32', 'x'):
    result = run(gles, ['--texture-budget-mib', invalid])
    assert result.returncode == 2 and 'invalid budget' in result.stderr
print('Four budgets, unchanged state/default pixels, 96/128 pixel identity, LOD differences reported, bounded memory and invalid limits passed')
