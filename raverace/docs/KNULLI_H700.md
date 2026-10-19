# Knulli H700 build and operation

Build from `raverace/`. User ROMs are required; no ROMs or asset payloads ship in Git.
Host dependencies: GCC, CMake, Python3/NumPy, SDL2, GLES3/EGL, zlib and OpenSSL.

```sh
cmake --preset gles3-host -DRR_ROM_DIR=/absolute/path/to/extracted
cmake --build --preset gles3-host
cmake --preset knulli-h700 -DRR_ROM_DIR=/absolute/path/to/extracted
cmake --build --preset knulli-h700
cmake --build build/knulli-h700 --target rr_asset_kit
python3 gles3/tools/package.py build/knulli-h700 build/knulli-h700/rom-assets/asset-kit /tmp/rave-gles3-app
```

Set `KNULLI_HOST` when the SDK is outside `$HOME/dev/knulli-scarab/output/h700/host`.
The desktop target remains `cmake --preset desktop; cmake --build --preset desktop --target rr`.
Generated DSP/sound code and local ROM metadata live only in the build directory.
The first app launch decodes pens and meshes from the user's original ROMs into
`app/cache/<key>/`; subsequent launches verify ROM, recipe, compiler and cache hashes.
The layout is generated from the whole ROM catalogue; recordings never build assets.

After review, installation is an explicit action: run platform/knulli-h700/deploy.py
on the device with the local package and `--install`. Validation never installs it.
It creates **Rave Racer (GLES3)** and **Rave Racer (GLES3, record)** Ports entries.
Production and its Ports entry are independent. Existing save migration paths and
source-of-truth hashes are recorded in HISTORY.md; back up/copy the chosen EEPROM
before switching to the new save directory, `/userdata/saves/rave-racer-gles3/`.
Copy the chosen `controller-input.cfg` there too; the launcher passes it to SDL.

Controls follow the saved controller configuration; quit with START+SELECT.
For a slowdown recording, choose the record entry, play through the event, then quit.
`app/recordings/<session>/session.rec` has input, metadata, EEPROM, end and checkpoint
sidecars, plus complete gzip telemetry outside the eight-session rolling log purge.
Normal logs and summaries are in `app/sessions/` and `latest-summary.{txt,json}`.
Per-picture batching split counters are diagnostic and never change geometry/order.

`device-profile.json` owns the panel rate, queue depth and CPU clock. CPU policy is
session-only; clocks.py records/restores originals, with an independent watchdog.py.
GPU policy remains stock. Unsupported content logs `GLES3 UNSUPPORTED`, writes the
same top summary line and exits86. The supervisor/watchdog restore clocks on failure.

Before later installation, retain the previous app. The generated restore.sh is
one-step rollback while idle; saves and retained recordings remain separate.
For a first installation it disables the two new entries, leaving the prior
installed entries available and retaining the new app's files for inspection.
This cleanup has not replaced the installed build. See CLEANUP_REPORT.md for gates.

Host acceptance uses `gles3/tests/validate.py CASE --build BUILD --roms ROMS
--cache CACHE --recordings LOCAL_RECORDINGS --output NEW_DIRECTORY`, for each
CASE of reference, cone, attract, provenance, unsupported and sound. Then run
`gles3/tests/provenance.py OUTPUT/provenance --roms ROMS --cache CACHE`.
`ctest --test-dir BUILD --output-on-failure` checks the renderer components.
Tests reject mismatched recording/EEPROM hashes; fixtures contain hashes only.
Device smoke requires `GLES3_TEST_CONTROLLER_CONFIG` pointing at that saved mapping.
