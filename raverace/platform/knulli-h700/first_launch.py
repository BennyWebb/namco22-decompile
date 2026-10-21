"""Strict release ROM verification and crash-safe whole-ROM compilation."""

import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
import zlib
from transport import child_guard


class SetupError(RuntimeError):
    pass


def digest(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as f:
        for chunk in iter(lambda: f.read(1048576), b""):
            h.update(chunk)
    return h.hexdigest()


def check_roms(roms, required):
    errors = []
    for name, identity in required.items():
        path = roms / name
        if not path.is_file():
            errors.append(f"Missing: {name}")
        else:
            data = path.read_bytes()
            if (
                len(data) != identity["bytes"]
                or hashlib.sha256(data).hexdigest() != identity["sha256"]
                or f"{zlib.crc32(data):08x}" != identity["crc32"]
            ):
                errors.append(f"Wrong: {name} (CRC {identity['crc32']})")
    if errors:
        raise SetupError("ROM check failed\n" + "\n".join(errors))


def stage(app, text):
    target = app / "logs/setup-stage.txt"
    temporary = target.with_suffix(".new")
    temporary.write_text(text)
    temporary.replace(target)


def verify_cache(target, receipt):
    identities = json.loads(receipt.read_text())
    actual = {str(p.relative_to(target)) for p in target.rglob("*") if p.is_file() and p != receipt}
    if actual != set(identities):
        raise ValueError("cache file list changed")
    for name, value in identities.items():
        p = target / name
        if p.stat().st_size != value["bytes"] or digest(p) != value["sha256"]:
            raise ValueError(name)


def ensure_cache(app, log, poll=lambda: None):
    checked_at = time.monotonic()
    stage(app, "Checking ROMs: 0%")
    required = json.loads((app / "launcher/roms.json").read_text())
    check_roms(app / "roms", required)
    stage(app, "Checking ROMs: 100%")
    print(f"[SETUP] rom_check_seconds={time.monotonic()-checked_at:.6f}", file=log, flush=True)
    sources = app / "launcher"
    identity = {
        str(p.relative_to(sources)): digest(p)
        for subdir in ("assets", "shaders")
        for p in sorted((sources / subdir).iterdir())
        if p.is_file()
    }
    identity["codec"] = digest(app / "bin/rr_asset_codec.so")
    key = hashlib.sha256(json.dumps([required, identity], sort_keys=True).encode()).hexdigest()
    target = app / "cache" / key
    receipt = target / "complete.json"
    if target.exists():
        try:
            stage(app, "Verifying asset cache: 0%")
            verify_cache(target, receipt)
        except (OSError, ValueError, KeyError) as error:
            raise SetupError(f"Asset cache damaged: {target.name}\nDelete cache contents and launch again.") from error
        stage(app, "Verifying asset cache: 100%")
        return target, False, 0.0
    # Incomplete generations are never read. Restart rather than trusting partial output.
    for incomplete in (app / "cache").glob("*.building"):
        shutil.rmtree(incomplete)
    temporary = target.with_name(key + ".building")
    env = dict(os.environ, PYTHONDONTWRITEBYTECODE="1", RR_BUILD_PROGRESS=str(app / "logs/setup-stage.txt"))
    env.pop("PYTHONPATH", None)
    start = time.monotonic()
    child = subprocess.Popen(
        [
            sys.executable,
            str(sources / "assets/stream.py"),
            str(app / "roms"),
            str(temporary),
            "--codec",
            str(app / "bin/rr_asset_codec.so"),
        ],
        env=env,
        stdout=log,
        stderr=log,
        start_new_session=True,
        preexec_fn=child_guard,
    )
    try:
        while child.poll() is None:
            poll()
            time.sleep(0.1)
        if child.returncode:
            current = (app / "logs/setup-stage.txt").read_text().splitlines()[0]
            raise SetupError(f"Asset compiler failed: {current}\nSee logs/setup.log.")
    finally:
        if child.poll() is None:
            child.terminate()
            try:
                child.wait(timeout=3)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait()
    stage(app, "Verifying completed build: 0%")
    manifest = {
        str(p.relative_to(temporary)): dict(bytes=p.stat().st_size, sha256=digest(p))
        for p in temporary.rglob("*")
        if p.is_file()
    }
    (temporary / "complete.json").write_text(json.dumps(manifest, sort_keys=True) + "\n")
    verify_cache(temporary, temporary / "complete.json")
    # Flush file data and the directory before publishing this generation.
    for p in temporary.rglob("*"):
        if p.is_file():
            with p.open("rb") as f:
                os.fsync(f.fileno())
    os.sync()
    temporary.rename(target)
    os.sync()
    stage(app, "Asset build complete: 100%")
    return target, True, time.monotonic() - start
