#!/usr/bin/env python3
"""Summarise retrieved A/B logs on host Python; validate complete real-ROM states."""
import csv
import hashlib
import json
import pathlib
import re
import statistics
import sys

root = pathlib.Path(sys.argv[1])
rows = []
states = {}
scene_states = {}
for path in sorted(root.glob("*-*-*.log")):
    workload, trial, mode = path.stem.split("-")
    if workload not in ("city", "race") or mode not in ("off", "on"):
        continue
    lines = path.read_text().splitlines()
    def fields(prefix):
        line = next(line for line in reversed(lines) if line.startswith(prefix))
        return dict(re.findall(r"(\w+)=([^ ]+)", line))
    state_bytes = path.with_suffix(".json").read_bytes()
    state = json.loads(state_bytes)
    frames = 1700 if workload == "city" else 3600
    assert path.with_suffix(".exit").read_text().strip() == "0", path
    assert state["frames"] == frames and state["traps"] == state["unmapped"] == state["romwrites"] == 0, path
    assert not state["dsp_fault"] and not state["sound_fault"], path
    assert states.setdefault(workload, state_bytes) == state_bytes, "State mismatch: " + str(path)
    rate = fields("[STAGES-RATE] total ")
    assert int(rate["steps"]) == frames - 1399
    assert int(rate["rendered"]) + int(rate["skipped"]) == int(rate["steps"])
    clock = fields("[GEOMETRY-RATE] ")
    geometry = fields("[GEOMETRY] ")
    assert geometry["test_present_every"] == "0", "Correctness schedule cannot be benchmarked"
    assert int(geometry["ticks"]) == frames
    assert geometry["skip"] == mode and geometry["effective"] == mode
    video = {key: geometry[key] for key in ("ticks", "walks", "direct_consumed", "digest")}
    assert scene_states.setdefault(workload, video) == video, "Video-tick mismatch: " + str(path)
    row = dict(workload=workload, trial=trial, mode=mode, **rate,
               resyncs=clock["resyncs"], discarded_lag_ms=clock["discarded_lag_ms"],
               state_sha256=hashlib.sha256(state_bytes).hexdigest())
    whole = fields("[TIMING] total ")
    assert int(whole["steps"]) == frames
    assert int(whole["renders"])+int(whole["skipped"]) == frames
    row.update({"whole_"+key: whole[key] for key in
                ("steps", "renders", "skipped", "sim_hz", "render_fps", "resyncs", "discarded_lag_ms")})
    memory = fields("[MEMORY] ")
    row.update({"video_" + key: value for key, value in video.items()})
    row["RSS_peak_mib"] = float(memory["RSS_peak_kib"])/1024
    row["MemAvailable_end_mib"] = float(memory["MemAvailable_kib"])/1024
    for cohort in ("rendered", "skipped"):
        data = fields("[STAGES] total timing=independent cohort=" + cohort + " ")
        simulation = sum(float(data[k]) for k in ("core68k_other", "dsp", "sound", "audio_mix", "serial", "board_io"))
        scene = sum(float(data[k]) for k in ("video_latch", "palette", "direct_decode", "list_decode", "sort", "scene_other"))
        row[cohort + "_simulation_ms"] = simulation
        row[cohort + "_scene_ms"] = scene
        for key, value in data.items():
            row[cohort + "_" + key] = value
        if mode == "on" and cohort == "skipped":
            assert all(float(data[k]) == 0 for k in ("direct_decode", "list_decode", "sort", "avg_quads")), path
    rows.append(row)
assert len(rows) == 12, f"Expected 12 complete runs, got {len(rows)}"
with (root / "comparison.tsv").open("w") as f:
    writer = csv.DictWriter(f, list(rows[0]), delimiter="\t")
    writer.writeheader()
    writer.writerows(rows)
print("workload mode sim_Hz render_FPS rendered skipped resyncs discarded_ms skipped_scene_ms rendered_scene_ms RSS_MiB")
for workload in ("city", "race"):
    for mode in ("off", "on"):
        selected = [r for r in rows if r["workload"] == workload and r["mode"] == mode]
        assert len(selected) == 3
        keys = ("sim_hz", "render_fps", "rendered", "skipped", "resyncs", "discarded_lag_ms",
                "skipped_scene_ms", "rendered_scene_ms", "RSS_peak_mib")
        print(workload, mode, *(f"{statistics.median(float(r[k]) for r in selected):.3f}" for k in keys))
print("All states, ranges and skipped-geometry invariants validated; details:", root / "comparison.tsv")
