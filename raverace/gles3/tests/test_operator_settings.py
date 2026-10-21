#!/usr/bin/env python3
"""Byte-exact lifted TEST/SERVICE oracle; generated EEPROMs never leave --output."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess

from recordings import generate_eeprom, host_environment

HERE = Path(__file__).resolve().parent
INITIAL_SHA256 = "81e19e9ce2fca04dbbd487b12e85cc473e30e61f2797c703743c9b86373721c7"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def metadata(seed):
    fnv = 14695981039346656037
    for byte in seed:
        fnv = ((fnv ^ byte) * 1099511628211) & ((1 << 64) - 1)
    lines = (HERE / "recordings/full-race.rec.meta").read_text().splitlines()
    for i, line in enumerate(lines):
        if line.startswith("EEPROM "):
            lines[i] = f"EEPROM {fnv:016x}"
        elif line.startswith("ROM rv1eeprm.9e "):
            lines[i] = f"ROM rv1eeprm.9e 8192 {fnv:016x}"
    return "\n".join(lines) + "\n"


def seal(data, offset, size=32):
    """Independent Python checksum, also tested against the ROM's actual writer."""
    value = (~sum(data[offset : offset + size - 2])) & 0xFFFF
    data[offset + size - 2 : offset + size] = value.to_bytes(2, "big")


def module_driver(driver):
    def module(path, command="check", field=None, value=None, *, expected="OK", env=None):
        cmd = [str(driver), command, str(path)]
        if field is not None:
            cmd += [str(field), str(value)]
        clean = {k: v for k, v in os.environ.items() if not k.startswith("OPERATOR_TEST_")}
        r = subprocess.run(cmd, env=dict(clean, **(env or {})), text=True, capture_output=True)
        assert r.returncode == (0 if expected == "OK" else 1), (cmd, r.stdout, r.stderr)
        assert r.stdout.split()[0] == expected, (cmd, expected, r.stdout, r.stderr)
        return r.stdout.strip()

    return module


def run_tests(build, roms, cache, output):
    output.mkdir(parents=True, exist_ok=False)
    seed_path = generate_eeprom(build / "gles3/rr_gles3_setup", roms, cache, output / "factory.eeprom", calibrated=True)
    seed = seed_path.read_bytes()
    assert sha(seed) == INITIAL_SHA256, "initial image before ANY edit"
    driver = build / "gles3/test_operator_settings"
    oracle = build / "gles3/rr_operator_oracle"
    results = dict(initial_sha256=sha(seed), oracle=[], negative=[], positive=[])

    module = module_driver(driver)

    fields = [(3, 1, 1), (3, 3, 3), (3, 4, 4), (2, 11, 13), (2, 12, 14), (3, 13, 13), (3, 14, 14), (2, 3, 3)]
    manifest = json.loads((HERE / "operator_recordings/cases.json").read_text())
    for case in manifest:
        d = output / case["name"]
        d.mkdir()
        source = HERE / "operator_recordings" / (case["name"] + ".rec")
        assert sha(source.read_bytes()) == case["input_sha256"]
        rec = d / source.name
        shutil.copyfile(source, rec)
        initial_path = output / case["initial_case"] / "game.eeprom" if "initial_case" in case else seed_path
        initial = initial_path.read_bytes()
        Path(str(rec) + ".eeprom").write_bytes(initial)
        Path(str(rec) + ".meta").write_text(metadata(initial))
        Path(str(rec) + ".end").write_text(str(case["ticks"]) + "\n")
        captured = d / "game.eeprom"
        env = dict(host_environment(cache), ENG_GLES3_INITIAL_EEPROM=str(initial_path),
                   OPERATOR_ORACLE_OUTPUT=str(captured))
        with (d / "game.log").open("w") as log:
            subprocess.run([str(oracle), str(roms), "--replay-input", str(rec)], env=env,
                           stdout=log, stderr=log, check=True)
        log = (d / "game.log").read_text()
        assert f"completed_frames={case['ticks']} status=ok" in log
        assert "0 traps" in log and "GLES3 UNSUPPORTED" not in log and "[SND] TRANSLATED DRIVER" not in log
        path = d / "module.eeprom"
        path.write_bytes(initial)
        module(path, "edit", case["field"], case["value"])
        actual, game = path.read_bytes(), captured.read_bytes()
        group, byte, duplicate = fields[case["field"]]
        record = 512 + 64 * group  # Generated seed selects bank 0 for every group.
        allowed = {record + byte, record + duplicate, record + 30, record + 31}
        changes = [i for i in range(len(seed)) if actual[i] != initial[i]]
        assert changes and set(changes) <= allowed, (case, changes)
        assert {i for i in range(len(seed)) if actual[i] != seed[i]} <= allowed
        differences = [i for i in range(len(seed)) if actual[i] != game[i]]
        assert actual == game, (case, "game/module mismatch offsets", differences)
        results["oracle"].append(dict(field=case["name"], change=case["change"], byte_identical=True,
                                      initial_sha256=sha(initial), changed_offsets=changes, sha256=sha(actual)))
        print("EXACT OPERATOR", case["name"], case["change"], flush=True)

    run_module_tests(seed_path, output, module, results)


def run_module_tests(seed_path, output, module, results):
    seed = seed_path.read_bytes()

    def negative(name, data, code, field=0, value=7):
        path = output / (name + ".eeprom")
        path.write_bytes(data)
        before = path.read_bytes()
        module(path, "edit", field, value, expected=code)
        assert path.read_bytes() == before, name
        assert not Path(str(path) + ".operator-new").exists(), name
        results["negative"].append(dict(case=name, error=code, unchanged=True))

    bad = bytearray(seed)
    bad[62] ^= 1
    negative("header-checksum", bad, "NO_HEADER")
    bad = bytearray(seed)
    bad[0x2DF] ^= 1
    negative("selected-checksum", bad, "RECORD_CHECKSUM")
    bad = bytearray(seed)
    bad[0x2DA] ^= 1
    seal(bad, 0x2C0)
    negative("selected-marker", bad, "RECORD_MARKER")
    bad = bytearray(seed)
    bad[58] ^= 1
    seal(bad, 0, 64)
    negative("header-marker", bad, "NO_HEADER")
    negative("truncated", seed[:-1], "SIZE")
    negative("oversized", seed + b"\0", "SIZE")
    for field, low, high in [(0, 0, 7), (1, 1, 8), (2, 1, 8), (3, 0, 63), (4, 0, 63), (5, 0, 1), (6, 0, 1), (7, 0, 1)]:
        for value in [low - 1, high + 1]:
            negative(f"range-{field}-{value}", seed, "RANGE", field, value)
    negative("unknown-field", seed, "READ_ONLY", 8, 0)
    bad = bytearray(seed)
    bad[0x2D2] = 0
    bad[0x2C2 : 0x2C5] = bytes([9]) * 3
    bad[0x2C5 : 0x2CD] = bytes([4]) * 8
    seal(bad, 0x2C0)
    for field in range(8):
        negative(f"extra-mode-{field}", bad, "EXTRA_MODE", field, 1)
    for offset, value in [(0x200, 0xFF), (0x280, 0), (0x281, 10), (0x282, 0), (0x28D, 31),
                          (0x2C0, 8), (0x2C2, 0), (0x2C5, 4), (0x2CF, 2), (0x2D0, 2),
                          (0x2D1, 2), (0x2D2, 2), (0x300, 2)]:
        bad = bytearray(seed)
        bad[offset] = value
        seal(bad, 512 + ((offset - 512) // 64) * 64)
        negative(f"read-only-range-{offset:x}", bad, "RANGE")

    # No-op: check contents and inode, proving there was no needless rename.
    path = output / "no-op.eeprom"
    path.write_bytes(seed)
    before = path.stat()
    module(path, "edit", 0, 0)
    assert path.read_bytes() == seed and path.stat().st_ino == before.st_ino
    results["positive"].append("no-op byte/inode identity")
    module(path, "api")
    assert path.read_bytes() == seed
    results["positive"].append("failed APIs leave output unchanged; aliased edit supported")
    for offset, value in [(0x201, seed[0x201] ^ 1), (0x280, 3), (0x281, 2), (0x282, 2),
                          (0x2C0, 1), (0x2C2, 5), (0x2C5, 1), (0x2CF, 0), (0x2D0, 0),
                          (0x2D1, 0), (0x2D3, seed[0x2D3] ^ 1), (0x300, seed[0x300] ^ 1),
                          (0x240, seed[0x240] ^ 1)]:
        module(path, "readonly", offset, value, expected="READ_ONLY")
        assert path.read_bytes() == seed
        results["negative"].append(dict(case=f"read-only-write-{offset:x}", error="READ_ONLY", unchanged=True))

    # Header scan skips a corrupt header, but must NOT rescue a corrupt selected
    # record by choosing a later header. Bank 1 is tested without a valid bank 0.
    banked = bytearray(seed)
    banked[64:128] = banked[0:64]
    banked[62] ^= 1
    for g in [2, 3]:
        a = 512 + 64 * g
        banked[a + 32 : a + 64] = banked[a : a + 32]
        banked[a + 26] = 0  # Inactive is deliberately invalid.
        banked[64 + g // 2] |= 1 << (0 if g % 2 else 4)
    seal(banked, 64, 64)
    path = output / "bank-1.eeprom"
    path.write_bytes(banked)
    module(path, "edit", 1, 8)
    actual = path.read_bytes()
    assert {i for i in range(8192) if actual[i] != banked[i]} <= {0x2E3, 0x2FE, 0x2FF}
    assert actual[0x2E3] == 8
    results["positive"].append("ordered header scan, bank 1, unused invalid bank preserved")
    bad = bytearray(seed)
    bad[64:128] = bad[0:64]
    bad[0x2E0:0x300] = bad[0x2C0:0x2E0]
    bad[65] |= 1  # Later header selects a valid bank 1; first header still wins.
    seal(bad, 64, 64)
    bad[0x2DF] ^= 1
    negative("no-header-rescue", bad, "RECORD_CHECKSUM")
    # Every selected record is checked, including unrelated score/stat records.
    bad = bytearray(seed)
    bad[512 + 115 * 64 + 30] ^= 1
    negative("unrelated-selected-record", bad, "RECORD_CHECKSUM")

    path = output / "stale.eeprom"
    path.write_bytes(seed)
    module(path, "stale-token", 1, 5, expected="STALE_SOURCE")
    assert path.read_bytes() == seed
    results["negative"].append(dict(case="stale-token", error="STALE_SOURCE", unchanged=True))
    module(path, "stale-token", 0, 0, expected="STALE_SOURCE")
    assert path.read_bytes() == seed
    results["negative"].append(dict(case="stale-no-op", error="STALE_SOURCE", unchanged=True))
    module(path, "stale", 1, 5, expected="STALE_SOURCE")
    # Driver simulates a different writer; stale commit must retain that writer's image.
    other = bytearray(seed)
    other[0x2C1] = 1
    seal(other, 0x2C0)
    assert path.read_bytes() == other
    results["negative"].append(dict(case="stale-source", error="STALE_SOURCE", concurrent_image_unchanged=True))
    path = output / "concurrent-read.eeprom"
    path.write_bytes(seed)
    module(path, expected="STALE_SOURCE", env={"OPERATOR_TEST_TRUNCATE_ON_READ": str(path)})
    assert path.read_bytes() == seed[:-1]
    results["negative"].append(dict(case="concurrent-read", error="STALE_SOURCE", concurrent_image_unchanged=True))

    for operation in ["write:1", "fsync:1", "rename:1"]:
        path = output / ("io-" + operation.replace(":", "-") + ".eeprom")
        path.write_bytes(seed)
        module(path, "edit", 0, 7, expected="IO", env={"OPERATOR_TEST_FAIL": operation})
        assert path.read_bytes() == seed
        results["negative"].append(dict(case=operation, error="IO", unchanged=True))
    path = output / "stale-transaction.eeprom"
    path.write_bytes(seed)
    Path(str(path) + ".operator-new").write_text("existing transaction")
    module(path, "edit", 0, 7, expected="IO")
    assert path.read_bytes() == seed and Path(str(path) + ".operator-new").read_text() == "existing transaction"
    results["negative"].append(dict(case="existing-transaction", error="IO", unchanged=True))
    # A post-rename durability/readback error cannot promise rollback. It MUST
    # report failure; the installed image must still be exactly the intended edit.
    for operation in ["fsync:2", "read:4"]:
        path = output / ("post-rename-" + operation.replace(":", "-") + ".eeprom")
        path.write_bytes(seed)
        module(path, "edit", 0, 7, expected="IO", env={"OPERATOR_TEST_FAIL": operation})
        changed = bytearray(seed)
        changed[0x2C1] = 7
        seal(changed, 0x2C0)
        assert path.read_bytes() == changed
        results["negative"].append(dict(case=operation, error="IO", installed_after_rename=True))
    for mode in ["OPERATOR_TEST_EINTR", "OPERATOR_TEST_SHORT_WRITE"]:
        path = output / (mode.lower() + ".eeprom")
        path.write_bytes(seed)
        module(path, "edit", 0, 7, env={mode: "1"})
        assert path.read_bytes() == changed
        results["positive"].append(mode)
    path = output / "transaction-order.eeprom"
    path.write_bytes(seed)
    trace = output / "transaction-order.txt"
    module(path, "edit", 0, 7, env={"OPERATOR_TEST_TRACE": str(trace)})
    assert trace.read_text().splitlines() == [
        "read:1", "read:2", "write:1", "fsync:1", "read:3", "rename:1", "fsync:2", "read:4"
    ]
    results["positive"].append("write/fsync/recheck/rename/directory-fsync/readback order")
    assert seed_path.read_bytes() == seed
    (output / "result.json").write_text(json.dumps(results, indent=2) + "\n")
    print("PASS OPERATOR", len(results["oracle"]), "exact oracles;", len(results["negative"]),
          "negative cases", flush=True)


if __name__ == "__main__":
    p = argparse.ArgumentParser(description=__doc__)
    for name in ["build", "roms", "cache", "output"]:
        p.add_argument("--" + name, type=Path, required=True)
    a = p.parse_args()
    run_tests(*(getattr(a, name).resolve() for name in ["build", "roms", "cache", "output"]))
