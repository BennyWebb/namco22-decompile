#!/usr/bin/env python3
"""Snapshot/thread/drop equivalence on one GPU; no hardcoded game oracle.

Usage: test_threaded_presentation.py THREADED_BINARY HEADLESS ROMS OUTPUT
Build RR_GLES_SUBMISSION_PROFILE=ON for ordered texture-event comparisons.
"""
import json
import os
import pathlib
import subprocess
import sys

binary, headless, roms, output = sys.argv[1:]
output = pathlib.Path(output)
output.mkdir(parents=True, exist_ok=True)
env = dict(os.environ, GLES_TEXTURE_TRACE="1")
for frame in (600, 1700, 3600):
    script = [roms, "--frames", str(frame), "--coin", "180", "--gas", "200",
              "--steer", "300:2304", "--steer", "700:2048"]
    reference = subprocess.run([headless, *script], capture_output=True, text=True, check=True)
    results = {}
    for name, options in (
        ("baseline", ["--presentation-thread", "off", "--test-present-every", "3"]),
        ("snapshot", ["--snapshot-reference", "--test-present-every", "3"]),
        ("threaded", ["--test-present-every", "3"]),
        ("dropping", []),
    ):
        image = output / f"{frame}-{name}.ppm"
        p = subprocess.run([binary, *script, "--timing", "independent",
                            "--skip-geometry-on-dropped-frames", "on",
                            "--texture-budget-mib", "64", "--vbo-upload", "frame",
                            "--audio-checksum-from", "1", "--capture-frame", str(frame),
                            "--capture", str(image), *options], env=env,
                           capture_output=True, text=True, timeout=360)
        (output / f"{frame}-{name}.json").write_text(p.stdout)
        (output / f"{frame}-{name}.log").write_text(p.stderr)
        assert p.returncode == 0, p.stderr
        assert json.loads(p.stdout) == json.loads(reference.stdout), (frame, name)
        lines = p.stderr.splitlines()
        tags = ("[AUDIO-CHECK]", "[BOARD-STATE]", "[PRESENTATION-GEOMETRY]")
        checks = tuple(next(l for l in lines if l.startswith(tag)) for tag in tags)
        # Presentation selection differs; hardware bookkeeping must not.
        geometry = next(l.split(" ticks=", 1)[1] for l in lines if l.startswith("[GEOMETRY]"))
        textures = next(l for l in lines if l.startswith("[TEXTURE-EVENTS]"))
        results[name] = image.read_bytes(), checks, geometry, textures
    for name in ("snapshot", "threaded", "dropping"):
        assert results[name][:3] == results["baseline"][:3], (frame, name)
    # Dropped pictures legitimately produce a different cache-event stream.
    assert results["snapshot"][3] == results["threaded"][3] == results["baseline"][3], frame
    print(frame, "pixels, ordered geometry, PCM/voices, board/video and simulation identical;"
          " fixed-presentation texture events identical", flush=True)
