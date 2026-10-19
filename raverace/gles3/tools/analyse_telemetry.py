#!/usr/bin/env python3
"""Summarize complete cone device profiles, including batching split reasons."""

import argparse, csv, gzip, json, math, pathlib, statistics

p = argparse.ArgumentParser()
p.add_argument("run", type=pathlib.Path)
a = p.parse_args()
base = a.run
with gzip.open(base / "telemetry/profile.log.gz", "rt") as f:
    rows = list(csv.DictReader(f))
assert rows and [int(x["frame"]) for x in rows] == list(range(1, len(rows) + 1))


def dist(v):
    v = sorted(v)
    return dict(median=statistics.median(v), p99=v[math.ceil(len(v) * 0.99) - 1], max=v[-1])


def section(lo, hi):
    r = [x for x in rows if lo <= int(x["frame"]) <= hi]
    out = dict(pictures=len(r))
    for key in [
        "cpu",
        "draw_wall",
        "prep_wall",
        "gles3_compile_ms",
        "gles3_walk_ms",
        "simulation_ms",
        "draws",
        "split_protected",
        "split_page",
        "split_scissor",
        "split_mode",
        "protected_visible",
    ]:
        if key not in r[0]:
            continue
        out[key] = dist([float(x[key]) * (1000 if key in ("cpu", "draw_wall", "prep_wall") else 1) for x in r])
    out["over_panel"] = {
        k: sum(float(x[k]) * (1000 if k in ("cpu", "draw_wall", "prep_wall") else 1) > 1000 / 59.7104 for x in r)
        for k in ("cpu", "draw_wall", "prep_wall", "gles3_compile_ms", "gles3_walk_ms")
    }
    for target in [lo, hi, 11822, 11823, 11824, 11825, 11826, 11827, 11828, 11936]:
        if lo <= target <= hi:
            out.setdefault("ticks", {})[target] = {
                k: rows[target - 1].get(k)
                for k in [
                    "draws",
                    "cpu",
                    "prep_wall",
                    "gles3_compile_ms",
                    "gles3_walk_ms",
                    "split_protected",
                    "split_page",
                    "split_scissor",
                    "split_mode",
                    "protected_visible",
                ]
            }
    return out


result = dict(all=section(1, len(rows)))
if len(rows) == 13808:
    result.update(bonus=section(11598, 13225), first_burst=section(11824, 12228))
elif len(rows) >= 1800:
    result["race"] = section(1800, len(rows))
(base / "metrics.json").write_text(json.dumps(result, indent=2) + "\n")
print(json.dumps(result, indent=2))
