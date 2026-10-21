#!/usr/bin/env python3
"""Full diagnostics must be opt-in and preserve game state and sampled pixels exactly."""

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys

from recordings import host_environment

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--build", type=Path, required=True)
p.add_argument("--roms", type=Path, required=True)
p.add_argument("--cache", type=Path, required=True)
p.add_argument("--eeprom", type=Path, required=True)
p.add_argument("--output", type=Path, required=True)
a = p.parse_args()
out = a.output.resolve()
out.mkdir()
prefixes = [
    "[RENDER-COVERAGE]",
    "[MATERIAL-UV-UPLOAD]",
    "[DYNAMIC_CACHE-UPLOAD]",
    "[UPLOAD-STAGING]",
    "[MATERIAL_CACHE-CACHE]",
    "[DYNAMIC-CACHE-PREP]",
]
for target in ["rr_gles3", "rr_gles3_validation"]:
    states = []
    images = []
    for mode in ["summary", "full"]:
        d = out / (target + "-" + mode)
        d.mkdir()
        (d / "images").mkdir()
        env = host_environment(a.cache)
        env["ENG_GLES3_INITIAL_EEPROM"] = str(a.eeprom.resolve())
        if target.endswith("_validation"):
            env.update(
                ENG_GLES3_CAPTURE="hash-only",
                ENG_GLES3_IMAGES=str(d / "images"),
                ENG_GLES3_IMAGE_HASH=str(d / "images.csv"),
                ENG_GLES3_CAPTURE_TICKS="1,159,300,435,600",
            )
        cmd = [
            str((a.build / "gles3" / target).resolve()),
            str(a.roms.resolve()),
            "--frames",
            "600",
            "--audio",
            "off",
            "--overlay",
            "off",
            "--coin",
            "180",
            "--gas",
            "200",
            "--steer",
            "300:2304",
        ]
        if mode == "full":
            cmd += ["--thread-profile", str(d / "thread.csv")]
        r = subprocess.run(cmd, env=env, check=True, text=True, capture_output=True)
        (d / "stderr.log").write_text(r.stderr)
        for prefix in prefixes:
            assert (prefix in r.stderr) == (mode == "full"), (target, mode, prefix)
        states.append(json.loads(r.stdout))
        if target.endswith("_validation"):
            images.append((d / "images.csv").read_bytes())
    assert states[0] == states[1], target
    if images:
        assert images[0] == images[1], target
(out / "result.json").write_text(json.dumps(dict(exact=True, summary_has_no_picture_diagnostics=True)) + "\n")
print("PASS summary/full: identical game state and sampled images; no per-picture diagnostics in normal mode")
