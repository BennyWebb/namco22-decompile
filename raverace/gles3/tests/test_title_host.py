#!/usr/bin/env python3
"""Host-only title, raw controls, launcher return and personal-file isolation checks."""

import argparse
import hashlib
import importlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import time
import zlib
from unittest.mock import patch
from recordings import host_environment, generate_eeprom

HERE = Path(__file__).resolve().parent
PLATFORM = HERE.parents[1] / "platform/knulli-h700"


def png(path):
    raw = path.read_bytes()
    assert len(raw) == 640 * 480 * 4

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data))

    rows = b"".join(b"\0" + raw[y * 640 * 4 : (y + 1) * 640 * 4] for y in range(479, -1, -1))
    path.with_suffix(".png").write_bytes(
        b"\x89PNG\r\n\x1a\n"
        + chunk(b"IHDR", struct.pack(">IIBBBBB", 640, 480, 8, 6, 0, 0, 0))
        + chunk(b"IDAT", zlib.compress(rows))
        + chunk(b"IEND", b"")
    )


def run(build, roms, cache, out):
    out.mkdir(parents=True, exist_ok=False)
    env = host_environment(cache)
    seed = generate_eeprom(build / "gles3/rr_gles3_setup", roms, cache, out / "factory.eeprom", calibrated=True)
    assert (
        hashlib.sha256(seed.read_bytes()).hexdigest()
        == "81e19e9ce2fca04dbbd487b12e85cc473e30e61f2797c703743c9b86373721c7"
    )
    captures = out / "captures"
    captures.mkdir()
    with (out / "unit.log").open("w") as log:
        subprocess.run(
            [
                str(build / "gles3/test_title"),
                str(seed),
                str(PLATFORM / "controller-input.cfg"),
                str(roms),
                str(captures),
            ],
            env=env,
            check=True,
            stdout=log,
            stderr=log,
        )
    for image in captures.glob("*.rgba"):
        png(image)
    print("PASS title C unit tests and", len(list(captures.glob("*.png"))), "GLES3 PNG captures", flush=True)
    sys.path.insert(0, str(PLATFORM))
    portable = importlib.import_module("portable")
    app = out / "app"
    app.mkdir()
    for name in ["bin", "launcher", "saves", "logs", "roms"]:
        (app / name).mkdir()
    for name in ["watchdog.py", "clocks.py", "device-profile.json", "controller-input.cfg"]:
        shutil.copyfile(PLATFORM / name, app / "launcher" / name)
    for source in roms.iterdir():
        if source.is_file():
            (app / "roms" / source.name).symlink_to(source)
    (app / "bin/rr_gles3").symlink_to(build / "gles3/test_title_runtime")
    (app / "bin/rr_gles3_setup").symlink_to(build / "gles3/rr_gles3_setup")
    helper = app / "bin/rr_message"
    helper.write_text("#!/usr/bin/python3\nimport time\ntime.sleep(120)\n")
    helper.chmod(0o755)
    calls = []
    outcomes = []
    spawn = []
    native = []
    real_monitor = portable.monitor
    real_popen = subprocess.Popen
    real_calibrate = portable.calibration.generate

    def calibrate(binary, roms, cache, output, environment, log):
        assert not any(
            key in environment
            for key in (
                "ENG_GLES3_SESSION",
                "ENG_GLES3_SAVE_PATH",
                "ENG_GLES3_CONTROLLER_PATH",
                "ENG_GLES3_SAVE_ACK_FD",
            )
        )
        calls.append(("calibrate",))
        return real_calibrate(binary, roms, cache, output, environment, log)

    def monitor(child, profile, collector, *, ack=None):
        line = collector.line

        def observed(kind, text):
            if kind == "stderr" and text.startswith(("[TITLE", "[RETURN-TITLE]", "[SAVE-FLUSH]")):
                native.append(text.strip())
            line(kind, text)

        collector.line = observed
        result = real_monitor(child, profile, collector, ack=ack)
        outcomes.append(dict(code=result, saves=collector.save_writes))
        calls.append(("child_exit", result, collector.save_writes))
        return result

    def process(command, **kwargs):
        if str(command[0]) == str(app / "bin/rr_gles3"):
            assert (app / "logs/active").is_dir()
            journal = json.loads((app / "logs/clock-journal.json").read_text())
            spawn.append(dict(owner=journal["owner"], owner_start=journal["owner_start"], restored=journal["restored"]))
            calls.append(("spawn", len(spawn)))
        return real_popen(command, **kwargs)

    host_env = dict(
        env, TITLE_TEST_COUNT=str(out / "child-count.txt"), TITLE_TEST_RECORDING=str(HERE / "recordings/full-race.rec")
    )

    def environment():
        return dict(host_env)

    with patch.object(portable, "environment", environment), patch.object(
        portable, "verify_package", lambda app: None
    ), patch.object(portable, "ensure_cache", lambda app, log, poll: (cache, False, 0)), patch.object(
        portable.clocks, "snapshot", lambda: {}
    ), patch.object(
        portable.clocks, "apply", lambda: calls.append(("apply",))
    ), patch.object(
        portable.clocks, "restore", lambda original: calls.append(("restore",)) or []
    ), patch.object(
        portable.clocks, "low_priority", lambda: None
    ), patch.object(
        portable, "monitor", monitor
    ), patch.object(
        portable.subprocess, "Popen", process
    ), patch.object(
        portable.calibration, "generate", calibrate
    ):
        started = time.monotonic()
        code = portable.run(app)
    assert code == 0 and outcomes[0]["code"] == 42 and outcomes[0]["saves"] >= 1 and outcomes[1]["code"] == 0, outcomes
    assert len(spawn) == 2 and spawn[0] == spawn[1] and not spawn[0]["restored"]
    assert calls.count(("apply",)) == 1 and calls.count(("restore",)) == 1
    assert calls.count(("calibrate",)) == 1
    assert json.loads((app / "logs/launch-summary.json").read_text())["returns_to_title"] == 1
    assert not (app / "logs/active").exists()
    assert (out / "child-count.txt").read_text().strip() == "2"
    assert (app / "saves/rr_eeprom.nv").read_bytes() == seed.read_bytes()
    assert sum("opened simulation_ticks=0" in line for line in native) == 2, native
    assert any("hold X in race" in line for line in native), native
    assert "[TITLE-TEST] handoff inputs neutral; simulation_ticks=0" in native, native
    assert any("save acknowledged before worker shutdown" in line for line in native), native
    assert native.index("[SAVE-FLUSH] return-title") < native.index(
        "[RETURN-TITLE] save acknowledged before worker shutdown"
    )
    (out / "runtime-result.json").write_text(
        json.dumps(
            dict(
                exact=True,
                outcomes=outcomes,
                spawns=spawn,
                calls=calls,
                native=native,
                elapsed=time.monotonic() - started,
            ),
            indent=2,
        )
        + "\n"
    )
    print(
        "PASS native X-hold in race, EEPROM persisted/acknowledged before code42, launcher restarts title under same lock",
        flush=True,
    )
    strict = out / "invalid-controls"
    strict.mkdir()
    packaged = (PLATFORM / "controller-input.cfg").read_text()
    invalid = {
        "malformed": ("malformed", "MALFORMED"),
        "guid": (
            packaged.replace("19000000010000000100000000010000", "00000000010000000100000000010000"),
            "DEVICE_IDENTITY",
        ),
        "name": (packaged.replace("Anbernic RG40XX-H Controller", "Unexpected controller"), "DEVICE_IDENTITY"),
        "dangling": (None, "IO_OPEN"),
    }
    for name, (text, error) in invalid.items():
        config = strict / (name + ".cfg")
        if text is None:
            config.symlink_to(strict / "absent.cfg")
        else:
            config.write_text(text)
        readfd, writefd = os.pipe()
        try:
            isolated_env = dict(
                host_env,
                TITLE_TEST_COUNT=str(strict / (name + "-count")),
                ENG_GLES3_SESSION="1",
                ENG_GLES3_SAVE_PATH=str(app / "saves/rr_eeprom.nv"),
                ENG_GLES3_INITIAL_EEPROM=str(app / "saves/rr_eeprom.nv"),
                ENG_GLES3_CONTROLLER_PATH=str(config),
                ENG_GLES3_DEFAULT_INPUT=str(PLATFORM / "controller-input.cfg"),
                ENG_GLES3_SAVE_ACK_FD=str(readfd),
            )
            result = subprocess.run(
                [str(build / "gles3/test_title_runtime"), str(roms), "--interactive"],
                env=isolated_env,
                pass_fds=(readfd,),
                capture_output=True,
                text=True,
                timeout=10,
            )
        finally:
            os.close(readfd)
            os.close(writefd)
        (strict / (name + ".log")).write_text(result.stderr)
        assert result.returncode == 2 and "[TITLE-ERROR] " + error in result.stderr, (
            name,
            result.returncode,
            result.stderr,
        )
        assert "[TITLE] handoff" not in result.stderr, name
        assert (app / "saves/rr_eeprom.nv").read_bytes() == seed.read_bytes()
    print("PASS 4 invalid saved profiles: named error, no packaged default, no game handoff", flush=True)
    poison = out / "personal"
    poison.mkdir()
    (poison / "rr_eeprom.nv").write_bytes(b"poison")
    (poison / "controller-input.cfg").write_text("malformed")
    isolated = []
    for target in ["rr_gles3", "rr_gles3_validation", "rr_gles3_setup", "rr_gles3_sound_oracle"]:
        for options in [
            [],
            ["--interactive"],
            ["--record-input", str(out / (target + ".rec"))],
            ["--replay-input", str(HERE / "recordings/full-race.rec")],
            ["--interactive", "--record-input", str(out / (target + "-interactive.rec"))],
            ["--interactive", "--replay-input", str(HERE / "recordings/full-race.rec")],
        ]:
            poisoned = dict(
                env,
                ENG_GLES3_SESSION="poison",
                ENG_GLES3_SAVE_PATH=str(poison / "rr_eeprom.nv"),
                ENG_GLES3_INITIAL_EEPROM=str(poison / "rr_eeprom.nv"),
                ENG_GLES3_CONTROLLER_PATH=str(poison / "controller-input.cfg"),
                TITLE_POISON_DIR=str(poison),
                LD_PRELOAD=str(build / "gles3/title_open_audit.so"),
            )
            r = subprocess.run(
                [str(build / "gles3" / target), str(roms), "--frames", "1", *options],
                env=poisoned,
                capture_output=True,
                text=True,
                timeout=15,
            )
            assert r.returncode == 2 and "PERSONAL_FILE_OPENED" not in r.stderr, (
                target,
                options,
                r.returncode,
                r.stderr,
            )
            isolated.append(dict(target=target, options=options, exit=r.returncode))
    (out / "isolation-result.json").write_text(json.dumps(isolated, indent=2) + "\n")
    print("PASS", len(isolated), "poisoned-mode gates rejected before any personal file opened", flush=True)
    (out / "result.json").write_text(
        json.dumps(
            dict(
                exact=True,
                captures=[str(p.relative_to(out)) for p in sorted(captures.glob("*.png"))],
                isolation_cases=len(isolated),
                invalid_profiles=len(invalid),
                return_to_title=True,
            ),
            indent=2,
        )
        + "\n"
    )


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    for name in ["build", "roms", "cache", "output"]:
        p.add_argument("--" + name, type=Path, required=True)
    a = p.parse_args()
    run(*(getattr(a, n).resolve() for n in ["build", "roms", "cache", "output"]))
