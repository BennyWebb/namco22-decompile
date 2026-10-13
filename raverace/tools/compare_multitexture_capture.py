#!/usr/bin/env python3
"""Compare two deterministic P6 captures without image-library dependencies."""

import argparse
from collections import Counter
from pathlib import Path


def read_capture(path, fmt):
    data = Path(path).read_bytes()
    if fmt == "rgba":
        if len(data) != 640 * 480 * 4:
            raise ValueError(f"{path}: expected 640x480 raw RGBA bytes")
        return data
    header = data.split(b"\n", 3)
    if len(header) != 4 or header[:3] != [b"P6", b"640 480", b"255"]:
        raise ValueError(f"{path}: expected a 640x480 P6 capture")
    if len(header[3]) != 640 * 480 * 3:
        raise ValueError(f"{path}: truncated or oversized pixel data")
    return header[3]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("baseline")
    parser.add_argument("candidate")
    parser.add_argument("--format", choices=("ppm", "rgba"), default="ppm",
                        help="P6 framebuffer or bottom-up raw scene RGBA")
    args = parser.parse_args()
    baseline = read_capture(args.baseline, args.format)
    candidate = read_capture(args.candidate, args.format)
    channels = "RGBA" if args.format == "rgba" else "RGB"
    stride = len(channels)
    affected = []
    changes = [Counter() for _ in channels]
    for pixel in range(640 * 480):
        offset = pixel * stride
        if baseline[offset:offset + stride] != candidate[offset:offset + stride]:
            affected.append((pixel % 640, pixel // 640))
        for channel in range(stride):
            delta = candidate[offset + channel] - baseline[offset + channel]
            if delta:
                changes[channel][delta] += 1
    print(f"affected pixels: {len(affected)}/{640 * 480}")
    if affected:
        print("bounding box: (%d,%d)-(%d,%d)" % (
            min(x for x, _ in affected), min(y for _, y in affected),
            max(x for x, _ in affected), max(y for _, y in affected)))
    for name, counts in zip(channels, changes):
        n = sum(counts.values())
        total = sum(abs(delta) * count for delta, count in counts.items())
        print(f"{name}: affected={n}, max_abs={max(map(abs, counts), default=0)}, "
              f"mean_abs_affected={total / n if n else 0:.6f}, "
              f"mean_abs_full={total / (640 * 480):.9f}")
        print("  signed delta distribution: " + repr(dict(sorted(counts.items()))))
    if args.format == "ppm":
        print("alpha: unavailable (P6/GL_RGB capture)")
    else:
        print("coordinates: raw GL readback, origin at lower left")


if __name__ == "__main__":
    main()
