#!/usr/bin/env python3
"""Real-ROM record/playback regression; existing scripted fixture, not a race."""
import json
import pathlib
import subprocess
import sys
import tempfile

binary, roms = [str(pathlib.Path(x).resolve()) for x in sys.argv[1:3]]
with tempfile.TemporaryDirectory(prefix='rr-replay-test-') as work:
    root = pathlib.Path(work)
    def run(args, success=True):
        p = subprocess.run([binary, roms, *args], capture_output=True, text=True, timeout=180)
        assert (p.returncode == 0) == success, p.stderr
        return p
    recording = root/'fixture.rec'
    baseline = run(['--frames', '600', '--coin', '180', '--gas', '200', '--steer', '300:2304',
                    '--record-input', str(recording)])
    assert recording.with_suffix('.rec.eeprom').stat().st_size == 8192
    for i in range(2):
        checkpoints = root/f'play{i}.states'
        p = run(['--replay-input', str(recording), '--input-checkpoints', str(checkpoints)])
        assert json.loads(baseline.stdout) == json.loads(p.stdout)
        assert checkpoints.read_bytes() == pathlib.Path(str(recording)+'.states.jsonl').read_bytes()
    run(['--replay-input', str(recording), '--gas', '200'], False)
    run(['--frames', '1', '--record-input', str(recording)], False)  # never overwrite
    meta = pathlib.Path(str(recording)+'.meta')
    original = meta.read_bytes(); meta.write_bytes(original.replace(b'TIMING 25600000/427350', b'TIMING 25600000/427351'))
    run(['--replay-input', str(recording)], False)
    meta.write_bytes(original)
    eeprom = pathlib.Path(str(recording)+'.eeprom'); original_seed=eeprom.read_bytes()
    eeprom.write_bytes(bytes([original_seed[0]^1])+original_seed[1:])
    run(['--replay-input', str(recording)], False); eeprom.write_bytes(original_seed)
    original_events=recording.read_bytes()
    recording.write_bytes(original_events+b'# MARK 550 1\n')
    assert json.loads(run(['--replay-input',str(recording)]).stdout)==json.loads(baseline.stdout)
    recording.write_bytes(original_events.replace(b'\n',b'\r\n'))
    assert json.loads(run(['--replay-input',str(recording)]).stdout)==json.loads(baseline.stdout)
    recording.write_bytes(original_events)
    for bad in (b'F 1 0000 1000 000 000\n', b'F 10 FEFF 800 000 000\nF 9 FEFF 800 000 000\n',
                b'F 1 FEFF 800 000 000\nBROKEN\n'):
        recording.write_bytes(bad)
        run(['--replay-input', str(recording)], False)
    recording.write_bytes(original_events)
print('Record/replay state + board + PCM/voice checkpoints, no-overwrite and malformed/mismatched session checks passed')
