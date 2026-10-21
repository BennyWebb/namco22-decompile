#!/usr/bin/env python3
"""Build a redistributable, ROM-free, single-folder Knulli release ZIP."""

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def build_release(build, output, version):
    build, output = build.resolve(), output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    platform = ROOT / "platform/knulli-h700"
    with tempfile.TemporaryDirectory(prefix="release-", dir=output) as directory:
        tree = Path(directory)
        app = tree / "rave-racer"
        for name in ("bin", "launcher", "roms", "cache", "saves", "logs"):
            (app / name).mkdir(parents=True)
        shutil.copyfile(platform / "Rave Racer.sh", tree / "Rave Racer.sh")
        (tree / "Rave Racer.sh").chmod(0o755)
        for name in ("rr_gles3", "rr_gles3_setup", "rr_message", "rr_asset_codec.so", "session-io.so"):
            shutil.copyfile(build / "gles3" / name, app / "bin" / name)
            (app / "bin" / name).chmod(0o755)
        for name in (
            "portable.py",
            "first_launch.py",
            "calibration.py",
            "clocks.py",
            "watchdog.py",
            "telemetry.py",
            "transport.py",
            "device-profile.json",
            "controller-input.cfg",
            "roms.json",
        ):
            shutil.copyfile(platform / name, app / "launcher" / name)
        shutil.copytree(ROOT / "gles3/assets", app / "launcher/assets", ignore=shutil.ignore_patterns("__pycache__"))
        shutil.copytree(ROOT / "gles3/shaders", app / "launcher/shaders")
        shutil.copyfile(ROOT / "docs/INSTALL.md", app / "README.md")
        shutil.copyfile(ROOT.parent / "LICENSE", app / "LICENSE")
        files = {str(p.relative_to(app)): sha(p) for p in sorted(app.rglob("*")) if p.is_file()}
        (app / "package-hashes.json").write_text(json.dumps(files, indent=2) + "\n")
        result = output / f"rave-racer-knulli-h700-{version}.zip"
        with zipfile.ZipFile(result, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
            for path in sorted(tree.rglob("*")):
                name = str(path.relative_to(tree)) + ("/" if path.is_dir() else "")
                info = zipfile.ZipInfo(name, date_time=(2026, 1, 1, 0, 0, 0))
                info.external_attr = (
                    0o40755
                    if path.is_dir()
                    else 0o100755 if path.suffix in (".sh", ".so") or path.parent.name == "bin" else 0o100644
                ) << 16
                info.compress_type = zipfile.ZIP_DEFLATED
                archive.writestr(info, b"" if path.is_dir() else path.read_bytes())
    print(result)
    print("SHA256", sha(result))
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()
    if not args.version or any(
        c not in "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-" for c in args.version
    ):
        parser.error("version must contain only letters, digits, '.', '_' or '-'")
    build_release(args.build, args.output, args.version)
