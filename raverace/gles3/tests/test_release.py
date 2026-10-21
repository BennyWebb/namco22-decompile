#!/usr/bin/env python3
"""Host package-layout and strict ROM/error tests (no device required)."""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "platform/knulli-h700"))
from first_launch import SetupError, check_roms


def checks(package):
    with tempfile.TemporaryDirectory() as directory:
        roms = Path(directory)
        payload = b"controller-only synthetic test fixture"
        identity = {
            "sample.rom": {"bytes": len(payload), "sha256": hashlib.sha256(payload).hexdigest(), "crc32": "8ce42d5d"}
        }
        import zlib

        identity["sample.rom"]["crc32"] = f"{zlib.crc32(payload):08x}"
        for expected, data in [("Missing: sample.rom", None), ("Wrong: sample.rom", b"wrong")]:
            if data:
                (roms / "sample.rom").write_bytes(data)
            try:
                check_roms(roms, identity)
            except SetupError as error:
                assert expected in str(error), error
            else:
                raise AssertionError("bad ROM accepted")
        (roms / "sample.rom").write_bytes(payload)
        check_roms(roms, identity)
    with zipfile.ZipFile(package) as archive:
        names = archive.namelist()
        assert {n.split("/")[0] for n in names} == {"Rave Racer.sh", "rave-racer"}
        for directory in ("roms", "cache", "saves", "logs"):
            prefix = f"rave-racer/{directory}/"
            assert prefix in names
            assert not any(n.startswith(prefix) and n != prefix for n in names)
        assert not any(
            "asset-kit/" in n or n.endswith((".nv", ".eeprom", ".rec", ".rgba", ".f32", ".r8ui")) for n in names
        )
        manifest = json.loads(archive.read("rave-racer/package-hashes.json"))
        for name, digest in manifest.items():
            assert hashlib.sha256(archive.read("rave-racer/" + name)).hexdigest() == digest, name
        launcher = archive.read("rave-racer/launcher/portable.py").decode()
        assert "/userdata/system" not in launcher and "/userdata/saves" not in launcher
        assert 'default="summary"' in launcher
        assert "rave-gles2" not in launcher
        assert archive.read("Rave Racer.sh").startswith(b"#!/bin/sh")
        assert "rave-racer/bin/rr_gles3_setup" in names
    print("PASS package layout, empty user-data folders, hash manifest, missing/wrong/correct ROM checks")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("package", type=Path)
    checks(parser.parse_args().package)
