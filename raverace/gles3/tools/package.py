#!/usr/bin/env python3
"""Create a local Knulli app from compiled binaries and user-ROM metadata."""

from pathlib import Path
import argparse, shutil, json, hashlib, subprocess

p = argparse.ArgumentParser()
p.add_argument("build", type=Path)
p.add_argument("kit", type=Path)
p.add_argument("output", type=Path)
a = p.parse_args()
root = Path(__file__).resolve().parents[2]
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=False)
for name, target in [
    ("rr_gles3", "rr_gles3"),
    ("rr_asset_builder", "rr_asset_builder"),
    ("session-io.so", "session-io.so"),
    ("smoke-chord.so", "smoke-chord.so"),
]:
    shutil.copyfile(a.build / "gles3" / name, out / target)
    (out / target).chmod(0o755)
for name in ["session.py", "clocks.py", "watchdog.py", "telemetry.py", "transport.py", "device-profile.json"]:
    shutil.copyfile(root / "platform/knulli-h700" / name, out / name)
shutil.copyfile(root / "gles3/assets/runtime_assets.py", out / "runtime_assets.py")
shutil.copytree(a.kit, out / "asset-kit")
manifest = {str(p.relative_to(out)): hashlib.sha256(p.read_bytes()).hexdigest() for p in out.rglob("*") if p.is_file()}
(out / "package-hashes.json").write_text(json.dumps(manifest, indent=2) + "\n")
print(out)
