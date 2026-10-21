#!/usr/bin/env python3
"""Prove that validation hooks and legacy renderers are absent from the product."""

import argparse
from pathlib import Path
import re
import subprocess

p = argparse.ArgumentParser()
p.add_argument("--build", type=Path, required=True)
p.add_argument("--nm", default="nm")
a = p.parse_args()
build = a.build / "gles3"
product = build / "rr_gles3"
validation = build / "rr_gles3_validation"
symbols = subprocess.check_output([a.nm, "-C", str(product)], text=True)
link = (build / "rr_gles3.map").read_text()
assert not re.search(
    r"rr_validation_|provenance_|record_splits|record_protected_visible|quad_gles2|tex_bake", symbols + link
)
strings = subprocess.check_output(["strings", str(product)], text=True)
assert not re.search(
    r"provenance|ENG_GLES3_(?:CAPTURE|PALETTE_REFERENCE|COMMAND_DUMP|RECEIPTS|IMAGE_HASH|SEMANTIC|FORCE_UNSUPPORTED|FACTORY_EEPROM)"
    r"|ENG_OBJLOG|PROPCYCL_CULLDUMP|GLES3_RECORD_DIAGNOSTICS",
    strings,
)
assert "gles3_validation_core.dir" not in link and "tests/async_io.c.o" not in link
assert "gles3_core.dir" in link and "GLES3 UNSUPPORTED:" in strings
assert "rr_validation_provenance" in subprocess.check_output([a.nm, str(validation)], text=True)
flags = (build / "CMakeFiles/gles3_core.dir/flags.make").read_text()
assert "-Wall" in flags and "-Wextra" in flags and "-w" not in flags.split()
assert "GLES3_VALIDATION" not in flags
validation_flags = (build / "CMakeFiles/gles3_validation_core.dir/flags.make").read_text()
assert "GLES3_VALIDATION" in validation_flags
assert "operator_settings.c.o" in link and "rr_operator_commit" in symbols
for target in ("rr_gles3_validation", "rr_gles3_setup", "rr_gles3_sound_oracle"):
    target_symbols = subprocess.check_output([a.nm, str(build / target)], text=True)
    target_map = (build / (target + ".map")).read_text()
    assert "rr_operator_" not in target_symbols and "operator_settings.c.o" not in target_map, target
print("PASS product link map/symbols/strings/flags; validation absent, strict unsupported path retained")
print("PASS operator settings linked only into product; absent from validation/setup/sound oracle")
