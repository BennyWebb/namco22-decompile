#!/usr/bin/env python3
"""Create a diagnostic workload manifest from an existing texture miss trace.

This is an experiment input, not a production preload list or pixel cache.
Palette fields are deliberately excluded: prepared data contains pen indices.
"""
import argparse
import csv
from pathlib import Path

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("trace", type=Path)
parser.add_argument("output", type=Path)
parser.add_argument("--last", type=int, default=5639)
args = parser.parse_args()
fields = ("min_u", "min_v", "range_u", "range_v", "texbank", "cmode", "cap", "decoded_w", "decoded_h")
with args.trace.open() as source:
    keys = sorted({tuple(int(row[k]) for k in fields)
                   for row in csv.DictReader(source) if int(row["frame"]) <= args.last})
with args.output.open("x") as target:
    target.write("RR_PENS_EXPERIMENT_1\n")
    for key in keys:
        target.write(" ".join(map(str, key)) + "\n")
print(f"{len(keys)} bakes, {sum(k[-1] * k[-2] for k in keys)} pen bytes")
