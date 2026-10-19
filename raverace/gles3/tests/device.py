#!/usr/bin/env python3
"""Isolated device validation only: never installs an app or changes Ports entries."""

from pathlib import Path
import argparse, hashlib, json, os, shutil, subprocess, sys, time

p = argparse.ArgumentParser()
p.add_argument("case", choices=["reference", "cone", "smoke"])
p.add_argument("--app", type=Path, required=True)
p.add_argument("--output", type=Path, required=True)
p.add_argument("--recordings", type=Path, required=True)
p.add_argument("--fixtures", type=Path, required=True)
p.add_argument("--protected", type=Path, required=True)
p.add_argument("--roms", type=Path, required=True)
p.add_argument("--installed-manifest", type=Path, required=True)
a = p.parse_args()
sys.path.insert(0, str(a.app.resolve()))
import clocks, session, runtime_assets
from telemetry import Collector

controller = os.environ.get("GLES3_TEST_CONTROLLER_CONFIG")
if a.case == "smoke":
    assert controller and Path(controller).is_file(), "set GLES3_TEST_CONTROLLER_CONFIG to the saved controller mapping"
out = a.output.resolve()
out.mkdir()
nv = out / "eeprom.nv"
identity = json.loads((a.fixtures / "recordings.json").read_text())
name = "cone" if a.case == "cone" else "reference"
rec = a.recordings / (name + ".rec")
seed = Path(str(rec) + ".eeprom")
assert hashlib.sha256(rec.read_bytes()).hexdigest() == identity[name]["input_sha256"]
assert hashlib.sha256(seed.read_bytes()).hexdigest() == identity[name]["eeprom_sha256"]
shutil.copyfile(seed, nv)
assert subprocess.run(["pidof", "rr_gles3", "rr_gles2"], stdout=subprocess.DEVNULL).returncode != 0, "game active"


def audit(suffix):
    with (out / ("protected." + suffix)).open("w") as f:
        subprocess.run(["sha256sum", "-c", str(a.protected)], check=True, stdout=f)
    for name, h in json.loads(a.installed_manifest.read_text()).items():
        assert hashlib.sha256(Path(name).read_bytes()).hexdigest() == h, name


audit("before")
original = clocks.snapshot()
journal = out / "clock-journal.json"
j = dict(owner=os.getpid(), owner_start=clocks.identity(os.getpid()), original=original, restored=False)
clocks.atomic(journal, j)
watch = subprocess.Popen([sys.executable, str(a.app / "watchdog.py"), str(journal)])
collector = Collector(out, nv, out / "telemetry")
stopped = False
child = None
readfd = None
code = 2
errors = []
try:
    deadline = time.monotonic() + 5
    while not (out / "watchdog.ready").exists() and time.monotonic() < deadline:
        time.sleep(0.05)
    assert (out / "watchdog.ready").exists()
    if subprocess.run(["pidof", "emulationstation"], stdout=subprocess.DEVNULL).returncode == 0:
        subprocess.run(
            ["/etc/init.d/S31emulationstation", "stop"],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        stopped = True
    clocks.apply()
    with (out / "assets.log").open("w") as log:
        cache, built = runtime_assets.ensure(a.app, a.roms, log)
    env = {k: v for k, v in os.environ.items() if not k.startswith(("ENG_", "RR_", "SND_"))}
    env.pop("LD_PRELOAD", None)
    env.update(
        SDL_VIDEODRIVER="mali",
        XDG_RUNTIME_DIR="/var/run",
        ENG_GLES3_ASSETS=str(cache),
        ENG_GLES3_SESSION="1",
        ENG_GLES3_SAVE_PATH=str(nv),
        ENG_GLES3_SUMMARY_PATH=str(out / "latest-summary.txt"),
        LD_PRELOAD=str(a.app / "session-io.so"),
    )
    cmd = [
        str(a.app / "rr_gles3"),
        str(a.roms),
        "--timing",
        "independent",
        "--presentation-thread",
        "on",
        "--audio",
        "on",
        "--overlay",
        "off",
        "--profile-simulation",
        "--simulation-profile-from",
        "1",
        "--input-checkpoints",
        str(out / "states.jsonl"),
    ]
    if a.case == "smoke":
        cmd += ["--interactive", "--es-input", controller]
        env.update(
            LD_PRELOAD=env["LD_PRELOAD"] + ":" + str(a.app / "smoke-chord.so"), ENG_GLES3_SMOKE_CHORD_SECONDS="125"
        )
    else:
        cmd += ["--replay-input", str(rec)]
        env["ENG_GLES3_INITIAL_EEPROM"] = str(seed)
    readfd, writefd = os.pipe()
    cmd += ["--thread-profile", f"/proc/self/fd/{writefd}"]
    (out / "method.json").write_text(
        json.dumps(
            dict(
                cmd=cmd,
                env=env,
                cache_built=built,
                binary_sha256=hashlib.sha256((a.app / "rr_gles3").read_bytes()).hexdigest(),
            ),
            indent=2,
        )
    )
    child = subprocess.Popen(
        cmd,
        cwd=out,
        env=env,
        preexec_fn=session.child_guard,
        pass_fds=(writefd,),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    os.close(writefd)
    j.update(game=child.pid, game_start=clocks.identity(child.pid))
    clocks.atomic(journal, j)
    clocks.low_priority()
    code = session.monitor(child, readfd, collector)
finally:
    if readfd is not None:
        os.close(readfd)
    if child and child.poll() is None:
        child.terminate()
        child.wait(timeout=10)
    errors = clocks.restore(original)
    j.update(restored=not errors, restore_errors=errors)
    clocks.atomic(journal, j)
    watch.wait(timeout=5)
    result = collector.finish(code, errors, dict(cache_built=locals().get("built")))
    audit("after")
    if stopped:
        subprocess.run(
            ["/etc/init.d/S31emulationstation", "start"],
            check=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
assert code == 0 and not errors and result["unmatched_records"] == 0, (code, errors, result["unmatched_records"])
if a.case == "smoke":
    assert result["smoke_start_select_seen"] and result["processed_pictures"] >= 7160
else:
    expected = json.loads((a.fixtures / (name + "-checkpoints.json")).read_text())
    actual = [hashlib.sha256(s.encode() + b"\n").hexdigest() for s in (out / "states.jsonl").read_text().splitlines()]
    assert actual == [x["sha256"] for x in expected]
print("PASS DEVICE", a.case, result["processed_pictures"], flush=True)
