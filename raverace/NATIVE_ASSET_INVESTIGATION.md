# Milestone 1 asset investigation — 2026-10-14

Status: **COMPLETE**, including the identified course/terrain representative.
Starting HEAD `a8bab73219e522da82612edfdd6ae768485f0f06`; starting Git state:
only the user-provided untracked `NATIVE_RENDERER_ROADMAP.md`. It was left intact.
Roadmap and handover, all listed architecture/geometry/replay/HUD/failed texture
reports, active scene/DSP/presentation/geometry/texture paths and game ROM table
were inspected. No production source was changed. Root licence is MIT; no new
runtime dependencies beyond the existing NumPy-based tooling approach.

## Recovered hierarchy and identity

Twelve 512 KiB POT chips concatenate into low/middle/upper byte planes, producing
2,097,152 signed 24-bit point words (6 MiB on disk, 8 MiB in current int32 RAM).
The first ROM word is 2490; the first object list pointer begins at 2490.
For this set, object IDs 0x45..2489 all decode: **2,421 models, 97,116 quads,
388,464 vertices, 582,696 indices**, zero excluded packets. There are 2,421
unique list pointers; this does not claim all chunk/vertex content is unique.
All inventoried quads carry Gouraud format bit 0x40; zero models contain normal
packets. Normals support is retained for format completeness, not claimed tested
with actual Rave normal assets.

`eng_walk_list` primitive length 0x0d supplies code, matrix and translation.
`geo_hw_object(code)` reads `point[code]`, then a terminated chunk-pointer list;
each chunk has a <=256-word payload. Quad packets have lengths 0x17/0x18,
lighting packets 0x10/0x0d. The last-packet format bit permits early chunk exit.
Thus `(ROM source hashes, object code)` is stable geometry identity; chunk and
packet addresses retain finer provenance. Q15 model/view transforms, viewport,
reflection, clip, light, objectshift, absolute priority and cz/material state
belong to an instance or picture. Asset triangles alone cannot replace original
clipping, synthetic priority, painter ties or degenerate draw boundaries.

Eight 2 MiB CG chips contain 65,536 16x16 raw pen tiles. CCRL is a 2 MiB
little-endian tile map; CCRH is 512 KiB of packed orientation/index-extension
nibbles. The million map entries reference **58,233 distinct extended tile IDs**;
58,232 are within the CG data. The remaining reference decodes raw zero, matching
the incumbent. The full ROM pen store is 16 MiB, not 58,233 separate RGBA textures.
Map addressing uses source UV plus bank; flips precede transpose. Packed cmode
selects subpens before lookup in a **live** 128x256 RGB palette.

There are **112 bank/palette/cmode combinations** and **21,783 unique inclusive
source rectangles** across all models. These counts distinguish material keys,
raw tiles and potential RGBA allocations. Referenced banks are 0..14.

## Static/dynamic boundary and worked examples

| Category | Worked example | Offline/runtime split |
|---|---|---|
| Static reusable | Object 1280, course 6 / segment 10 | 119 immutable quads and source UVs; XYZ bounds (-105320,0,1698)..(-6978,10000,152342). Course tag 1211 maps through DSP ADDK 69 to this terrain object. |
| Static with dynamic material | Object 69 material bank 12, group 28, cmode 0 (additional inventoried example) | Pens and UVs immutable; RGBA uses palette group 28 of the selected picture. A changed group must revise/rebake affected resident recipes. |
| Dynamic transform | Racing car body object 1945 | Actual tick-2500 emitted object with per-instance matrix/translation; 89 immutable quads, bounds (-2658,-508,-2414)..(2658,1964,8330). Offline texture/geometry preview confirms car body. Car-select tag 1765 had earlier been confused with a direct ROM code; that label is withdrawn. |
| Runtime generated | Display-list code 5 | List starts at point RAM 0xF00000; negative non--1 pointers are masked to 24 bits. Geometry must be keyed by snapshot/revision, never treated as `point[5]` ROM asset. |
| Runtime generated | DSP port-3/0x1c-word direct polygon | `rr_scene_direct_poly` queues it and frame consumption clears it. It has no immutable ROM object identity. |
| Dynamic material override | Object-shift record supplies objectflags/cz_adjust | The same mesh can become palette-solid; flags 6 select a direct palette pen and disable shade. Exported textured recipes are not the final material in this mode. |

Course display code at `rd_course_display_list` uses course records at 0xECDC,
five track pieces and camera-relative vertex records from `rd_vertex_record`.
The remaining semantic gap was resolved with the actual tick-2500 race replay:
course 6, segment 10, base tag 1190 yields tag 1211 for its second piece record.
The master DSP instruction at PC 0x4425 is opcode CC45 (`ADDK 0x45`); the recorded
input 1211 maps to 1280. The actual slave-list primitive is object 1280, whose
translation exactly matches the master input record. This establishes reusable
course terrain identity independently of visual guesswork. Object 69 remains
an additional decoded mesh, not the chosen terrain acceptance representative.
Runtime examples above are traced contracts, not exhaustive dynamic coverage.

## Converter, representative pack and validation

See [format and commands](native/assets/FORMAT.md), [compiler/inspector](native/assets/asset_tool.py)
and [actual-asset validation](native/tests/validate_assets.py).
Two complete conversions with actual palette `/tmp/rr-native-texture/palette.rgb`
and representative bake selection 1280,1765 produced identical manifests:
`d4dd84a468065e371b36d63dcfd0503503ee4a1ff86c7baf23f7f296578815eb`.
The manifest records all payload hashes and source provenance; inspection
validates every hash/size, vertex/index agreement, quad winding/index bounds,
source UV and material associations. Coordinates retain integer ROM units;
original dimensions and lighting bytes remain intact, including seams.
59 representative material recipes have actual RGBA plus reusable final pens.

A separate validation against preserved incumbent tick-2500 captured payloads
matches **670 resources, 6,524,539 texels, zero differing RGBA texels**, including
source strides/guards. This provides an independent renderer-output oracle for
source interpretation and the actual palette; it does not establish all banks,
future palette states or portable GPU sampling fidelity. All 2,421 source model
recipes match reread ROM data. Invalid addresses/list pointers, unknown packets,
short ROM files, unknown pack versions and corrupt index payloads are rejected.
Raw results: `/tmp/rr-m1-final-validation.log`;
`/tmp/rr-m1-inventory-summary.json`; converter logs `/tmp/rr-m1-final-convert-{a,b}.log`.
Generated packs `/tmp/rr-m1-final-{a,b}` remain outside Git.

## Bounded material/residency proposal

Bake actual RGBA before gameplay from a selected immutable palette snapshot.
Predecode source pens once, then build versioned RGBA per palette group/recipe.
Reuse unchanged groups; retain bounded revision storage until old snapshots no
longer reference it. Animated palettes require CPU rebakes before use or bounded
prebaked known variants. Do not sample map→pen→palette in a fragment shader.
Do not integrate this design in Milestone 1.

All geometry buffers cost **11,653,920 bytes (11.11 MiB)**. The inspection pack,
including verbose JSON, binary geometry and 59 material bakes, is **55,679,799 bytes**; JSON need not remain resident after loading. Whole-inventory RGBA
rectangles cost **2,167,902,116 bytes (2.02 GiB)**. Simple per-axis cap estimates
(not visual/performance claims): cap 256 = 769,774,964 B; cap 512 = 1,369,816,660 B;
cap 1024 = 1,911,279,584 B. Even cap 256 cannot preload the whole game.
One-texel duplicated gutters on all four sides raise cap-256 estimate to
799,081,204 B, before mipmaps; a complete mip chain is roughly another third.
Capping must be explicitly reviewed for visual fidelity, not inherited as policy.

The actual frozen scene's 670 RGBA resources total **26,098,156 B (24.89 MiB)**,
including incumbent allocation guards. This is a lower bound for a moving track,
not proof that the lap fits. A plausible initial **128 MiB total native asset
residency cap** reserves 12 MiB geometry, 80 MiB RGBA including mips/gutters,
20 MiB decoded pens/maps and 16 MiB metadata/staging, rounded/conservative.
The all-model geometry fits; a scene fits the RGBA slice with apparent headroom,
but track-wide resource union, menu transitions, palette revisions and driver
memory remain required unknowns. Reject loading a track when its computed budget
exceeds the cap; do not hide content by dropping polygons. Avoid simultaneously
retaining full old-world resources and duplicated new resources in later work.

Use independent 2D RGBA resources initially. Future mip chains should downsample
resolved RGBA with appropriate alpha handling; define source-footprint seams and
palette revision identity. Gutters must sample the **next actual source texel**
when preserving current source coverage; duplicated edges are only a proposed
native sampling policy requiring review. Vertex seams/material boundaries stay
split. Filtering/gamma/mip quality and actual Mali upload/residency remain
unverified. The rejected atlas UV relocation/array deletion and dependent lookup
approaches are not reintroduced here.

## Production and acceptance

Fresh host headless build and existing deterministic/input/CLI regressions pass.
Existing H700 preset builds successfully with GPU HUD and parallel preparation
ON, world atlas OFF. Logs `/tmp/rr-m1-{config,build,headless-test,arm-build}.log`.
No emulator/simulation/audio/renderer source changes were made.

Handover's last accepted production hash is
`6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`;
latest comparable production window is 52.900 FPS, 24.899 1% low, 32.685 ms p99,
59.904 Hz simulation. These are historical evidence, **not remeasured baseline**.
Read-only SSH timed out; the escalated retry returned `No route to host`.
Consequently live executable/launcher/backup hashes, recording sidecars and
EmulationStation state cannot be freshly confirmed. No device command reached
execution, no transfers/deployment/frontend stop occurred. Production was
untouched by this task, but live unchanged status is unverified.

Fresh read-only production verification passed on the user-requested authenticated
retry: executable matches the handover hash, EmulationStation PID 3154, no game
process, one Rave Racer launcher, backup SHA256SUMS all pass, controller config
matches backup, GPU HUD default and replay off, reference replay/sidecars present.
This supersedes the initial connectivity/authentication blocker; no device state
was changed. Historical read-only evidence is preserved in the progress report.

## Final terrain identity and data-comparison evidence

Private host diagnostic relinks used the existing headless objects and temporary
copies of generated DSP code. Shared runtime sources/build objects stayed intact.
A destructor exported final board data; logging observed the existing CC45
mapping instructions. Both instrumented and uninstrumented replay runs through
2500 report `status=ok`; endpoint JSON and all captured polygon RAM bytes match.
The original replay and sidecars were read only; no new checkpoint destination
was required. Diagnostic captures/logs remain `/tmp/rr-m1-trace/verified/`.

The course source record at polygon word 0x43A6 is tag 1211 followed by
(18812,-1000,-55399). Those coordinates equal the course-ROM point at index 20
minus player-zero camera coordinates. The master input record at 0x63A6 retains
tag 1211 with (18487,-1000,-54099); its current master-adjusted translation is
copied exactly to the slave primitive at 0x7D1, code 1280, length 0x0D.
The executed mapping log reads `pdp=2392 pc=4425 input=1211 output=1280`.
This connects game course semantics, executed DSP mapping, slave identity and
immutable geometry; no inference from mesh appearance is needed.

A standalone CLI linked the **existing** `eng_walk_list` and `geo_hw_object`
implementation and inspected this saved actual board state. It performs no GPU
rendering or simulation. The selected mesh has 119 quads; the incumbent stage
emits 50 after original culling/behind rejection. All 182 valid raw vertices of
those emitted quads match the converted asset under the recorded Q15 view/model
transform and projection: screen coordinates, depth, UVs and original brightness,
plus quad colour, flags, texture bank and cmode. Remaining original corners are
behind the eye; they are retained in the full asset/topology validation but are
not claimed as projected-coordinate checks. Zero discrepancies.

Durable tooling: `native/tools/build_source_trace.py`,
`native/tools/inspect_scene_geometry.c`, `native/tests/validate_terrain_identity.py`.
Final representative material selection (1280,1765) has 59 baked RGBA/pen recipes,
all validated against actual ROM source and palette. Two new packs have identical
manifests and every payload hash. Raw evidence:
`/tmp/rr-m1-terrain-validation.json`, `/tmp/rr-m1-final-validation.log`,
`/tmp/rr-m1-trace/geometry1280.jsonl`. Reproduction is in the progress report.

**All Milestone 1 exit gates are satisfied. COMPLETE. STOPPED. Milestone 2
requires separate explicit user authorisation.** Track-wide residency, dynamic
scene completeness and handheld rendering remain later-stage unknowns, not
claims established by this asset milestone.

### Asset preview correction and additional representative validation

User-requested local previews confirmed terrain 1280 and racing car body 1945.
The earlier object-1765 car-select label confused a game display-list tag with a
post-DSP ROM object code; its flat-panel asset is not a car representative.
This does not affect terrain mapping/geometry evidence or full inventory.
The new actual terrain/car selection bakes 144 recipes; validation and duplicate
conversion pass. See progress report for final hashes and local PNG links/paths.
The preview tool is an offline CPU asset inspector, not a Milestone 2 renderer.
