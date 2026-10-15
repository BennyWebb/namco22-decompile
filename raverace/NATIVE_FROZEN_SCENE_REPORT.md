# Revised Milestone 2 — native mesh/material renderer COMPLETE

The revised gate passes on the H700. **STOPPED at M2; M3 not started.**
The original failed design and its measurements are preserved below.

## Design and coverage

`native/assets/consolidate_materials.py` decodes actual ROM material footprints
from original source UVs and the selected snapshot palette. It trims, deduplicates
and packs ordinary RGBA rectangles with two source-aware gutter texels into a
4096² page. Native UVs include texel centres; they are independent of incumbent
cache resources. There is no runtime atlas allocator, texture array, or dependent
map/pen/palette shader. See [format and reproduction](native/assets/CONSOLIDATED_FORMAT.md).

`build_native_scene.py` retains complete original model local vertices, packet
identity and actual instance/camera/scissor state. `native_scene.c` applies Q15
row-vector transforms and near-Z=1 clipping once at load, then projects through
the GPU camera. Persistent indexed buffers group compatible page/scissor parts;
synthetic depth retains painter order, and degenerate source boundaries stay
protected. Brightness interpolation follows the source screen-linear contract.
Neither renderer input nor compiler geometry imports captured projected triangles.

| Coverage | Tick 2500 tunnel | Tick 3600 coastal |
|---|---:|---:|
| Original models | 50 | 50 |
| Mesh/view tuples | 68 | 78 |
| Static source quads | 1,633 | 1,775 |
| Original direct polygons | 9 | 9 |
| Total represented primitives | 1,642 | 1,784 |
| Prepared vertices | 9,852 | 10,701 |
| Protected source boundaries | 79 | 153 |
| Main/mirror scissor regions | 2 | 2 |
| Actual consolidated draws | 81 | 155 |

Runtime point-RAM code 5 and nonzero objectflags do not occur in either selected
scene; the compiler rejects unsupported cases rather than omitting them. General
changing point-RAM geometry remains a later milestone concern. Direct geometry
is decoded from the actual nine original DSP records, including overlay placement.
The renderer's `near_clipped` statistic counts changed polygon vertex counts,
not every quad intersecting the near plane. Geometry clipping itself is applied.

## Validation and fidelity

The independent source trace matches all 6,468 / 7,013 valid original static
vertices, UVs, brightness and projection, with zero differences. Native format,
index, original local-coordinate, instance, material, affine UV and hash checks
pass for both scenes. Unknown-version and bad-checksum negative fixtures are
rejected. Tick-2500 material validation compares 14,007,136 gutter
and footprint texels plus 5,765 independently decoded samples: zero differences.
It covers all 16 cmodes and all nine orientation nibbles present in these actual
materials (0,2,4,6,8,10,12,14,15), 23 palette groups, wrapping and flipping.
Absent orientation values are not claimed as actual-scene coverage.

The small terrain-1280/car-1945 gate passed enlarged visual inspection before
full-scene device testing: 21 differing RGB pixels, maximum channel error 10,
zero alpha changes, between independent source textures and consolidated pages.
Full-world source-texture control versus page sampling differs at 25 pixels
(maximum 11) at tick2500 and 42 pixels (maximum 85) at tick3600. Repeated H700
captures are byte deterministic. Source control uses the same genuine native
meshes and separate decoded textures; it is **not** the incumbent timing reference.

Full-world native versus the original Mali tick2500 reference changes 100,326
RGB pixels (mean absolute RGB error 0.9442), with zero alpha changes. Intended
differences are uncapped original source sampling versus history-dependent capped
bakes, and native floating projection versus legacy 1/16-pixel quantisation.
These explain widespread small sampling differences; this is not an exact-pixel
gate. Enlarged images and complete 640x480 scenes were reviewed for tunnel,
traffic, road, mirror, coastal sky, palms, buildings and overlays; placement and
layering are faithful. Tick3600's separate frozen reference was captured on host
llvmpipe with depth16, so its RGB difference also includes cross-driver effects;
the native H700 uses depth24. Synthetic depth has ample separation in both scenes.

The isolated **production HUD compositor** was run on H700 with both candidate
worlds and captured original HUD states. CPU/GPU compositions agree exactly;
all 22,867 / 22,150 opaque glyph pixels match original composition and no glyph
priority pixels change. Three world priority pixels change at tick3600, none
under a glyph. Final HUD-composed comparisons change 93,840 / 76,339 RGB pixels
(mean absolute errors 1.4925 / 1.9794), zero alpha differences. These statistics
include the world sampling changes; they do not imply HUD mismatches.
The initial HUD validation mask needed a top-down/bottom-up correction; final
logs use the corrected oracle. No production rendering code was changed.

View actual images outside Git:
`/tmp/rr-m2rev/visual-hud2500/side-by-side.png` and
`/tmp/rr-m2rev/visual-hud3600/side-by-side.png` (original left, native right).
Raw images, ROMs, meshes and pages stay outside Git.

## H700 performance acceptance

Mali-G31, GLES 3.2 v1.r20p0, maximum 8192 texture extent and 16 vertex attributes
were queried. Baseline uses GLES2 features plus verified unsigned-index support,
nearest filtering and one RGBA texture sample. Three independent matched pairs
of reference/native processes ran with five warm-up passes per mode, rotating
mode measurements and 30 samples per mode/process. No transfers ran during the
final measurements. Stock scheduling/governor stayed unchanged; clocks were not
locked. No final samples were discarded.

Completion is CPU feeding plus driver work through `glFinish`, **not exclusive
GPU time, whole-frame cadence or playable FPS**. Loading is outside timing.
The prospective tick2500 gates were <=128 draws, <=128 MiB native asset residency,
completion median <=12 ms and p95 <=14 ms, without retrospective relaxation.

| Tick2500 design | Median of run medians: CPU / submission / completion ms |
|---|---|
| Streamed incumbent replay | 15.157 / 15.326 / 28.533 |
| Original resident replay | 14.824 / 14.954 / 28.118 |
| Rejected grouped replay | 11.129 / 11.293 / 23.438 |
| Revised original-mesh native | **1.416 / 1.449 / 10.093** |

| Revised native run | Completion median | p95 | p99 / worst |
|---|---:|---:|---:|
| Tick2500 round1 | 9.644 | 10.613 | 11.624 |
| Tick2500 round2 | 10.093 | 10.513 | 10.693 |
| Tick2500 round3 | 10.118 | 10.451 | 10.685 |
| Tick3600 run1 | 9.442 | 10.554 | 10.572 |
| Tick3600 run2 | 9.317 | 10.732 | 10.821 |
| Tick3600 run3 | 9.419 | 11.305 | 11.348 |

All timings are milliseconds. The second scene's 155 draws are disclosed; the
<=128 draw gate was explicitly tick2500. Both scenes meet the timing budget.
The 81-draw result reduces 701 rejected grouped draws by 88.4%; median completion
is 56.9% below that matched rejected design. Per-run CPU/submission percentiles,
all reference modes and framebuffer hashes are in
[summary.json](benchmarks/native-frozen-revised-20261015/summary.json), with raw
text logs alongside it. This is meaningful frozen-world headroom, not a live
59.904 Hz claim. No per-frame static upload, decoding, allocation, sorting or
mesh reconstruction occurs in the measured renderer loop.

## Memory, determinism and palette feasibility

Tick2500 has 710 recipes / 641 distinct RGBA rectangles; gutter content occupies
78.49% of the 64 MiB page (52,674,208 bytes). Tick3600 occupies 92.77% of one
page. Prepared GPU VBO/EBO storage is 551,712 + 39,408 bytes at tick2500 and
599,256 + 42,804 at tick3600. The dry-run budget conservatively includes source
mesh storage and an additional 8 MiB reserve: 75,742,752 bytes (72.24 MiB).
A full mip chain reserves 98,112,372 bytes (93.57 MiB) including that reserve.
**Mips are not generated or enabled.** Future levels must independently filter
materials and regenerate gutters; whole-page mip generation is forbidden.

CPU preprocessing separately holds 30,507,008 source bytes, 52,674,208 bytes of
material staging and 67,108,864 page bytes, plus Python/transients. Timed repeat
compilation: 3.50 s, peak host RSS 273,648 KiB (267.23 MiB). These are preprocessing
costs, not target GPU residency. H700 process peaks are 216,280–216,436 KiB and
227,068–227,124 KiB respectively, **including diagnostic source-control texture
copies**, CPU memory and driver allocations. Source-control textures add about
50.42 MiB at tick2500; they are excluded from native asset residency and are
reported separately, not hidden. Loading including diagnostic resources is
746–749 ms / 769–796 ms. RSS cannot be added to GPU allocations as disjoint costs.

Material compilation repeated with identical manifest SHA256
`9d848ed006b0028260d967c4b25b302654871c14eaa7a11b5ff7592fd4fb218a`.
Scene compilation repeated with identical inputs and every byte identical.
The canonical budget corrects an earlier CPU-source accounting underestimate;
all canonical runtime payloads are byte-identical to the H700-tested payloads.
Passing an explicit black-background reference spells metadata `0.0` rather than
`0`; that is a different invocation, not nondeterminism. Same invocation is exact.

Palette identity is hashed, and all selected materials use their own actual
snapshot palette. All 23 groups share page0: replacing the whole page costs
64 MiB upload; keeping two complete variants plus buffers exceeds the 128 MiB
budget. Rectangle costs by group are in
[palette-costs.json](benchmarks/native-frozen-revised-20261015/palette-costs.json).
This exposes a concrete future update/residency problem; M2 does not implement
live palette updates, time-varying scene capture, streaming or a race protocol.

## Exit and production protection

All seven revised roadmap gates are checked against this evidence. Final device
hashes match the pre-test production executable/launcher; all five backup checks
(including controls and modes) pass. EmulationStation restored, no game remains
running. Tests used temporary standalone binaries/assets only. Shared engine and
gameplay sources are unchanged. No commits, pushes, deployment, OS/clock settings,
save/replay writes or M3 work occurred. Remaining live integration, changing
palettes/content, race residency, frame pacing and latency are outside this gate.
See [progress](NATIVE_RENDERER_PROGRESS.md) and [handover](HANDOVER.md).

---

## Historical initial design — FAILED/RETHINK

# Milestone 2 — resident frozen-scene gate, 2026-10-15

This section records the initial failed probe. The completed revision above supersedes its active status.

**Status: FAILED/RETHINK. STOPPED before the complete native object/instance
backend. Milestone 3 has not started. Production is unchanged.**

The first measured design cannot demonstrate the required performance headroom.
A standalone resident replay of the actual complete tick-2500 **world pass** is
pixel-exact on the H700, but its best tested grouped design completes in
**23.409 ms**, versus **28.494 ms** for streamed incumbent draw replay. These are
medians of three run medians, not exclusive GPU times or playable FPS. The
16.693 ms whole-picture deadline is already exceeded with **no simulation,
ROM decode, texture updates, model transforms, near clipping, HUD or audio** in
the timed loop. Stop under roadmap section 2.8 and Milestone 2's redesign gate.
This rejects the measured ordinary-2D resident draw design, not all possible
native renderers. It does not claim a completed reusable-model renderer failed.

## Prior gate, scope and implementation

Starting HEAD `237641b` (`Native Renderer Milestone 1 Complete`), clean repository.
Roadmap, progress and handover read; all five Milestone 1 gates checked. Fresh
host inspection of the final visible pack passes; fresh real-ROM validation
reproduces 2,421 models and all **6,524,539 texels in 670 captured resources**, zero
RGBA differences. Final asset manifest SHA256 remains
`954b624365e8a80773fa9e4de20a6d868260dff96392d786cfa5111752dd97eb`.

The bounded first increment was a **ceiling/feasibility probe**: use all captured
resources and already prepared triangles to measure whether simply making them
persistent and grouping ordinary RGBA materials leaves enough time for the real
native object path. This deliberately excludes work that a reusable local-space
mesh path still has to perform. Its failure is reason to rethink before spending
time reproducing every world transform and dynamic exception.

Delivered:

- `native/render/frozen.c`: standalone GLES2 diagnostic, no emulator, gameplay,
  scene decoder or ROM decoder linked. Requests GLES2, queries actual features,
  uses ordinary 2D RGBA textures, persistent vertex/index buffers and a simple
  single-sampler shader. No arrays, atlas remapping, dependent map/pen/palette
  shader or changes to production code.
- `native/tools/export_frozen_instances.py`: private CLI diagnostic reads an
  actual saved board snapshot through the incumbent list/geometry stage. Exports
  object code, source packet address, Q15 combined model/view matrix,
  translation, viewport centre, lens mantissa/shift, raw projected corners,
  brightness and material metadata; validates those against converted meshes.
- `native/tools/analyse_frozen.py`: parses run timings, requires all nine final
  device framebuffers to equal the captured reference, writes metrics and PNG
  comparisons. Numeric logs and summaries under `benchmarks/native-frozen-20261015/`.

**Important implementation limit:** the timed probe consumes the incumbent's
frozen projected triangles/UVQ, not the original reusable model buffers. Camera,
instance and mesh attributes are recovered and independently validated, but are
**not yet fed to a new local-space vertex shader**. Persistent projected geometry
is a useful fixed-scene feasibility measurement, not a substitute for the
roadmap's genuinely native geometry path. No complete native asset importer,
model/instance renderer, scene protocol or live integration is claimed.

## Actual inputs and source validation

Preserved complete scene: host `/tmp/rr-atlas/frozen`; device
`/userdata/system/rave-racer-world-atlas-20261014/frozen-20261014-160318-25197`.
It contains 1,280 original draw commands, 7,998 vertices and 670 RGBA resources.
Captured draw bytes SHA256
`f56a4fe372cec1176355d0e92eeac792f6e7f8d203efe24c6c9ab18c5688bd50`.
Frozen endpoint JSON equals the actual tick-2500 replay snapshot JSON in
`/tmp/rr-m1-durable-trace/capture` byte-for-data. The replay and original sidecars
were not modified or replaced.

The source inspector checks **1,633 emitted source quads, 6,468 valid raw source
vertices, 50 converted ROM models and 68 distinct object/transform/view tuples**
with **zero discrepancies**. It checks original packet colour/flags/bank/cmode,
source UV/brightness and integer Q15 transform/projection. No code-5 point-RAM
quads occur in that inspected display list. This does **not** prove direct FIFO
absence: direct polygons/HUD are outside that inspector. The frozen draw replay
retains every captured world triangle regardless of source classification.
Source validation is archived in
[the source report](benchmarks/native-frozen-20261015/source-validation.json).

## Rendering contract and coverage

Input vertices retain captured screen X/Y, synthetic priority depth, per-vertex
RGBA and perspective UVQ. The projection maps 640x480 screen coordinates to clip
space with W=1, matching the incumbent. This preserves perspective texture
sampling through `UVQ.xy / UVQ.z`, screen-linear brightness, depth/priority alpha
and current scissor conventions. It deliberately does not replace synthetic
priority with eye-distance Z. Original RGBA textures and sampling remain
nearest/clamp, including actual allocation guard texels and bake tiers.

Modes:

0. Streamed incumbent draw replay: original ordered draws, full vertex upload
   once per sample. This is a same-context frozen submission reference, **not
   a running production executable or its complete rendering cost**.
1. Resident original order: original draws, persistent VBO, no uploads.
2. Resident grouped: same geometry and textures, persistent VBO/EBO, grouped by
   material and identical scissor, 701 indexed draws. Degenerate original draws
   keep their boundaries, and equal-resource order remains stable.

Modes 1/2 have no allocation, decode, triangulation, buffer upload, texture
creation/update/deletion or sorting in steady state. Mode 0 deliberately streams
319,920 vertex bytes. Final code uses a **separate streamed VBO**, so baseline
uploads never replace the resident VBO's storage. Grouping is enabled only for
opaque priority mode and after a synthetic-depth separation check: 1,328 unique
Z values, minimum 35.894 16-bit-depth LSBs. Future transparent scenes are rejected
rather than reordered without proof. No texture resource is dropped.

| Coverage | Result / limitation |
|---|---|
| Road/tunnel/terrain, cars/traffic and scenery | Captured world content retained; Mali reference RGBA exactly matches all three modes in all three runs. |
| Camera, mirror view and scissor | Captured placement and mirror/scissor results retained exactly; original instance metadata validated separately. |
| Lighting, material and UV associations | Actual resolved texture bytes, vertex colours/brightness, UVQ and priority alpha retained exactly; no new live lighting evaluator. |
| Near/guard/screen clipping | Captured clipped triangles retained; original raw mesh projections independently checked. A new native clip path is not implemented. |
| Synthetic depth, painter ties, degenerates | Original depth retained, separation gate passes, degenerate draw boundaries protected; exact resulting framebuffer comparison passes. |
| Direct/runtime geometry | Captured world triangles retained if present; no reusable source bridge or revision handling claimed. |
| HUD and world/HUD interactions | Exact world alpha metadata retained; HUD compositor and full final-game composition are **not** executed by this probe. Original `final.ppm` remains the full-frame reference. |
| Other scenes / sky / palette animation | Not tested; after initial scene failed the performance gate, additional captures and renderer integration were not entered. |

Comparisons: `/tmp/rr-m2/analysis-final/{incumbent-world,resident-world,side-by-side}.png`.
The 1280x480 side-by-side is incumbent left / resident grouped right, each 640x480.
Preview alpha is forced opaque because framebuffer alpha carries compositor
priority metadata; validation uses the original RGBA bytes without alteration.
These PNGs contain game assets, so they remain outside Git.

## Real H700 results

Mali-G31, OpenGL ES **3.2 v1.r20p0-01rel0.8ec337de4ddab3365a32cff58063a9da**,
16 fragment units, maximum texture size 8192. GLES2 requested; unsigned-int
indices are accepted only after checking core GLES3 or OES support. Actual FBO
uses captured 24-bit depth; completeness and GL errors checked. No MSAA,
filtering enhancements, refresh policy or driver/clock settings were changed.
Governor reads `schedutil`, frequency read before each final run was 1,200,000
kHz; frequency was not locked. No exclusive GPU timer/utilisation claim.

Each fresh process loads once, verifies three framebuffer modes, completes five
warm-up triplets, then runs **30 rotating mode triplets**. Each sample ends with
`glFinish` for diagnostic completion; readbacks occur only outside timing.
Three complete final processes, no discarded final samples and no transfers
while measuring. EmulationStation stopped only during diagnostic ownership,
with trap-based restoration. No production game ran concurrently.

| Final run median completion (ms) | Streamed original | Resident original | Resident grouped |
|---|---:|---:|---:|
| 1 | 28.532 | 28.125 | 23.909 |
| 2 | 28.494 | 28.089 | 23.409 |
| 3 | 28.452 | 28.107 | 23.287 |
| Median of run medians | **28.494** | **28.107** | **23.409** |

| Median of run medians | Streamed original | Resident original | Resident grouped |
|---|---:|---:|---:|
| Thread CPU (ms) | 15.207 | 14.815 | 11.132 |
| Submission wall (ms) | 15.286 | 14.894 | 11.467 |
| Completion wall (ms) | 28.494 | 28.107 | 23.409 |
| Draws | 1,280 | 1,280 | 701 |

Draws fall 45.23%; completion improves 17.85%, but grouped completion exceeds
16.693 ms by 40.23% before the excluded work. Completion includes feeding CPU,
driver work, scheduling and completion wait; subtraction does not isolate GPU
shader cost. These figures cannot be converted into a claimed live-game FPS.
See [all final samples and p95/max metrics](benchmarks/native-frozen-20261015/summary.json).

Resource accounting: 26,098,156 texture bytes, 319,920 resident vertex bytes,
31,992 resident index bytes; baseline has a separate streamed VBO of 319,920 B.
Offscreen RGBA target is 1,228,800 B; depth allocation and SDL/window/driver
storage are additional and implementation-dependent. Source RGBA CPU shadows
remain allocated by this diagnostic. Final process peak RSS is 141,008–141,108
KiB (~137.7–137.8 MiB), not disjoint GPU memory and not a live residency budget.
Loading/setup is explicitly logged separately (about three seconds on H700),
never part of steady-state timing. Warm resources do not imply every driver
allocation is known.

Earlier `/tmp/rr-m2/device` runs shared the baseline and resident VBO storage and
measured 20.132 ms grouped. They are preserved exploratory evidence, superseded
by the final separate-buffer runs `/tmp/rr-m2/device-final`. Do not mix their
absolute numbers. The final probe intentionally removes this storage confound.
No claim is made that it alone explains the timing difference.

Host llvmpipe OpenGL ES 3.2 Mesa 26.0.8 reproduces its own original-order output
exactly in both resident modes, but differs from the captured Mali reference at
941 pixels. Cross-driver raster/sampling equivalence is not established. This
does not affect the exact Mali-to-Mali gate; no corrective shader fudge was used.

## Focused redesign proposal — not implemented

Keeping hundreds of independent RGBA materials resident removes uploads but
leaves too much per-draw feeding/completion cost. The next bounded proposal is
**an offline material/mesh consolidation design**: derive materials from original
source UVs at compile time, trim their source footprints, define fresh native UVs
and gutters/mip seams, and coalesce spatially compatible opaque mesh parts into
far fewer material groups while retaining explicit priority/scissor boundaries.
First require a measured draw/material reduction budget and small visual test.

This would be a revised Milestone 2, requiring separate approval. It must not
reintroduce the rejected runtime exact-resource atlas/array allocator, history-
dependent bake-tier remapping or dependent pen/map/palette fragment shader.
The distinction is an explicitly new offline native UV/material contract under
visual-fidelity review, not relocation of incumbent cache resources with a claim
of exact sampling. If that distinction or the budget cannot be justified, reject
the proposal rather than relabelling an old failed experiment. No page packer,
new sampling shader, extra scenes or revised renderer was implemented here.

## Reproduction and stop state

Commands and input requirements: [README](native/render/README.md). Private host
binaries `/tmp/rr-m2/frozen-{host,arm}`; device `/tmp/rr-m2-frozen-final`;
final device logs/captures `/tmp/rr-m2-results-final/run{1,2,3}`. Source inspection
outputs `/tmp/rr-m2/instances-final`, analysis `/tmp/rr-m2/analysis-final`.
Existing original captures and recordings remain preserved.

Production executable before/after matches
`6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`;
launcher `5358c57fe1a1d73b717c05a32e10b1683613db3771fc78b1ebad4ec6c22e3fad`,
controller `355aef433cc4a6d11381b1b218319e35978f5b23f1396a0ccfef0049a94e3284`.
Backup checks pass, sole Rave Racer Ports launcher preserved, GPU HUD `default`,
replay `off`. No gameplay/physics/audio/simulation/shared engine source changed.
Only private diagnostics were copied to device `/tmp`; no playable deployment,
commit, push, save deletion or OS/clock change occurred. Final frontend/process
verification is recorded in the progress report.

**Milestone 2 did not pass its overall exit gate. Stop with FAILED/RETHINK;
complete object-space mesh rendering and broader scene coverage are not claimed.
Milestone 3 requires explicit user approval and is not entered.**
