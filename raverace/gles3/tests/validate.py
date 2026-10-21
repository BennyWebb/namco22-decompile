#!/usr/bin/env python3
"""Exact local-ROM replay gates. Git contains controller inputs and hashes; game payloads stay local."""

from pathlib import Path
import argparse, csv, hashlib, json, os, re, subprocess

from recordings import DEFAULT_RECORDINGS, IDENTITIES, generate_eeprom, prepare_recording

FIX = Path(__file__).resolve().parent / "fixtures"
p = argparse.ArgumentParser()
p.add_argument("case", choices=["reference", "fixture", "attract", "provenance", "unsupported", "sound", "full-race"])
p.add_argument("--build", type=Path, required=True)
p.add_argument("--roms", type=Path, required=True)
p.add_argument("--cache", type=Path, required=True)
p.add_argument("--recordings", type=Path, default=DEFAULT_RECORDINGS)
p.add_argument("--output", type=Path, required=True)
a = p.parse_args()
a.output.mkdir(parents=True, exist_ok=False)
out = a.output.resolve()
eeprom = generate_eeprom(
    a.build / "gles3/rr_gles3_validation", a.roms, a.cache, out / "factory.eeprom", calibrated=a.case == "full-race"
)


def sha(p):
    return hashlib.sha256(p.read_bytes()).hexdigest()


def recording(name):
    return prepare_recording(name, a.recordings, out / "inputs", eeprom)


def run(case, binary, *, sound=False):
    d = out / case
    d.mkdir()
    (d / "images").mkdir()
    env = {k: v for k, v in os.environ.items() if not k.startswith(("RR_", "SND_", "ENG_"))}
    env.pop("LD_PRELOAD", None)
    env.update(
        SDL_VIDEODRIVER="offscreen",
        SDL_AUDIODRIVER="dummy",
        LP_NUM_THREADS="1",
        ENG_GLES3_HOST="1",
        ENG_GLES3_ASSETS=str(a.cache.resolve()),
        ENG_GLES3_SUMMARY_PATH=str(d / "latest-summary.txt"),
    )
    mode = case.rsplit("-", 1)[0] if sound else case
    name = "reference" if mode in ("provenance", "unsupported") else mode
    cmd = [
        str(binary.resolve()),
        str(a.roms.resolve()),
        "--audio",
        "off",
        "--overlay",
        "off",
        "--input-checkpoints",
        str(d / "states.jsonl"),
    ]
    env["ENG_GLES3_INITIAL_EEPROM"] = str(eeprom)
    if name == "attract":
        cmd += ["--frames", "36000"]
    else:
        q = recording(name)
        cmd += ["--replay-input", str(q)]
        env["ENG_GLES3_INITIAL_EEPROM"] = str(q) + ".eeprom"
    if sound:
        if mode == "full-race":
            # Coverage receipts are diagnostics: request them explicitly even in the PCM gate.
            cmd += ["--thread-profile", str(d / "thread.csv")]
        cmd += ["--audio-checksum-from", "1"]
        env["ENG_SOUND_REFERENCE"] = str(d / "audio.jsonl")
    else:
        cmd += ["--thread-profile", str(d / "thread.csv"), "--profile-simulation", "--simulation-profile-from", "1"]
        env.update(
            ENG_GLES3_CAPTURE="hash-only",
            ENG_GLES3_IMAGES=str(d / "images"),
            ENG_GLES3_IMAGE_HASH=str(d / "images.csv"),
        )
        if mode in ("reference", "provenance", "full-race"):
            env.update(
                ENG_GLES3_RECEIPTS=str(d / "commands.csv"),
                ENG_GLES3_NORMALIZED_RECEIPTS=str(d / "normalized.csv"),
                ENG_GLES3_SEMANTIC=str(d / "semantic.csv"),
            )
        if mode == "full-race":
            env["ENG_GLES3_CAPTURE_TICKS"] = (
                "500,800,1320,1800,3000,4500,6006,8000,10000,11000,11721,11800,12000,12500,13000,13263,13300,13896"
            )
            env["ENG_GLES3_RECEIPTS_FROM"] = "1"
        if mode == "fixture":
            env["ENG_GLES3_CAPTURE_TICKS"] = "1,159,300,435,600,900,1200,1500,1700"
            env["ENG_GLES3_RECEIPTS_FROM"] = "1"
            env.update(
                ENG_GLES3_RECEIPTS=str(d / "commands.csv"),
                ENG_GLES3_NORMALIZED_RECEIPTS=str(d / "normalized.csv"),
                ENG_GLES3_SEMANTIC=str(d / "semantic.csv"),
            )
        if mode == "attract":
            env["ENG_GLES3_CAPTURE_TICKS"] = "1,159,435,3462,6006,12000,18000,24000,30000,36000"
        if mode == "provenance":
            env.update(
                ENG_GLES3_PROVENANCE="1",
                ENG_GLES3_PROVENANCE_DIR=str(d / "images"),
                ENG_GLES3_PALETTE_REFERENCE=str(d / "palettes.bin"),
            )
        if mode == "unsupported":
            env["ENG_GLES3_FORCE_UNSUPPORTED"] = "20"
    (d / "method.json").write_text(
        json.dumps(
            dict(
                cmd=cmd,
                env={k: v for k, v in env.items() if k.startswith(("ENG_", "SDL_", "LP_"))},
                binary_sha256=sha(binary),
            ),
            indent=2,
        )
    )
    with (d / "stdout.jsonl").open("w") as f, (d / "stderr.txt").open("w") as log:
        r = subprocess.run(cmd, env=env, stdout=f, stderr=log)
    (d / "exit.txt").write_text(str(r.returncode) + "\n")
    log = (d / "stderr.txt").read_text()
    if mode == "unsupported":
        assert r.returncode == 86
        message = (d / "latest-summary.txt").read_text().splitlines()[0]
        assert message.startswith("GLES3 UNSUPPORTED: tick=20 state=0/0 key=69/4660") and message in log
        return d
    assert r.returncode == 0, r.returncode
    assert "[SND] TRANSLATED DRIVER" not in log and "GLES3 UNSUPPORTED" not in log
    records = [hashlib.sha256(s.encode() + b"\n").hexdigest() for s in (d / "states.jsonl").read_text().splitlines()]
    if mode in ("reference", "provenance", "fixture", "attract"):
        expected = json.loads((FIX / (mode + "-checkpoints.json")).read_text())
        assert records == [x["sha256"] for x in expected], ("checkpoints", mode)
    if not sound:
        if mode in ("reference", "provenance", "fixture"):
            identity = "reference" if mode == "provenance" else mode
            expected_commands = (FIX / (identity + "-commands.csv")).read_bytes()
            assert (d / "normalized.csv").read_bytes() == expected_commands, "commands"
            assert len(list(csv.DictReader((d / "normalized.csv").open()))) > 0
            assert (d / "semantic.csv").read_bytes() == (FIX / (identity + "-semantic.csv")).read_bytes(), "semantic"
        if mode == "provenance":
            expected = json.loads((FIX / "provenance-images.json").read_text())
            assert {p.name: sha(p) for p in (d / "images").iterdir()} == expected, "provenance images"
        elif mode != "full-race":
            assert (d / "images.csv").read_bytes() == (FIX / (mode + "-images.csv")).read_bytes(), "images"
        if mode == "reference":
            boot = re.findall(r"GLES3-BOOT-ORACLE\] tick=(\d+).*differences=(\d+)", log)
            assert len(boot) == 159 and all(int(x[1]) == 0 for x in boot)
    if mode == "full-race":
        expected = json.loads((FIX / "full-race.json").read_text())
        assert records == expected["checkpoints"], "full-race checkpoints"
        coverage = re.findall(r"RENDER-COVERAGE\] tick=(\d+) main=(\d+) sub=(\d+)", log)
        counts = {}
        for tick, main, sub in coverage:
            key = main + "/" + sub
            counts[key] = counts.get(key, 0) + 1
        assert counts == expected["state_pictures"], "full-race coverage"
        assert counts.get("3/0", 0) == 10674 and counts.get("4/0", 0) > 0
        assert counts.get("5/0", 0) == 1543, "bonus stage not reached"
        for filename in (["audio.jsonl"] if sound else ["normalized.csv", "semantic.csv", "images.csv"]):
            assert sha(d / filename) == expected["files"][filename], filename
        if not sound:
            for filename, count in expected["receipt_rows"].items():
                assert len(list(csv.DictReader((d / filename).open()))) == count, filename
    final = [json.loads(s) for s in (d / "stdout.jsonl").read_text().splitlines() if s.startswith('{"schema":')][-1]
    final_digest = hashlib.sha256(json.dumps(final, sort_keys=True, separators=(",", ":")).encode()).hexdigest()
    expected_final = expected["final"] if mode == "full-race" else (FIX / (mode + "-final.sha256")).read_text().strip()
    assert final_digest == expected_final, ("final state", mode)
    return d


if a.case == "sound":
    results = {}
    for name in ["reference", "attract", "fixture"]:
        paths = [
            run(name + "-" + tag, a.build / "gles3" / binary, sound=True)
            for tag, binary in [("translated", "rr_gles3"), ("oracle", "rr_gles3_sound_oracle")]
        ]

        def audio(d):
            return [
                json.loads(s) for s in (d / "audio.jsonl").read_text().splitlines() if s.startswith('{"audio_frame":')
            ]

        x, y = map(audio, paths)
        assert x == y and x, "PCM/voices"
        ticks = 36000 if name == "attract" else IDENTITIES[name]["ticks"]
        assert [row["audio_frame"] for row in x] == list(range(1, ticks + 1)), "missing audio ticks"
        assert (paths[0] / "states.jsonl").read_bytes() == (paths[1] / "states.jsonl").read_bytes()
        if name == "reference":
            assert x == [json.loads(s) for s in (FIX / "sound-reference.jsonl").read_text().splitlines()]
        results[name] = len(x)
        print("EXACT SOUND", name, len(x), flush=True)
    (out / "result.json").write_text(json.dumps(results, indent=2))
elif a.case == "full-race":
    run("full-race", a.build / "gles3/rr_gles3_validation")
    paths = [
        run("full-race-" + tag, a.build / "gles3" / binary, sound=True)
        for tag, binary in [("translated", "rr_gles3"), ("oracle", "rr_gles3_sound_oracle")]
    ]
    assert (paths[0] / "audio.jsonl").read_bytes() == (paths[1] / "audio.jsonl").read_bytes()
    audio = [json.loads(line) for line in (paths[0] / "audio.jsonl").read_text().splitlines()]
    assert [row["audio_frame"] for row in audio] == list(range(1, 13938))
    (out / "result.json").write_text(json.dumps(dict(case=a.case, exact=True, sound_ticks=len(audio))))
    print("EXACT FULL RACE", len(audio), flush=True)
else:
    d = run(a.case, a.build / "gles3/rr_gles3_validation")
    (out / "result.json").write_text(json.dumps(dict(case=a.case, exact=True)))
    print("EXACT", a.case, flush=True)
