# Knulli H700 build and operation

For a local source build and clean-device setup, follow [INSTALL.md](INSTALL.md).
The asset-kit commands below are developer validation tools, not installation packaging.

Build from `raverace/`. User ROMs are required; see [INSTALL.md](INSTALL.md)
for the package and source content disclosure.
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

For installation, updates, controls, logs and uninstall, use [INSTALL.md](INSTALL.md).
The locally built package contains one Ports entry and a self-contained `ports/rave-racer/`
folder; ROMs, generated cache, EEPROM, mapping and logs all stay inside it.
Normal play uses summary-only logging. Full telemetry is explicit.
The old deploy.py/flat-package workflow is developer history, not the current
installation procedure. No ROM-derived asset kit belongs in an installation ZIP.

The full host regression suite needs only a fresh clone and the ROMs (plus the
host build dependencies above):

```sh
python3 raverace/gles3/tests/suite.py --roms /absolute/path/to/extracted
```

Run that command from the repository root. It configures/builds a fresh host
build and ROM asset cache, generates the factory-default EEPROM by booting from
empty EEPROM, verifies its SHA256, and runs reference (6,006 ticks), fixture
(1,700), full-race (13,937), attract, provenance/boot oracle, strict unsupported, sound oracle,
record/replay and CTest gates. No stored EEPROM is read or used as a fallback.
Controller input recordings and their text metadata are stored in Git under
`gles3/tests/recordings/`; checkpoints, semantic packets and images retain
hash-only fixtures. `GLES3_TEST_RECORDINGS` or `--recordings DIR` overrides the
inputs; SHA256 checks remain mandatory.
Individual gates use `gles3/tests/validate.py CASE --build BUILD --roms ROMS
--cache CACHE --output NEW_DIRECTORY`; cases are reference, fixture, attract,
provenance, unsupported, sound and full-race. Generated EEPROM/runtime sidecars stay local.
Device smoke requires `GLES3_TEST_CONTROLLER_CONFIG` pointing at that saved mapping.

For local validation, build rr_validation_asset_kit and pass the build directory's
validation-assets cache to the host gates. rr_asset_kit and packaged apps contain
only product shaders. Run gles3/tests/audit_product.py --build BUILD (add
--nm aarch64-linux-gnu-nm for an ARM build) to prove validation/product separation.

The full-race gate generates its calibrated EEPROM from blank bytes using the
ROM's TEST/SERVICE adjustment (1,600 ticks), then cold-starts the replay. Its
SHA256 is pinned in recordings.json; it never reads a stored seed. It checks
race/results/bonus coverage, 141 checkpoint records, complete packet/image hash
manifests and translated/interpreter sound on every tick. The older reference
and fixture retain their 600-tick blank-boot seed for historical regression;
their deterministic results alone do not establish full-race coverage.
