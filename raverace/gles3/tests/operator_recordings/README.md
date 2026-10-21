# Operator-setting oracles

These are controller inputs only: `F tick inputs steering gas brake`, with
the existing TEST switch held while operating the original TEST/SERVICE menu.
Steering moves the cursor/value and gas confirms. Releasing TEST lets the
game's own writer save. No recording contains RAM writes, EEPROM images,
snapshots, or ROM payloads. `cases.json` pins their hashes and tick counts.

Run from `raverace/`, using an unused directory inside the ignored build tree:

```sh
cmake -S . -B build/operator-host -DRR_BUILD_GLES3=ON \
  -DCMAKE_BUILD_TYPE=Release -DRR_ROM_DIR=/absolute/path/to/roms
cmake --build build/operator-host -j2 --target rr_gles3_setup \
  rr_operator_oracle test_operator_settings rr_validation_asset_kit
python3 gles3/tests/test_operator_settings.py --build build/operator-host \
  --roms /absolute/path/to/roms --cache build/operator-host/validation-assets \
  --output build/operator-results
```

The existing setup recipe first generates the calibrated image and asserts
SHA256 `81e19e9ce2fca04dbbd487b12e85cc473e30e61f2797c703743c9b86373721c7`.
The test-only capture executable runs the same lifted core, checks normal
replay metadata, and exports EEPROM at exit. It does not link the new module.
The host driver applies the equivalent edit to the identical starting image;
all 8,192 bytes must match. Only the selected record's approved field(s) and
checksum may differ. This also protects calibration, scores, headers, unused
banks and opaque bytes. Runtime sidecars and results remain under `--output`.

The generated seed has **right and left volume 32**, not 63. Each channel's
32→63 preparation is independently replayed and compared byte for byte first.
Its captured image then seeds both sides of that channel's 63→0 and 63→32
comparisons. Other fields start directly from the pinned calibrated image.
There are 16 comparisons, including these two preparations.

The module exposes only car appearance, solo/time-trial difficulty, right/left
volume (including duplicates), units, attract sound and free play. Other known
operator fields are decoded/range-checked; opaque payload bytes are retained.
Extra-mode sentinel combinations can be read but any v1 edit is rejected.

The file API requires the launcher's exclusive session lock. Inode, timestamps
and bytes are checked before replacement; this is not a filesystem CAS against
uncooperative writers. A successful commit writes a new file, fsyncs, rechecks
the source, renames atomically, fsyncs the directory, then reads/validates back.
No-op commits check the source but preserve its inode. Failed pre-rename
transactions retain the original and any temporary file for diagnosis; nothing
is deleted or silently repaired. Post-rename durability/readback failures
return IO even though the new image may already be installed. Tests inject both
kinds of failure, short writes and EINTR, and check the syscall ordering.

The module is linked into `rr_gles3` and its host test only. The product audit
checks that validation, setup and sound-oracle executables do not contain it.
