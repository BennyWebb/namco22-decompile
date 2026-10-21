#!/usr/bin/env python3
"""Self-contained Knulli launcher: strict setup, summary-only normal play."""

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import signal
import subprocess
import sys
import time

import calibration
import clocks
from first_launch import SetupError, digest, ensure_cache, stage
from telemetry import Collector, Rolling
from transport import child_guard, monitor, persist_eeprom

RETURN_TO_TITLE = 42


def game_guard():
    child_guard()
    os.sched_setscheduler(0, os.SCHED_OTHER, os.sched_param(0))
    os.setpriority(os.PRIO_PROCESS, 0, 0)


APP = Path(__file__).resolve().parent.parent


def environment():
    env = {k: v for k, v in os.environ.items() if not k.startswith(("ENG_", "RR_", "SND_"))}
    env.pop("LD_PRELOAD", None)
    env.update(SDL_VIDEODRIVER="mali", XDG_RUNTIME_DIR="/var/run", PYTHONDONTWRITEBYTECODE="1")
    return env


def screen(app, kind, message):
    # The message helper owns the display only during setup/error reporting.
    return subprocess.Popen([str(app / "bin/rr_message"), kind, message], env=environment(), preexec_fn=child_guard)


def close_screen(child):
    if child.poll() is None:
        child.terminate()
    child.wait(timeout=5)


def verify_package(app):
    try:
        expected = json.loads((app / "package-hashes.json").read_text())
        for name, value in expected.items():
            if digest(app / name) != value:
                raise ValueError(name)
    except (OSError, ValueError) as error:
        raise SetupError(
            f"Package file damaged: {error}\nExtract a fresh copy of the same package; keep saves and ROMs."
        ) from error


class Summary:
    """No profile parsing, resource sampling or per-picture storage in normal play."""

    def __init__(self, logs, nv):
        self.nv = nv
        self.stream = Rolling(logs / "play.log", limit=65536)
        self.unmatched = 0
        self.save_writes = 0
        self.title_error = None

    def line(self, kind, line):
        if kind == "stderr" and line.startswith("[TITLE-ERROR] "):
            self.title_error = line.removeprefix("[TITLE-ERROR] ").strip()
        if kind == "stderr" and line.startswith("[SAVE-EEPROM] "):
            data = bytes.fromhex(line.split()[1])
            if len(data) != 8192:
                raise SetupError("Save write failed: invalid EEPROM size.")
            persist_eeprom(self.nv, data)
            self.save_writes += 1
            line = "[SAVE-EEPROM] saved sha256=" + hashlib.sha256(data).hexdigest() + "\n"
        self.stream.write(line.encode())

    def sample(self, pid):
        pass


def run(app=APP, *, full=False, frames=0):
    for name in ("cache", "saves", "logs"):
        (app / name).mkdir(exist_ok=True)
    logs, nv = app / "logs", app / "saves/rr_eeprom.nv"
    lock, journal = logs / "active", logs / "clock-journal.json"
    if lock.exists():
        previous = json.loads(journal.read_text())
        if clocks.identity(previous["owner"]) == previous["owner_start"]:
            raise SetupError("Rave Racer is already running. Exit the other session first.")
        if not previous.get("restored") and clocks.restore(previous["original"]):
            raise SetupError("Clock restoration failed. Restart the handheld.")
        shutil.rmtree(lock)
    lock.mkdir()
    original = None
    watch = child = collector = None
    result = 2
    started = time.monotonic()
    timings = {}
    try:
        verify_package(app)
        original = clocks.snapshot()
        state = dict(owner=os.getpid(), owner_start=clocks.identity(os.getpid()), original=original, restored=False)
        clocks.atomic(journal, state)
        ready = logs / "watchdog.ready"
        ready.unlink(missing_ok=True)
        watch = subprocess.Popen(
            [sys.executable, str(app / "launcher/watchdog.py"), str(journal)],
            start_new_session=True,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        deadline = time.monotonic() + 5
        while not ready.exists() and time.monotonic() < deadline:
            time.sleep(0.05)
        if not ready.exists():
            raise SetupError("Clock watchdog unavailable. Restart the handheld.")
        clocks.apply()
        stage(app, "Checking ROMs: 0%")
        progress = screen(app, "progress", str(logs / "setup-stage.txt"))

        def setup_poll():
            if progress.poll() is not None:
                raise SetupError("Setup cancelled or display unavailable. Launch again to restart setup.")

        try:
            with (logs / "setup.log").open("w") as log:
                cache, built, seconds = ensure_cache(app, log, setup_poll)
            timings.update(asset_cache_built=built, asset_compile_seconds=seconds)
        finally:
            close_screen(progress)
        env = environment()
        env.update(
            ENG_GLES3_ASSETS=str(cache),
            ENG_GLES3_SUMMARY_PATH=str(logs / "latest-summary.txt"),
        )
        if not nv.exists():
            start = time.monotonic()
            with (logs / "calibration.log").open("w") as log:
                try:
                    calibration.generate(app / "bin/rr_gles3_setup", app / "roms", cache, nv, env, log)
                except (RuntimeError, subprocess.CalledProcessError) as error:
                    raise SetupError(
                        "EEPROM calibration failed. See logs/calibration.log.\nNo uncalibrated game was started."
                    ) from error
            timings["calibration_seconds"] = time.monotonic() - start
        if nv.stat().st_size != 8192:
            raise SetupError(
                "EEPROM damaged. Restore your personal save backup.\nTo reset, move saves/rr_eeprom.nv aside and relaunch."
            )
        env.update(
            ENG_GLES3_SESSION="1",
            ENG_GLES3_SAVE_PATH=str(nv),
            ENG_GLES3_INITIAL_EEPROM=str(nv),
            ENG_GLES3_DEFAULT_INPUT=str(app / "launcher/controller-input.cfg"),
            ENG_GLES3_CONTROLLER_PATH=str(app / "saves/controller-input.cfg"),
        )
        cmd = [
            str(app / "bin/rr_gles3"),
            str(app / "roms"),
            "--interactive",
            "--timing",
            "independent",
            "--presentation-thread",
            "on",
            "--audio",
            "on",
            "--overlay",
            "off",
        ]
        if frames:
            cmd += ["--frames", str(frames)]
        returns = 0
        while True:
            child_cmd = list(cmd)
            readfd, writefd = os.pipe()
            ackread, ackwrite = os.pipe()
            env["ENG_GLES3_SAVE_ACK_FD"] = str(ackread)
            if full:
                sessions = logs / "sessions"
                sessions.mkdir(exist_ok=True)
                run_dir = sessions / (time.strftime("%Y%m%d-%H%M%S") + "-" + str(os.getpid()) + "-" + str(returns))
                run_dir.mkdir()
                collector = Collector(run_dir, nv)
                env["LD_PRELOAD"] = str(app / "bin/session-io.so")
                child_cmd += [
                    "--thread-profile",
                    f"/proc/self/fd/{writefd}",
                    "--profile-simulation",
                    "--simulation-profile-from",
                    "1",
                ]
            else:
                collector = Summary(logs, nv)
            child = subprocess.Popen(
                child_cmd,
                cwd=app / "saves",
                preexec_fn=game_guard,
                env=env,
                pass_fds=(ackread, writefd) if full else (ackread,),
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            os.close(writefd)
            os.close(ackread)
            state.update(game=child.pid, game_start=clocks.identity(child.pid))
            clocks.atomic(journal, state)
            clocks.low_priority()
            try:
                result = monitor(child, readfd, collector, ack=ackwrite)
            finally:
                os.close(readfd)
                os.close(ackwrite)
            if result != RETURN_TO_TITLE:
                break
            if full:
                collector.finish(result, [], dict(timings, return_to_title=True))
            else:
                collector.stream.close()
            collector = None
            returns += 1
            timings["returns_to_title"] = returns
    finally:
        if child and child.poll() is None:
            child.terminate()
            try:
                child.wait(timeout=5)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait()
        errors = clocks.restore(original) if original is not None else []
        if original is not None:
            state.update(restored=not errors, restore_errors=errors)
            clocks.atomic(journal, state)
        if watch:
            watch.wait(timeout=5)
        if collector:
            if full:
                collector.finish(result, errors, timings)
            else:
                collector.stream.close()
        clocks.atomic(
            logs / "launch-summary.json",
            dict(
                exit_code=result,
                elapsed_seconds=time.monotonic() - started,
                telemetry="full" if full else "summary",
                restore_errors=errors,
                **timings,
            ),
        )
        if not errors:
            shutil.rmtree(lock)
        else:
            raise SetupError("Clock restoration failed. Restart the handheld.")
    if result:
        if collector and getattr(collector, "title_error", None):
            raise SetupError(collector.title_error)
        raise SetupError(
            f"Game stopped (exit {result}).\nSee logs/play.log or logs/latest-summary.txt.\nNo substitute renderer or sound driver was used."
        )
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--telemetry", choices=("summary", "full"), default="summary")
    parser.add_argument("--frames", type=int, default=0, help="optional frame limit for diagnostics")
    args = parser.parse_args()
    try:
        return run(full=args.telemetry == "full", frames=args.frames)
    except Exception as error:
        message = str(error)
        (APP / "logs").mkdir(exist_ok=True)
        (APP / "logs/last-error.txt").write_text(message + "\n")
        screen(APP, "error", message[:7000] + "\n\nPress a button to return to Ports.").wait()
        return 2


if __name__ == "__main__":

    def interrupted(*unused):
        raise KeyboardInterrupt("session interrupted")

    signal.signal(signal.SIGTERM, interrupted)
    signal.signal(signal.SIGINT, interrupted)
    sys.exit(main())
