#!/usr/bin/env python3
"""Run real-ROM determinism/input tests; no hardcoded game-state oracle."""
import json
import os
import pathlib
import subprocess
import sys

binary, roms = map(lambda p: str(pathlib.Path(p).resolve()), sys.argv[1:3])


def run(arguments, env=None):
    result = subprocess.run([binary, roms, *arguments], capture_output=True,
                            text=True, env=env, timeout=60)
    assert result.returncode == 0, result.stderr
    state = json.loads(result.stdout)
    assert state["frames"] == 600 and state["slices"] == 9600
    assert state["instructions_68k"] > 0 and state["dsp_steps"] > 0
    assert state["sound_cycles"] > 0
    assert state["traps"] == 0 and not state["dsp_fault"] and not state["sound_fault"]
    return state


script = ["--frames", "600", "--coin", "180", "--gas", "200", "--steer", "300:2304"]
first = run(script)
assert first == run(script), "Repeated runs differ"
hostile = dict(os.environ, RR_RD="check", RR_POKE="1:10000000:4:12345678",
               RR_NET_SERVER="127.0.0.1:1", RR_PACE="1", RR_EEPROM_FILL="1")
assert first == run(script, hostile), "Desktop environment changed the test"
idle = run(["--frames", "600"])
assert idle["fnv1a32"]["wram"] != first["fnv1a32"]["wram"], "Scripted input did not affect RAM"
for arguments in (["--frames", "0"], ["--frames", "-1"], ["--frames"],
                  ["--frames", "12junk"], ["--window"], ["--steer", "1:4096"]):
    result = subprocess.run([binary, roms, *arguments], capture_output=True, timeout=10)
    assert result.returncode == 2 and not result.stdout, arguments
print("Repeated state, environment isolation, effective input, counters and CLI checks passed")
