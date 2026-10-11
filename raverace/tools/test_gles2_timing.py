#!/usr/bin/env python3
"""Real-ROM skipped-render state/pixel regression (native GLES2 required)."""
import json
import pathlib
import re
import subprocess
import sys
import tempfile

gles, headless, roms = (str(pathlib.Path(p).resolve()) for p in sys.argv[1:4])
script = ["--frames", "600", "--coin", "180", "--gas", "200", "--steer", "300:2304"]
def run(binary, extra):
    result = subprocess.run([binary, roms, *script, *extra], capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stderr
    return result
state = json.loads(run(headless, []).stdout)
with tempfile.TemporaryDirectory(prefix="rr-timing-") as directory:
    images = []
    for mode, options in (
        ("coupled", ["--unpaced"]),
        ("independent", ["--render-hz", "15"]),
        ("independent", ["--render-hz", "30", "--texture-budget-mib", "4"]),
    ):
        capture = str(pathlib.Path(directory) / (str(len(images))+".ppm"))
        result = run(gles, ["--timing", mode, *options, "--capture-frame", "600", "--capture", capture])
        assert json.loads(result.stdout) == state, "Timing changed emulated state"
        images.append(pathlib.Path(capture).read_bytes())
        timing = next(line for line in result.stderr.splitlines() if line.startswith("[TIMING] total"))
        assert "steps=600" in timing
        if mode == "independent":
            assert int(re.search(r"skipped=(\d+)", timing)[1]) > 200, timing
        print(timing)
    assert images[0] == images[1] == images[2], "Skipped presentations changed final pixels"
print("Coupled/independent 600-frame JSON and pixels identical; skipped renders and eviction exercised")
