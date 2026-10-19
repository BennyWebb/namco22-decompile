#!/usr/bin/env python3
"""Separate Ports session: isolated saves, bounded async telemetry and watchdog."""

import pathlib, os, sys, json, time, subprocess, selectors, signal, shutil
import runtime_assets as assets, clocks, ctypes, hashlib

LIBC = ctypes.CDLL(None, use_errno=True)


def child_guard():
    LIBC.prctl(1, signal.SIGKILL, 0, 0, 0)
    if os.getppid() == 1:
        os._exit(96)


from telemetry import Collector

APP = pathlib.Path(__file__).resolve().parent
PROD = pathlib.Path("/userdata/roms/ports/rave-gles2")
SAVE = pathlib.Path("/userdata/saves/rave-racer-gles3")


def prepare_save(app, prod, save):
    save.mkdir(parents=True, exist_ok=True)
    nv = save / "rr_eeprom.nv"
    if not nv.exists():
        source = prod / "rr_eeprom.nv"
        if not source.exists():
            source = prod / "extracted/rv1eeprm.9e"
        data = source.read_bytes()
        assert len(data) == 8192
        temp = nv.with_suffix(".first")
        temp.write_bytes(data)
        os.replace(temp, nv)
        (save / "initial-source.txt").write_text(str(source) + "\n")
    assert nv.stat().st_size == 8192
    for name in ["controller-input.cfg", "rr_controls.cfg"]:
        if (prod / name).exists() and not (save / name).exists():
            shutil.copyfile(prod / name, save / name)
    return nv


def monitor(child, profile, collector):
    sel = selectors.DefaultSelector()
    buffers = {}
    for kind, fd in [("stderr", child.stderr.fileno()), ("stdout", child.stdout.fileno()), ("profile", profile)]:
        os.set_blocking(fd, False)
        sel.register(fd, selectors.EVENT_READ, kind)
        buffers[fd] = b""
    next_sample = time.monotonic()
    exit_time = None
    while sel.get_map():
        for key, mask in sel.select(0.05):
            data = os.read(key.fd, 65536)
            if not data:
                if buffers[key.fd]:
                    collector.line(key.data, buffers[key.fd].decode(errors="replace"))
                sel.unregister(key.fd)
                continue
            buffers[key.fd] += data
            while b"\n" in buffers[key.fd]:
                line, buffers[key.fd] = buffers[key.fd].split(b"\n", 1)
                collector.line(key.data, line.decode(errors="replace") + "\n")
            if len(buffers[key.fd]) > 1048576:
                collector.unmatched += 1
                buffers[key.fd] = b""
        now = time.monotonic()
        if now >= next_sample:
            collector.sample(child.pid)
            next_sample += 0.1
            if next_sample < now - 0.1:
                next_sample = now + 0.1
        if child.poll() is not None:
            if exit_time is None:
                exit_time = now
            if now - exit_time > 5:
                collector.unmatched += len(sel.get_map())
                break
    sel.close()
    return child.wait()


def run(app=APP, prod=PROD, save=SAVE, smoke=False, reference=False, attract_frames=0, record=False, smoke_seconds=120):
    if record and (reference or attract_frames):
        raise ValueError("recording requires interactive play")
    if (
        record
        and subprocess.run(
            ["pidof", "rr_gles3", "rr_gles3_record", "rr_p1b", "rr_gles3"], stdout=subprocess.DEVNULL
        ).returncode
        == 0
    ):
        raise RuntimeError("Exit the active game before recording")
    app.mkdir(exist_ok=True)
    lock = app / "active"
    journal = app / "clock-journal.json"
    if lock.exists():
        if journal.exists():
            old = json.loads(journal.read_text())
            if clocks.identity(old.get("owner", 0)) == old.get("owner_start"):
                raise RuntimeError("GLES3 session already running")
            if not old.get("restored"):
                errors = clocks.restore(old["original"])
                assert not errors, errors
        shutil.rmtree(lock)
    lock.mkdir()
    sessions = app / "sessions"
    sessions.mkdir(exist_ok=True)
    run = sessions / (time.strftime("%Y%m%d-%H%M%S") + "-" + str(os.getpid()) + "-" + str(time.time_ns() % 1000000000))
    run.mkdir()
    child = None
    collector = None
    watch = None
    result = 2
    extra = {}
    original = None
    recording = None
    try:
        nv = prepare_save(app, prod, save)
        if record:
            directory = app / "recordings" / run.name
            directory.mkdir(parents=True)
            recording = directory / "session.rec"
            shutil.copyfile(nv, directory / "initial-eeprom.nv")
            extra.update(recording=str(recording), initial_eeprom_sha256=hashlib.sha256(nv.read_bytes()).hexdigest())
        collector = Collector(run, nv, recording.parent / "telemetry" if recording else None)
        original = clocks.snapshot()
        j = dict(owner=os.getpid(), owner_start=clocks.identity(os.getpid()), original=original, restored=False)
        clocks.atomic(journal, j)
        ready = app / "watchdog.ready"
        ready.unlink(missing_ok=True)
        watch = subprocess.Popen(
            [sys.executable, str(app / "watchdog.py"), str(journal)],
            start_new_session=True,
            stdin=subprocess.DEVNULL,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        deadline = time.monotonic() + 5
        while not ready.exists() and time.monotonic() < deadline:
            time.sleep(0.05)
        assert ready.exists(), "clock watchdog unavailable"
        clocks.apply()
        t = time.monotonic()
        with (run / "assets.log").open("w") as log:
            pack, built = assets.ensure(app, prod / "extracted", log)
        extra.update(asset_cache_built=built, asset_cache_seconds=time.monotonic() - t, cache=str(pack))
        env = {k: v for k, v in os.environ.items() if not k.startswith(("ENG_", "RR_", "SND_"))}
        env.pop("LD_PRELOAD", None)
        readfd, writefd = os.pipe()
        env.update(
            SDL_VIDEODRIVER="mali",
            XDG_RUNTIME_DIR="/var/run",
            ENG_GLES3_ASSETS=str(pack),
            ENG_GLES3_SESSION="1",
            ENG_GLES3_SAVE_PATH=str(nv),
            ENG_GLES3_SUMMARY_PATH=str(app / "latest-summary.txt"),
        )
        env["LD_PRELOAD"] = str(app / "session-io.so")
        if smoke:
            env.update(
                LD_PRELOAD=env["LD_PRELOAD"] + ":" + str(app / "smoke-chord.so"),
                ENG_GLES3_SMOKE_CHORD_SECONDS=str(smoke_seconds),
            )
        cmd = [
            str(app / "rr_gles3"),
            str(prod / "extracted"),
            "--interactive",
            "--timing",
            "independent",
            "--presentation-thread",
            "on",
            "--audio",
            "on",
            "--overlay",
            "off",
            "--thread-profile",
            f"/proc/self/fd/{writefd}",
            "--profile-simulation",
            "--simulation-profile-from",
            "1",
        ]
        if record:
            env["ENG_GLES3_INITIAL_EEPROM"] = str(recording.parent / "initial-eeprom.nv")
            cmd += ["--record-input", str(recording)]
        if reference or attract_frames:
            cmd.remove("--interactive")
            cmd += ["--input-checkpoints", str(run / "states.jsonl")]
            if reference:
                cmd += ["--replay-input", os.environ["GLES3_REFERENCE_REPLAY"]]
            else:
                cmd += ["--frames", str(attract_frames)]
        if (save / "controller-input.cfg").exists():
            cmd += ["--es-input", str(save / "controller-input.cfg")]
        (run / "method.json").write_text(json.dumps(dict(cmd=cmd, env=env, save=str(nv)), indent=2) + "\n")
        child = subprocess.Popen(
            cmd,
            cwd=save,
            env=env,
            preexec_fn=child_guard,
            pass_fds=(writefd,),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        os.close(writefd)
        j.update(game=child.pid, game_start=clocks.identity(child.pid))
        clocks.atomic(journal, j)
        clocks.low_priority()
        result = monitor(child, readfd, collector)
        os.close(readfd)
    except BaseException as error:
        extra["supervisor_error"] = repr(error)
        print("GLES3 session error:", repr(error), file=sys.stderr, flush=True)
        if child and child.poll() is None:
            child.terminate()
            try:
                child.wait(timeout=5)
            except subprocess.TimeoutExpired:
                child.kill()
                child.wait()
    finally:
        errors = clocks.restore(original) if original is not None else []
        if journal.exists():
            j = json.loads(journal.read_text())
            j.update(restored=not errors, restore_errors=errors)
            clocks.atomic(journal, j)
        if watch:
            try:
                watch.wait(timeout=3)
            except subprocess.TimeoutExpired:
                watch.terminate()
                watch.wait(timeout=3)
        if recording:
            required = [recording] + [
                pathlib.Path(str(recording) + s) for s in [".meta", ".eeprom", ".end", ".states.jsonl"]
            ]
            try:
                complete = (
                    result == 0
                    and all(p.is_file() for p in required)
                    and all(p.stat().st_size for p in required[:-1])
                    and required[2].read_bytes() == (recording.parent / "initial-eeprom.nv").read_bytes()
                    and int(required[3].read_text()) == collector.replay_completed
                    and collector.replay_completed > 0
                )
            except (OSError, ValueError, TypeError):
                complete = False
            extra["recording_complete"] = complete
            if not complete and result == 0:
                result = 2
        if collector:
            collector.finish(result, errors, extra)
            for n in ["summary.json", "summary.txt"]:
                shutil.copyfile(run / n, app / ("latest-" + n))
        if recording:
            for n in ["summary.json", "summary.txt", "method.json", "assets.log"]:
                if (run / n).exists():
                    shutil.copyfile(run / n, recording.parent / n)
            manifest = dict(
                complete=complete,
                exit_code=result,
                binary_sha256=hashlib.sha256((app / "rr_gles3").read_bytes()).hexdigest(),
                files={
                    str(p.relative_to(recording.parent)): dict(
                        bytes=p.stat().st_size, sha256=hashlib.sha256(p.read_bytes()).hexdigest()
                    )
                    for p in recording.parent.rglob("*")
                    if p.is_file()
                },
            )
            (recording.parent / "recording.json").write_text(json.dumps(manifest, indent=2) + "\n")
        old = sorted(p for p in sessions.iterdir() if p.is_dir())
        for p in old[:-8]:
            shutil.rmtree(p)
        if not errors:
            shutil.rmtree(lock, ignore_errors=True)
        else:
            result = 91
    return result


if __name__ == "__main__":

    def stop(*unused):
        raise KeyboardInterrupt("session interrupted")

    signal.signal(signal.SIGTERM, stop)
    signal.signal(signal.SIGINT, stop)
    sys.exit(
        run(
            record="--record" in sys.argv,
            smoke="--smoke" in sys.argv,
            reference="--reference" in sys.argv,
            attract_frames=36000 if "--attract" in sys.argv else 0,
        )
    )
