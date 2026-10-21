#!/usr/bin/env python3
"""Real-ROM interrupt/restart/cache-integrity and calibrated setup checks on the host."""

import argparse
import json
import os
from pathlib import Path
import shutil
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "platform/knulli-h700"))
sys.path.insert(0, str(Path(__file__).resolve().parent))
from first_launch import SetupError, ensure_cache
from calibration import generate, EXPECTED
from recordings import host_environment

p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--build", type=Path, required=True)
p.add_argument("--roms", type=Path, required=True)
p.add_argument("--output", type=Path, required=True)
a = p.parse_args()
app = a.output.resolve()
app.mkdir()
for name in ("launcher", "bin", "logs", "cache", "saves"):
    (app / name).mkdir()
(app / "roms").symlink_to(a.roms.resolve(), target_is_directory=True)
for name in ("assets", "shaders"):
    shutil.copytree(ROOT / "gles3" / name, app / "launcher" / name, ignore=shutil.ignore_patterns("__pycache__"))
shutil.copyfile(ROOT / "platform/knulli-h700/roms.json", app / "launcher/roms.json")
shutil.copyfile(a.build / "gles3/rr_asset_codec.so", app / "bin/rr_asset_codec.so")
started = time.monotonic()


def interrupt():
    if "decode" in (app / "logs/setup-stage.txt").read_text():
        raise SetupError("fixture cancellation")


with (app / "logs/interrupted.log").open("w") as log:
    try:
        ensure_cache(app, log, interrupt)
    except SetupError as e:
        assert "fixture cancellation" in str(e)
    else:
        raise AssertionError("interruption did not occur")
assert not any(p.name.endswith(".json") for p in (app / "cache").iterdir())
assert all(p.name.endswith(".building") for p in (app / "cache").iterdir())


def interrupt_verification():
    if "Verifying completed build" in (app / "logs/setup-stage.txt").read_text():
        raise SetupError("fixture verification cancellation")


with (app / "logs/interrupted-verification.log").open("w") as log:
    try:
        ensure_cache(app, log, interrupt_verification)
    except SetupError as error:
        assert "fixture verification cancellation" in str(error)
    else:
        raise AssertionError("verification interruption did not occur")
assert all(p.name.endswith(".building") for p in (app / "cache").iterdir())

with (app / "logs/setup.log").open("w") as log:
    cache, built, seconds = ensure_cache(app, log)
assert built and not list((app / "cache").glob("*.building"))
with (app / "logs/hit.log").open("w") as log:
    assert ensure_cache(app, log)[1] is False
with (app / "logs/calibration.log").open("w") as log:
    seed = generate(
        (a.build / "gles3/rr_gles3_setup").resolve(),
        a.roms.resolve(),
        cache,
        app / "saves/rr_eeprom.nv",
        host_environment(cache),
        log,
    )
import hashlib

assert hashlib.sha256(seed.read_bytes()).hexdigest() == EXPECTED
with (cache / "materials.bin").open("r+b") as f:
    f.write(b"bad")
with (app / "logs/damaged.log").open("w") as log:
    try:
        ensure_cache(app, log)
    except SetupError as e:
        assert "Asset cache damaged" in str(e)
    else:
        raise AssertionError("damaged cache accepted")
result = dict(
    exact=True,
    restarted=True,
    cache_corruption_rejected=True,
    calibrated_sha256=EXPECTED,
    asset_compile_seconds=seconds,
    total_seconds=time.monotonic() - started,
)
(app / "result.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps(result))
