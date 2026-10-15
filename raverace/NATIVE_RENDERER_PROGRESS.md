# Native renderer progress

**Current: revised Milestone 2 COMPLETE, 2026-10-15. STOPPED at its exit gate.**
Milestone 1 remains complete. The current gate and evidence follow.

## Revised Milestone 2 — COMPLETE (2026-10-15)

Milestone 1's completed exit gate was confirmed before this work. The revised
M2 exit gate now passes. **STOPPED: Milestone 3 is not started or authorised.**
No commit, push, deployment or production playable change occurred.

The offline compiler now builds source-derived RGBA pages with fresh native UVs,
source-aware gutters and snapshot palette provenance. The standalone renderer
loads original local-space meshes and actual Q15 instances, prepares clipping
once, projects through the native GPU camera and keeps its buffers resident.
The previous projected-triangle probe remains historical failed evidence below.

Acceptance evidence is in [the revised report](NATIVE_FROZEN_SCENE_REPORT.md)
and [numeric records](benchmarks/native-frozen-revised-20261015/summary.json).
Tick 2500 uses **81 draws**, one 4096² RGBA page, and **72.24 MiB budgeted native
asset residency** including an 8 MiB reserve. Three matched H700 processes give
completion medians **9.644 / 10.093 / 10.118 ms**, with p95
**10.613 / 10.513 / 10.451 ms**: all pass the prospective 12/14 ms gate.
The additional actual tick-3600 coastal scene gives **9.442 / 9.317 / 9.419 ms**,
p95 at most **11.306 ms**, with 155 draws (the 128-draw target applies to tick
2500). These are frozen-world completion measurements, not playable FPS.

All 1,642 / 1,784 source primitives are represented, including nine direct
polygons in each scene, both main/mirror views and priority/scissor boundaries.
Independent source sampling, source mesh/instance comparisons, checksums and
fresh UV validations pass. Full frames were visually reviewed. The isolated
production HUD compositor preserves all **22,867 / 22,150 opaque glyph pixels**
exactly; CPU/GPU HUD compositions agree. Texture sampling is intentionally
visually faithful rather than incumbent-cache pixel exact.

Final device checks: executable SHA256
`6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`,
launcher `5358c57fe1a1d73b717c05a32e10b1683613db3771fc78b1ebad4ec6c22e3fad`;
all five backup checks pass, including controls and mode files. EmulationStation
restored (PID 11053); no game left running. Shared gameplay/engine sources have
no changes. Diagnostic device files are in `/tmp` only. Existing saves/replays
were read only, never used as diagnostic outputs.

Live palette updates, changing scene commands, full-race residency, frame pacing,
input/audio latency and playable integration remain unproved and outside M2.
Next work requires separate explicit Milestone 3 authorisation.

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

## Milestone 2 — Standalone native frozen-scene renderer (2026-10-15)

**Authorised: Milestone 2 only. Status: FAILED/RETHINK. STOPPED at the measured
resident-design gate. No Milestone 3 work.**

Read [full report](NATIVE_FROZEN_SCENE_REPORT.md),
[build/reproduction commands](native/render/README.md),
[final metrics](benchmarks/native-frozen-20261015/summary.json) and
[source validation](benchmarks/native-frozen-20261015/source-validation.json).

Prior gate: all five M1 gates confirmed checked. Fresh final asset inspection and
real-ROM/captured-resource validation pass (2,421 models, 670 resources,
6,524,539 texels, zero differences). Starting HEAD `237641b`, clean tree.

Delivered files:

- `native/render/frozen.c`, `native/render/README.md`: isolated GLES2 resident
  frozen-world ceiling probe, persistent buffers/RGBA textures, original and
  grouped/indexed draw modes, correctness checks and timings; no emulator linked.
- `native/tools/export_frozen_instances.py`: actual snapshot camera/instance/
  packet/material export and converted-mesh coordinate validation.
- `native/tools/analyse_frozen.py`: repeated-run analysis and comparison PNGs.
- `benchmarks/native-frozen-20261015/{summary.json,source-validation.json,mali-run1.txt,mali-run2.txt,mali-run3.txt}`:
  numeric evidence only; no game assets. Original device/run logs retained too.
- `NATIVE_FROZEN_SCENE_REPORT.md`; roadmap/progress/handover updated.

Accepted final evidence:

| Item | Result |
|---|---|
| Original ROM mesh correspondence | 1,633 source quads / 6,468 valid raw vertices / 50 ROM models / 68 object-view tuples; zero discrepancies. Frozen capture and source snapshot have equal endpoint state. |
| Full captured world pixels | All three modes in all three final H700 runs equal preserved Mali RGBA reference exactly. No polygons/resources omitted. |
| Resource reuse | 670 2D RGBA resources, 7,998 vertices; 1,280 original draws -> 701 grouped draws. Candidate modes never upload/rebuild in timed loops. Baseline has a separate streamed VBO. |
| Final H700 medians | Streamed original / resident original / resident grouped completion: 28.494 / 28.107 / 23.409 ms. Grouped CPU 11.132 ms; submission 11.467 ms. Three processes, 30 rotating triplets each; no final sample discarded. |
| Native host | GLES build/run passes; candidate modes equal their llvmpipe original reference exactly. Host original differs from Mali capture at 941 pixels; portability equivalence not claimed. |
| Invalid scene inputs | Short command header, malformed triangle count and empty command stream all reject with exit 2; raw logs `/tmp/rr-m2/negative-tests`. |
| Overall exit | **Not passed.** Projected resident probe already lacks 16.693 ms headroom. Full local-space model/instance shader, native clipping/lighting and HUD integration were not implemented. No claim of a completed genuine native object renderer. |

The measured first design is rejected, not the whole native-renderer objective.
Per roadmap section 2.8 and the M2 redesign gate, stop before extending it.
Proposed next bounded investigation: offline native mesh/material consolidation
with fresh source UV/gutter/priority rules and an explicit draw-reduction budget;
not a return to the rejected runtime cache atlas/array or dependent-lookup shader.
This proposal is **not implemented** and needs separate authorisation as a revised
Milestone 2. Full time-varying scene protocol/replay belongs to Milestone 3 and
has not started.

Local assets/captures: `/tmp/rr-m2/instances-final`, `/tmp/rr-m2/device-final`,
`/tmp/rr-m2/analysis-final` (side-by-side PNG: incumbent left, resident right).
Private host binaries `/tmp/rr-m2/frozen-host` SHA256
`a7580666cc6420327f6fa4ee9c42891019e2399a761786c7a3d71565a0dbf8a2`,
`/tmp/rr-m2/frozen-arm` SHA256
`885b3ba5d4fce866cf91570db549a80f59c07655d8465642cb78d5305e1c1196`.
Device binary `/tmp/rr-m2-frozen-final`, results `/tmp/rr-m2-results-final`.
Earlier shared-VBO exploratory results are preserved under `/tmp/rr-m2/device`;
only the separate-buffer final results support the headline numbers.
No new racing FPS, refresh/swap policy, latency or audio result is claimed.

Final device checks after all tests: production executable, launcher and
controller hashes match M1/hand-over references; all five backup checksum entries
pass; exactly one Rave Racer Ports launcher; frontend restored (PID **5664**), no
rr_gles2 game process, GPU HUD `default`, replay `off`. No production deployment,
shared gameplay/simulation/audio/renderer source edits, commits, push, settings
changes or deletion of user data. No credentials stored. No transfers during runs.

**STOPPED. Milestone 2 requires an approved redesign to resume; Milestone 3
requires explicit user authorisation and the earlier gate must be resolved.**

## Milestone 2 focused revision authorised (2026-10-15)

The user requested amendment of M2 to carry out the focused offline native
mesh/material consolidation proposal. The roadmap now defines that authorised
revision: actual-source footprint audit and deterministic RGBA compiler, fresh
native UV/seam/gutter rules, small terrain/car fidelity gate, genuine reusable
mesh/instance backend, complete frozen scenes and repeated H700 headroom tests.
Initial working budgets: <=128 world draws, <=128 MiB native asset residency;
world completion median <=12 ms / p95 <=14 ms on tick 2500. These are prospective
acceptance targets, not measured results or a claim that the design is feasible.

All revised gates are unchecked. This amendment changed documentation only; the
initial 23.409 ms failure and its captures/source remain preserved. Implementation
has not resumed as part of this amendment. Follow the revised roadmap's bounded
sequence and stop at its first failed gate or completion. No Milestone 3, commit,
push, deployment or production change is authorised.
