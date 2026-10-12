#!/usr/bin/env python3
"""Real-ROM A/B with deterministic presentation schedule; not a speed benchmark."""
import json
import pathlib
import re
import subprocess
import sys

gles, headless, roms, output = (pathlib.Path(p).resolve() for p in sys.argv[1:5])
output.mkdir(parents=True, exist_ok=True)

def run(binary, args, label):
    result = subprocess.run([str(binary), str(roms), *args], capture_output=True, text=True, timeout=180)
    (output / (label + ".log")).write_text(result.stderr)
    (output / (label + ".json")).write_text(result.stdout)
    assert result.returncode == 0, result.stderr
    state = json.loads(result.stdout)
    assert state["traps"] == state["unmapped"] == state["romwrites"] == 0
    assert not state["dsp_fault"] and not state["sound_fault"]
    return state, result.stderr

for frame, budget in ((600, 64), (1400, 64), (1700, 64), (1700, 4)):
    label = f"f{frame}-b{budget}"
    script = ["--frames", str(frame), "--coin", "180", "--gas", "200", "--steer", "300:2304", "--steer", "700:2048"]
    reference, _ = run(headless, script, label + "-headless")
    images, digests = [], []
    for mode in ("off", "on"):
        name = label + "-" + mode
        capture = output / (name + ".ppm")
        state, log = run(gles, script + ["--timing", "independent", "--test-present-every", "3",
            "--skip-geometry-on-dropped-frames", mode, "--profile-stages", "--texture-budget-mib", str(budget),
            "--capture-frame", str(frame), "--capture", str(capture)], name)
        assert state == reference, name
        images.append(capture.read_bytes())
        geometry = next(line for line in log.splitlines() if line.startswith("[GEOMETRY]"))
        digests.append(geometry.split(" ticks=")[1])
        assert f"ticks={frame} " in geometry
        skipped = next(line for line in log.splitlines() if line.startswith("[STAGES] total ") and "cohort=skipped" in line)
        assert int(re.search(r"samples=(\d+)", skipped)[1]) > frame//2
        if mode == "on":
            for field in ("direct_decode", "list_decode", "sort"):
                assert f" {field}=0.000 " in skipped, skipped
            assert "avg_quads=0.0" in skipped, skipped
    assert images[0] == images[1], "Skipped geometry changed pixels: " + label
    assert digests[0] == digests[1], "Video latch/command consumption differs: " + label
    print(label, "state, pixels and video-tick digest identical", flush=True)

# Coupled behaviour is unaffected even if the opt-in flag is supplied.
script = ["--frames", "600", "--coin", "180", "--gas", "200", "--steer", "300:2304"]
images = []
for mode in ("off", "on"):
    capture = output / ("coupled-" + mode + ".ppm")
    state, log = run(gles, script + ["--unpaced", "--skip-geometry-on-dropped-frames", mode,
        "--capture-frame", "600", "--capture", str(capture)], "coupled-" + mode)
    assert "effective=off" in log
    images.append((state, capture.read_bytes()))
assert images[0] == images[1]
print("Coupled state and pixels identical; fixed-pattern skipped-geometry regression passed")
