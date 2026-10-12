#!/usr/bin/env python3
"""Real-ROM skipped-render state/pixel regression (native GLES2 required)."""
import json
import math
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
    baseline_capture = str(pathlib.Path(directory) / "baseline.ppm")
    baseline = run(gles, ["--unpaced", "--capture-frame", "600", "--capture", baseline_capture])
    assert json.loads(baseline.stdout) == state
    images = [pathlib.Path(baseline_capture).read_bytes()]
    for mode, options in (
        ("coupled", ["--unpaced"]),
        ("independent", ["--render-hz", "15"]),
        ("independent", ["--render-hz", "30", "--texture-budget-mib", "4"]),
    ):
        capture = str(pathlib.Path(directory) / (str(len(images))+".ppm"))
        result = run(gles, ["--profile-stages", "--timing", mode, *options, "--capture-frame", "600", "--capture", capture])
        assert json.loads(result.stdout) == state, "Timing changed emulated state"
        images.append(pathlib.Path(capture).read_bytes())
        timing = next(line for line in result.stderr.splitlines() if line.startswith("[TIMING] total"))
        assert "steps=600" in timing
        if mode == "independent":
            assert int(re.search(r"skipped=(\d+)", timing)[1]) > 200, timing
        cohorts = [line for line in result.stderr.splitlines() if line.startswith("[STAGES] total ")]
        assert sum(int(re.search(r"samples=(\d+)", line)[1]) for line in cohorts) == 600
        for line in cohorts:
            for name, value in re.findall(r"(\w+)=(-?[0-9]+\.[0-9]+)", line):
                assert math.isfinite(float(value)) and float(value) >= -0.01, (name, value)
            if "cohort=skipped" in line:
                for name in ("clip_build_state", "texture_lookup_decode", "texture_upload", "vertex_upload", "submission_other", "draw_call", "swap"):
                    assert re.search(r" " + name + r"=0\.000(?: |$)", line), line
                assert "avg_triangles=0.0 avg_draws=0.0 misses=0 evictions=0 texture_uploads=0" in line
        rate = next(line for line in result.stderr.splitlines() if line.startswith("[STAGES-RATE] total "))
        assert "steps=600" in rate
        print(timing)
        print(rate)
        for line in cohorts: print(line)
    assert all(image == images[0] for image in images), "Skipped presentations changed final pixels"
print("Coupled/independent 600-frame JSON and pixels identical; skipped renders and eviction exercised")
