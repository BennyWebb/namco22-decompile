#!/usr/bin/env python3
"""Summarise matched first-lap A/B runs and five-second emulated sections.

Usage: analyse_multicore_replay.py RESULTS_DIRECTORY FIRST_FRAME LAST_FRAME
"""
import csv
import json
import math
import pathlib
import re
import statistics as stats
import sys
from collections import defaultdict

root = pathlib.Path(sys.argv[1])
first, last = map(int, sys.argv[2:4])


def percentile(values, q):
    return sorted(values)[max(0, math.ceil(len(values) * q) - 1)]


def tagged(line):
    return dict(re.findall(r"([A-Za-z_][A-Za-z_0-9]*)=([^ ]+)", line))


def monitor_summary(path, begin, end):
    snapshots = []
    current = None
    for line in path.read_text().splitlines():
        if line.startswith("SAMPLE "):
            if current:
                snapshots.append(current)
            current = {"time": float(line.split()[1]), "threads": {}, "cores": {},
                       "freq": [], "thermal": [], "rss": [], "hwm": [], "wchan": {}}
        elif current is None:
            continue
        elif re.match(r"^\d+ \([^)]*\) [RSDTIZ] ", line):
            match = re.match(r"(\d+) \(([^)]*)\) ([RSDTIZ]) (.*)", line)
            fields = match[4].split()
            if len(fields) >= 36:
                current["threads"][int(match[1])] = (match[2], int(fields[10]) + int(fields[11]),
                                                       int(fields[35]), match[3])
        elif re.match(r"^cpu[0-3] ", line):
            parts = line.split()
            current["cores"][parts[0]] = list(map(int, parts[1:]))
        elif line.startswith("WCHAN "):
            parts = line.split(maxsplit=2)
            if len(parts) == 3:
                current["wchan"][int(parts[1])] = parts[2]
        elif "scaling_cur_freq " in line:
            current["freq"].append(int(line.split()[-1]))
        elif re.match(r"^/sys/class/thermal/thermal_zone[0-3]/temp ", line):
            current["thermal"].append(int(line.split()[-1]))
        elif line.startswith("VmRSS:"):
            current["rss"].append(int(line.split()[1]))
        elif line.startswith("VmHWM:"):
            current["hwm"].append(int(line.split()[1]))
    if current:
        snapshots.append(current)
    snapshots = [sample for sample in snapshots if begin <= sample["time"] <= end]
    if len(snapshots) < 2:
        return {}
    cpu_ticks = defaultdict(int)
    cpu_observations = defaultdict(list)
    thread_states = defaultdict(lambda: defaultdict(int))
    thread_wchan = defaultdict(lambda: defaultdict(int))
    migrations = defaultdict(int)
    core_usage = defaultdict(list)
    for before, after in zip(snapshots, snapshots[1:]):
        elapsed = after["time"] - before["time"]
        if elapsed <= 0:
            continue
        for tid, (name, tick, core, state) in after["threads"].items():
            cpu_observations[name].append(core)
            thread_states[name][state] += 1
            channel = after["wchan"].get(tid)
            if channel:
                thread_wchan[name][channel] += 1
            old = before["threads"].get(tid)
            if old:
                cpu_ticks[name] += max(0, tick - old[1])
                migrations[name] += old[2] != core
        for core in ("cpu0", "cpu1", "cpu2", "cpu3"):
            a, b = before["cores"].get(core), after["cores"].get(core)
            if a and b:
                total = sum(b) - sum(a)
                idle = (b[3] + b[4]) - (a[3] + a[4])
                if total > 0:
                    core_usage[core].append((total - idle) / total)
    elapsed = snapshots[-1]["time"] - snapshots[0]["time"]
    return {
        "thread_core_equivalents": {k: v / (100 * elapsed) for k, v in cpu_ticks.items()},
        "thread_observed_cores": {k: sorted(set(v)) for k, v in cpu_observations.items()},
        "thread_state_samples": {k: dict(v) for k, v in thread_states.items()},
        "thread_wchan_samples": {k: dict(v) for k, v in thread_wchan.items()},
        "thread_sampled_migrations": dict(migrations),
        "core_utilisation": {k: stats.mean(v) for k, v in core_usage.items()},
        "cpu_frequency_mhz_min_max": [min(x for s in snapshots for x in s["freq"]) / 1000,
                                       max(x for s in snapshots for x in s["freq"]) / 1000],
        "thermal_c_min_max": [min(x for s in snapshots for x in s["thermal"]) / 1000,
                              max(x for s in snapshots for x in s["thermal"]) / 1000],
        "peak_rss_mib": max((x for s in snapshots for x in (s["hwm"] or s["rss"])), default=0) / 1024,
    }


def section(rows, ticks, lo, hi):
    begin, end = ticks[lo - 1], ticks[hi]
    selected = [r for r in rows if begin < float(r["presented"]) <= end and int(r["frame"]) >= lo]
    intervals = [float(b["presented"]) - float(a["presented"])
                 for a, b in zip(rows, rows[1:])
                 if begin < float(b["presented"]) <= end and int(a["frame"]) >= lo]
    if not selected or not intervals:
        raise ValueError(f"No presentation samples in {lo}..{hi}")
    slow = sorted(intervals, reverse=True)[:math.ceil(len(intervals) * .01)]
    mean_ms = lambda key: stats.mean(float(r.get(key, 0)) for r in selected) * 1000
    median_ms = lambda key: stats.median(float(r.get(key, 0)) for r in selected) * 1000
    p95_ms = lambda key: percentile([float(r.get(key, 0)) for r in selected], .95) * 1000
    return {
        "frames": f"{lo}-{hi}", "rendered": len(selected), "fps": len(selected) / (end - begin),
        "sim_hz": (hi - lo + 1) / (end - begin),
        "frame_ms_mean": stats.mean(intervals) * 1000,
        "frame_ms_median": stats.median(intervals) * 1000,
        "low1_fps": 1 / stats.mean(slow),
        "frame_ms_p99": percentile(intervals, .99) * 1000,
        "frame_ms_max": max(intervals) * 1000,
        "presentation_cpu_ms": mean_ms("cpu"), "prep_cpu_ms": mean_ms("prep_cpu"),
        "scene_ms": mean_ms("scene_wall"), "hud_cpu_ms": mean_ms("hud_cpu"),
        "direct_cpu_ms": mean_ms("direct_cpu"), "list_cpu_ms": mean_ms("list_cpu"),
        "sort_cpu_ms": mean_ms("sort_cpu"),
        "draw_ms": mean_ms("draw_wall"), "submit_ms": mean_ms("submission_wall"),
        "decode_ms": mean_ms("decode_wall"), "upload_ms": mean_ms("upload_wall"),
        "swap_ms": mean_ms("swap_wall"), "age_ms": mean_ms("publication_age"),
        "age_ms_median": median_ms("publication_age"),
        "age_ms_p95": p95_ms("publication_age"),
        "snapshot_copy_ms": mean_ms("snapshot_copy_wall"),
        "snapshot_queue_ms": mean_ms("snapshot_queue_wait"),
        "prep_wall_ms": mean_ms("prep_wall"),
        "prepared_wait_ms": mean_ms("prepared_wait"),
        "draws": stats.mean(float(r["draws"]) for r in selected),
        "triangles": stats.mean(float(r["triangles"]) for r in selected),
        "binds": stats.mean(float(r["binds"]) for r in selected),
        "misses": sum(int(r["misses"]) for r in selected),
        "uploads": sum(int(r["uploads"]) for r in selected),
        "evictions": sum(int(r["evictions"]) for r in selected),
        "texture_peak_mib": max(int(r["texture_bytes"]) for r in selected) / 1048576,
        "queue_peak": max(int(r["queue_ready"]) for r in selected),
    }


runs = []
for path in sorted(root.glob("*.presentations.csv")):
    name = path.name.removesuffix(".presentations.csv")
    variant, pair = name.rsplit("-", 1)
    rows = list(csv.DictReader(path.open()))
    ticks = {int(r["frame"]): float(r["completed"])
             for r in csv.DictReader((root / f"{name}.ticks.csv").open())}
    log = (root / f"{name}.log").read_text().splitlines()
    state = json.loads((root / f"{name}.json").read_text())
    assert not state["traps"] and not state["dsp_fault"] and not state["sound_fault"]
    sections = [section(rows, ticks, lo, min(lo + 299, last))
                for lo in range(first, last + 1, 300)]
    whole = section(rows, ticks, first, last)
    rate = tagged(next(l for l in log if l.startswith("[THREAD-RATE]")))
    audio = tagged(next(l for l in log if l.startswith("[AUDIO-OUTPUT]")))
    prep = next((tagged(l) for l in log if l.startswith("[PREP-RATE]")), {})
    runs.append({"name": name, "variant": variant, "pair": int(pair),
                 "whole": whole, "sections": sections,
                 "resyncs": int(rate["resyncs"]), "discarded_lag_ms": float(rate["discarded_lag_ms"]),
                 "snapshot_drops": int(rate["dropped"]),
                 "prepared": int(prep.get("prepared", 0)),
                 "dropped_prepared": int(prep.get("dropped_prepared", 0)),
                 "audio_underrun_samples": int(audio["underrun_samples"]),
                 "monitor": monitor_summary(root / f"{name}.monitor", ticks[first - 1], ticks[last])})

assert runs
medians = {}
for variant in ("baseline", "candidate"):
    subset = [r for r in runs if r["variant"] == variant]
    assert len(subset) >= 5
    medians[variant] = {key: stats.median(r["whole"][key] for r in subset)
                        for key in subset[0]["whole"] if isinstance(subset[0]["whole"][key], (int, float))}
    for key in ("resyncs", "discarded_lag_ms", "snapshot_drops", "prepared", "dropped_prepared",
                "audio_underrun_samples"):
        medians[variant][key] = stats.median(r[key] for r in subset)
    medians[variant]["peak_rss_mib"] = stats.median(r["monitor"].get("peak_rss_mib", 0) for r in subset)

(root / "summary.json").write_text(json.dumps({"runs": runs, "medians": medians}, indent=2) + "\n")
with (root / "sections.csv").open("w", newline="") as out:
    writer = csv.DictWriter(out, fieldnames=["variant", "pair", *runs[0]["sections"][0].keys()])
    writer.writeheader()
    for run in runs:
        for item in run["sections"]:
            writer.writerow({"variant": run["variant"], "pair": run["pair"], **item})
for key in ("fps", "low1_fps", "frame_ms_p99", "frame_ms_max", "sim_hz", "presentation_cpu_ms",
            "prep_cpu_ms", "scene_ms", "hud_cpu_ms", "draw_ms", "submit_ms", "age_ms_median", "age_ms_p95",
            "snapshot_queue_ms", "prep_wall_ms", "prepared_wait_ms",
            "snapshot_drops", "dropped_prepared", "peak_rss_mib"):
    print(f"{key}: baseline={medians['baseline'][key]:.3f} candidate={medians['candidate'][key]:.3f}")
print(f"Detailed report: {root / 'summary.json'}")
