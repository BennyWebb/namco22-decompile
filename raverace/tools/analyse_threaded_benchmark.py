#!/usr/bin/env python3
"""Validate five alternating A/B pairs per workload and summarise their medians.
Usage: analyse_threaded_benchmark.py RESULTS_DIRECTORY
"""
import csv
import json
import pathlib
import re
import statistics as st
import sys

root = pathlib.Path(sys.argv[1])
def fields(line):
    return dict(re.findall(r"(\w+)=([^ ]+)", line))

def monitor(path, begin, end, baseline_duration):
    samples = []
    for line in path.read_text().splitlines():
        if line.startswith("SAMPLE "):
            samples.append({"time": float(line.split()[1]), "cores": {}, "tasks": {}, "freq": [], "temp": []})
        elif samples:
            s = samples[-1]
            if re.match(r"cpu[0-3] ", line):
                words = line.split()
                values = list(map(int, words[1:9]))
                s["cores"][words[0]] = (sum(values), values[3] + values[4])
            elif (match := re.match(r"(\d+) \(([^)]*)\) (.*)", line)):
                words = match[3].split()
                s["tasks"][int(match[1])] = (match[2], int(words[11]) + int(words[12]))
            elif "scaling_cur_freq " in line:
                s["freq"].append(int(line.split()[-1]))
            elif "/thermal_zone" in line and "/temp " in line:
                s["temp"].append(int(line.split()[-1]))
    if begin is None:  # Old baseline reports duration, not an absolute epoch.
        end = samples[-1]["time"]
        begin = end - baseline_duration
    total = {f"cpu{i}": [0, 0] for i in range(4)}
    task_totals = {}
    duration = 0
    temperatures, frequencies = [], []
    for a, b in zip(samples, samples[1:]):
        if not begin <= (a["time"] + b["time"]) / 2 <= end:
            continue
        duration += b["time"] - a["time"]
        for core in total:
            if core in a["cores"] and core in b["cores"]:
                for j in range(2):
                    total[core][j] += b["cores"][core][j] - a["cores"][core][j]
        for tid, (name, ticks) in b["tasks"].items():
            if tid in a["tasks"]:
                task_totals[name] = task_totals.get(name, 0) + ticks - a["tasks"][tid][1]
        temperatures.extend(b["temp"])
        frequencies.extend(b["freq"])
    # The handheld's userspace CLK_TCK is 100; kernel CONFIG_HZ is unrelated.
    return {"core_busy_pct": [100 * (1 - idle / ticks) if ticks else 0 for ticks, idle in total.values()],
            "task_core_pct": {name: ticks / duration for name, ticks in task_totals.items()} if duration else {},
            "sampled_seconds": duration,
            "temperature_millic_range": [min(temperatures), max(temperatures)] if temperatures else [],
            "cpu_frequency_khz_range": [min(frequencies), max(frequencies)] if frequencies else [],
            "baseline_endpoint_proxy": "thread" not in path.name}

def load(name):
    path = root / name
    assert path.with_suffix(".exit").read_text().strip() == "0", name
    state = json.loads(path.with_suffix(".json").read_text())
    assert state["traps"] == state["unmapped"] == state["romwrites"] == 0
    assert not state["dsp_fault"] and not state["sound_fault"]
    result = {"state": state}
    for line in path.with_suffix(".log").read_text().splitlines():
        for tag, key in (("[SIM-RATE]", "rate"), ("[THREAD-RATE]", "rate"),
                         ("[THREAD-DETAIL]", "detail"), ("[THREAD-PERIOD]", "period"),
                         ("[PROFILE] total", "profile"), ("[PROFILE-CACHE] total", "cache"),
                         ("[MEMORY]", "memory"), ("[BOARD-STATE]", "board"),
                         ("[GEOMETRY-RATE]", "lag")):
            if line.startswith(tag):
                result[key] = fields(line)
    baseline_duration = 0
    if "threaded" in name:
        period = result["period"]
        begin, end = float(period["start"]), float(period["end"])
        records = list(csv.DictReader(path.with_suffix(".csv").open()))
        completed = [r for r in records if float(r["end"]) <= end]
        for source, target in (("cpu", "presentation_cpu"), ("scene_wall", "scene_wall"),
                               ("publication_age", "publication_age")):
            values = sorted(1000 * float(r[source]) for r in completed)
            result[target] = {"median_ms": st.median(values),
                              "p95_ms": values[(len(values)*95+99)//100-1], "max_ms": max(values)}
        result["age_ticks"] = {"median": st.median(int(r["age_ticks"]) for r in completed),
                               "max": max(int(r["age_ticks"]) for r in completed)}
    else:
        begin = end = None
        baseline_duration = float(result["rate"]["wall_s"])
    result["utilisation"] = monitor(path.with_suffix(".monitor"), begin, end, baseline_duration)
    return result

runs = {}
workloads = [w for w in ("dense", "extended") if (root / f"{w}-1-baseline.exit").exists()]
assert workloads, "No completed workloads"
for workload in workloads:
    reference = None
    for trial in range(1, 6):
        for mode in ("baseline", "threaded"):
            name = f"{workload}-{trial}-{mode}"
            r = runs[name] = load(name)
            comparison = r["state"], r["board"]
            reference = comparison if reference is None else reference
            assert comparison == reference, name
    print(workload)
    for mode in ("baseline", "threaded"):
        group = [runs[f"{workload}-{trial}-{mode}"] for trial in range(1, 6)]
        print(" ", mode)
        for category, key in (("rate", "sim_hz"), ("rate", "render_fps"),
                              ("profile", "scene"), ("profile", "submission"),
                              ("profile", "draw_call"), ("profile", "swap"),
                              ("profile", "avg_draws"), ("profile", "avg_triangles"),
                              ("memory", "RSS_kib")):
            values = [float(r[category][key]) for r in group]
            print(f"   {category}.{key}: median={st.median(values):.6f} range={min(values):.6f}..{max(values):.6f}")
        if mode == "threaded":
            for key in ("sim_cpu_ms", "presentation_cpu_ms", "copy_cpu_ms", "dropped",
                        "resyncs", "discarded_lag_ms", "publication_age_ms", "cpu_cores_equivalent"):
                values = [float(r["rate"][key]) for r in group]
                print(f"   {key}: median={st.median(values):.6f} range={min(values):.6f}..{max(values):.6f}")
        print("   core busy % medians:", [round(st.median(r["utilisation"]["core_busy_pct"][i] for r in group), 2) for i in range(4)])
    changes = [100*(float(runs[f"{workload}-{trial}-threaded"]["rate"]["render_fps"])/
                   float(runs[f"{workload}-{trial}-baseline"]["rate"]["render_fps"])-1) for trial in range(1, 6)]
    print("  Paired FPS changes %:", changes, "median", st.median(changes))
(root / "summary.json").write_text(json.dumps(runs, indent=2) + "\n")
print(f"All {len(runs)} exits and per-workload simulation/board states match")
