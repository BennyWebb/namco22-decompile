#!/usr/bin/env python3
"""Compare SHA256 of every compiler output, including metadata and local asset-kit files."""

import argparse
import hashlib
import json
from pathlib import Path


def hashes(root):
    return {
        str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in root.rglob("*") if p.is_file()
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reference", type=Path)
    parser.add_argument("streaming", type=Path)
    parser.add_argument("--manifest", type=Path)
    args = parser.parse_args()
    expected, actual = hashes(args.reference), hashes(args.streaming)
    differences = [name for name in sorted(expected.keys() | actual.keys()) if expected.get(name) != actual.get(name)]
    if differences:
        raise SystemExit("Compiler outputs differ: " + ", ".join(differences))
    if args.manifest:
        args.manifest.write_text("".join(f"{value}  {name}\n" for name, value in sorted(expected.items())))
    print(f"PASS all {len(expected)} compiler output files SHA256-identical")
