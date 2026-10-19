#!/usr/bin/env python3
"""Build and run all ROM-dependent host gates from a clone plus a ROM directory."""

from pathlib import Path
import argparse
import json
import subprocess
import sys

from recordings import DEFAULT_RECORDINGS, host_environment

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
p = argparse.ArgumentParser(description=__doc__)
p.add_argument("--roms", type=Path, required=True)
p.add_argument("--work", type=Path, default=ROOT / "build/regression")
p.add_argument("--recordings", type=Path, default=DEFAULT_RECORDINGS)
a = p.parse_args()
work = a.work.resolve()
work.mkdir(parents=True, exist_ok=False)
build, cache = work / "build", work / "build/validation-assets"
roms = a.roms.resolve()


def run(label, cmd, env=None):
    print("RUN", label, flush=True)
    with (work / (label + ".log")).open("w") as log:
        subprocess.run(list(map(str, cmd)), check=True, env=env, stdout=log, stderr=subprocess.STDOUT)


run(
    "configure",
    [
        "cmake",
        "-S",
        ROOT,
        "-B",
        build,
        "-DRR_BUILD_GLES3=ON",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DRR_ROM_DIR=" + str(roms),
    ],
)
run(
    "build",
    [
        "cmake",
        "--build",
        build,
        "-j2",
        "--target",
        "rr_gles3",
        "rr_gles3_validation",
        "rr_gles3_sound_oracle",
        "test_projection",
        "test_cull",
        "test_cache",
        "test_staging",
    ],
)
run("assets", ["cmake", "--build", build, "--target", "rr_validation_asset_kit"])
for case in ("reference", "fixture", "attract", "provenance", "unsupported", "sound"):
    run(
        case,
        [
            sys.executable,
            HERE / "validate.py",
            case,
            "--build",
            build,
            "--roms",
            roms,
            "--cache",
            cache,
            "--recordings",
            a.recordings.resolve(),
            "--output",
            work / case,
        ],
    )
run(
    "rom-provenance",
    [sys.executable, HERE / "provenance.py", work / "provenance/provenance", "--roms", roms, "--cache", cache],
)
run(
    "record-replay",
    [sys.executable, HERE / "test_input_replay.py", build / "gles3/rr_gles3", roms, work / "reference/factory.eeprom"],
    env=host_environment(cache),
)
run("ctest", ["ctest", "--test-dir", build, "--output-on-failure"])
run("product-audit", [sys.executable, HERE / "audit_product.py", "--build", build])
(work / "result.json").write_text(
    json.dumps(
        {
            "exact": True,
            "cases": [
                "reference",
                "fixture",
                "attract",
                "provenance",
                "unsupported",
                "sound",
                "record-replay",
                "ctest",
                "product-audit",
            ],
        },
        indent=2,
    )
    + "\n"
)
print("ALL HOST GATES EXACT", work, flush=True)
