# Native renderer progress

## Milestone 1 — Recover and package reusable game assets

**Authorised:** Milestone 1 only. **Status: COMPLETE.**
All exit gates satisfied, including terrain identity and production verification.
See [investigation](NATIVE_ASSET_INVESTIGATION.md) and [format](native/assets/FORMAT.md).
No Milestone 2 work, commits, push or deployment occurred.

Delivered source:

- `native/assets/asset_tool.py`: deterministic converter/inspector, hashed versioned
  manifest, ROM provenance, original vertex/index/lighting/material attributes,
  source pen decode and actual-palette RGBA bakes.
- `native/assets/FORMAT.md`: format, conventions, diagnostics and commands.
- `native/tests/validate_assets.py`: real-ROM and captured-RGBA validation,
  topology/index/UV/material checks and malformed-input gates.
- `native/tools/build_source_trace.py`: private host diagnostic relink; shared
  emulator sources and normal build objects remain intact.
- `native/tools/inspect_scene_geometry.c`: CLI inspection through the incumbent
  scene/geometry stage, without a graphics renderer.
- `native/tests/validate_terrain_identity.py`: course-ROM tag, executed DSP
  mapping, slave primitive and converted mesh comparisons.
- `.gitignore`: ignores repository-local generated asset outputs.
- Investigation and this report. The user-provided roadmap is untouched.

Acceptance evidence:

| Gate | Result |
|---|---|
| Deterministic conversion | PASS: two actual-ROM packs, same manifest and all payload hashes. Manifest SHA256 `d4dd84a468065e371b36d63dcfd0503503ee4a1ff86c7baf23f7f296578815eb`. |
| Representative geometry | PASS: course 6 / segment 10 / source tag 1211 -> DSP PC 0x4425 ADDK 69 -> terrain object 1280. 119 immutable quads; incumbent's 50 emitted quads / 182 valid raw vertices match projection, depth, UV, brightness and materials. Additional actual racing-car object 1945 is visually confirmed by the offline asset preview; the earlier label for object 1765 was corrected below. |
| Complete inventory/topology | PASS: 2,421 models / 97,116 quads / 388,464 vertices, zero exclusions; indices, dimensions, coordinate conventions and source UV/material associations checked. |
| Material conversion | PASS: 59 terrain/car representative RGBA/pen recipes. Independent incumbent capture oracle: 670 resources / 6,524,539 texels / zero differences. |
| Invalid data | PASS: invalid addresses/list pointers/packets, short ROMs, unsupported versions and corrupted buffers rejected. |
| Static/dynamic boundary | ROM geometry vs instance transforms, live palette/solid overrides, point RAM code 5 and direct FIFO contracts documented with worked examples. |
| Residency proposal | 11.11 MiB geometry, 2.02 GiB full texture inventory, 24.89 MiB frozen scene. Proposed bounded 128 MiB asset residency with moving-track union/palette/driver unknowns stated. |
| Emulator build/tests | Native fresh headless build and deterministic/input/environment/CLI regression pass; existing H700 playable build passes. Traced vs uninstrumented tick-2500 endpoint JSON and polygon RAM match exactly, both replay checks pass. |
| Production | Read-only authenticated verification: executable matches handover, frontend running, no game, one launcher, backup checksums pass. No remote writes or frontend changes. |

Generated packs `/tmp/rr-m1-final-{a,b}` and captures are outside Git. No ROM,
converted game asset or recording was added to Git. No shared runtime source,
physics/audio/controller/config/launcher/backup/replay change was made.

Reproduce from repository root; use fresh output directories:

```sh
cmake -S raverace -B /tmp/rr-m1-headless -DRR_HEADLESS_ONLY=ON -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/rr-m1-headless -j4
python3 raverace/tools/test_headless.py /tmp/rr-m1-headless/rr_headless raverace/extracted
cmake --build raverace/build/playable-knulli -j4
python3 raverace/native/assets/asset_tool.py convert raverace/extracted /tmp/rr-m1-new-a --palette /tmp/rr-native-texture/palette.rgb --bake-models 1280,1765
python3 raverace/native/assets/asset_tool.py convert raverace/extracted /tmp/rr-m1-new-b --palette /tmp/rr-native-texture/palette.rgb --bake-models 1280,1765
sha256sum /tmp/rr-m1-new-a/manifest.json /tmp/rr-m1-new-b/manifest.json
python3 raverace/native/assets/asset_tool.py inspect /tmp/rr-m1-new-a
python3 raverace/native/tests/validate_assets.py raverace/extracted /tmp/rr-m1-new-a /tmp/rr-atlas/frozen /tmp/rr-native-texture/palette.rgb
python3 raverace/native/tools/build_source_trace.py /tmp/rr-m1-headless /tmp/rr-m1-new-trace
/tmp/rr-m1-new-trace/rr_asset_trace raverace/extracted --frames 2500 --replay-input /tmp/race-20261013-122242-25136.rec > /tmp/rr-m1-new-trace/capture/state.json 2> /tmp/rr-m1-new-trace/capture/run.log
cc -O2 -fwrapv -Iengine raverace/native/tools/inspect_scene_geometry.c -o /tmp/rr-m1-new-trace/inspect_geometry -lm
python3 - <<'PY'
from pathlib import Path
import importlib.util
spec=importlib.util.spec_from_file_location('asset_tool','raverace/native/assets/asset_tool.py')
t=importlib.util.module_from_spec(spec);spec.loader.exec_module(t)
t.ROM(Path('raverace/extracted')).words.astype('<i4').tofile('/tmp/rr-m1-new-trace/pointrom.i32')
PY
/tmp/rr-m1-new-trace/inspect_geometry /tmp/rr-m1-new-trace/pointrom.i32 /tmp/rr-m1-new-trace/capture/poly.i32 /tmp/rr-m1-new-trace/capture/pointram.i32 1280 > /tmp/rr-m1-new-trace/geometry1280.jsonl
python3 raverace/native/tests/validate_terrain_identity.py /tmp/rr-m1-new-trace/capture /tmp/rr-m1-new-trace/geometry1280.jsonl /tmp/rr-m1-new-a
```

These use preserved host copies of the original recording and sidecars; replay
and frozen data availability is required. The private capture binary should run
once per output directory (exclusive creation). It is an asset-investigation
instrument, not a scene protocol, renderer or performance benchmark.

Raw evidence:
[terrain identity](/tmp/rr-m1-terrain-validation.json),
[final asset validation](/tmp/rr-m1-final-validation.log),
[DSP execution log](/tmp/rr-m1-trace/verified/run.log),
[incumbent geometry](/tmp/rr-m1-trace/geometry1280.jsonl),
[native build](/tmp/rr-m1-build.log),
[native regression](/tmp/rr-m1-headless-test.log),
[H700 build](/tmp/rr-m1-arm-build.log).
The earlier `/tmp/rr-m1-inventory-summary.json` describes the initial eight-bake
pack; its all-model counts/budget estimates remain valid, but final pack size is
55,679,799 B with 59 recipes. Earlier candidate-69 packs are preserved separately.

## Production verification retained from authenticated retry

Production executable SHA256:
`6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`.
Launcher SHA256 `5358c57fe1a1d73b717c05a32e10b1683613db3771fc78b1ebad4ec6c22e3fad`;
one Rave Racer Ports launcher, EmulationStation PID 3154, no rr_gles2 process.
GPU HUD `default`, replay `off`. Controller config SHA256
`355aef433cc4a6d11381b1b218319e35978f5b23f1396a0ccfef0049a94e3284`, matching backup.
All five pre-consolidated-GPU-HUD backup SHA256SUMS entries pass. Reference replay
and five sidecars present, replay hash
`d76551345170fbbe0b4700dfe91cebf36d2c1a408dda69de6c2b043a95043216`.
Initial network/authentication failures were resolved; no device writes occurred.
Subsequent terrain closure used host diagnostics only. Authentication is not
stored in repository files. No new racing performance measurement was required.

Remaining risks: complete moving-track/material union and palette variants,
dynamic scene coverage, native lighting/priority/filtering and actual Mali
residency/performance remain unverified future work, not Milestone 1 claims.

**STOPPED. Next milestone 2 requires explicit user authorisation.**

Reproduction-tool verification: `build_source_trace.py` was built and run from a
fresh `/tmp/rr-m1-durable-trace` directory. Its terrain validation passes with
identical counts, and endpoint JSON/polygon RAM match the uninstrumented run.
Raw [repeat evidence](/tmp/rr-m1-durable-trace/validation.json).

## User-requested offline asset previews

`native/tools/preview_assets.py` produces local PNGs of extracted meshes and
texture sheets using a small CPU orthographic preview (no GPU scene renderer,
no running simulation). Outputs `/tmp/rr-m1-visible-preview/`: terrain object
1280, racing car body 1945, and 144 baked material images/contact sheet.
These views omit game lighting/fog and do not establish scene fidelity.

Correction: the original car-select source table contains tag 1765; that tag
was prematurely labelled as direct ROM object 1765. The preview revealed that
object 1765 is a flat textured panel. It remains a valid decoded asset, but
is not the car representative. Object 1945 is an actual emitted tick-2500
primitive and the preview clearly shows its car-body geometry/textures.
Terrain 1280 + car 1945 are now the representative visible set.

Reproduction:

```sh
python3 raverace/native/assets/asset_tool.py convert raverace/extracted /tmp/rr-m1-new-visible --palette /tmp/rr-native-texture/palette.rgb --bake-models 1280,1945
python3 raverace/native/tools/preview_assets.py /tmp/rr-m1-new-visible /tmp/rr-m1-new-preview
```

144 material recipes validate; all 670 captured renderer resources still match
6,524,539 RGBA texels exactly. Two fresh visible packs match manifest hash
`954b624365e8a80773fa9e4de20a6d868260dff96392d786cfa5111752dd97eb`.
Raw evidence `/tmp/rr-m1-visible-validation.log`; packs
`/tmp/rr-m1-visible-pack` and `/tmp/rr-m1-visible-pack-repeat`.
Previous packs remain preserved; the terrain trace evidence is unchanged.
Milestone 1 remains complete. No Milestone 2 work, deployment or commits.
