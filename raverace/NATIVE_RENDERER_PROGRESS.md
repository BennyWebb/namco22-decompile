# Native renderer progress

**Current: M4A H700 validation complete / STOPPED; real-time/audio acceptance failed; M3 remains performance-BLOCKED, 2026-10-15.**
Milestones 1 and 2 remain complete. The current gate and evidence follow.

## Controlled M4A H700 validation — COMPLETE / STOPPED (2026-10-15)

Full6006-tick H700 replay and all 21 authoritative CPU/RAM, board/video and PCM/voice
checkpoint sets pass.4207 live-native pictures/5,699,037 source commands; all 18
sampled hardware HUDs pass the exact CPU oracle. Small host/Mali raster differences
are disclosed. User reports good visuals with slow/garbled playback; bounded
boot/menu control test1500 ticks has no perceived input delay. Native race controls
and general interactive coverage remain unproven.

No-capture repeat: world+HUD completion **10.187/12.071/13.441/24.458 ms** median/
p95/p99/worst,1 renderer deadline miss. Actual native presentation intervals
**31.601/38.636/41.615/49.866 ms**, all 4207 miss16.693 ms (~31.45 FPS). Simulation+
scene alone median21.034 ms, over budget4206/4207 pictures. Compiler7.277 ms median;
simulation10.432 ms, walk3.293 ms. Serial pipeline/audio real-time acceptance fails:
3,596,071 underrun samples. Clean serial production-copy tick intervals median
28.385 ms; this diagnostic is not the normal threaded/independent launcher.

Native static482,760,448 bytes fits512 MiB, but full-live Mali allocation peaks
~618MB and process peak~727 MiB. Initial test OOM at~tick 300 with192 MiB old M2/tmp
outputs. Those outputs were copied/hash-verified into persistent diagnostic archive,
then RAM copies removed; same prepared build passes. Clean whole-live headroom is
limited (~114.5 MiB minimum available on repeat). No residency/clock/swap/gameplay
optimisation was made. Timing logger double-close bug was fixed in a private
instrumented build; failed run and all written samples retained, repeat exits0.

Separate device test directory: `/userdata/system/rave-racer-native-live-4a-20261015`.
EmulationStation restored; all 47 protected production/ROM/config/replay file hashes
unchanged, no diagnostic game remains. No commit, push or production deployment.
**M3 remains performance-BLOCKED;4B remains unauthorised. STOP.**
See [current report](NATIVE_LIVE_INTEGRATION_REPORT.md) and
[raw evidence/methods](benchmarks/native-live-h700-20261015/README.md).
Review PNGs persist in `native/generated/milestone4a-hardware/results/validation-clean/review/`.
Next work requires separate authorisation: profile live command compilation,
immutable CPU/GPU handoff and a whole-live residency budget; do not begin it now.

## Prior host-only M4A coverage phase (historical)

The full6006-tick host replay now passes all 21 authoritative CPU/RAM, board/video,
PCM/voice checkpoints. All3840 original M3 packets /5,152,196 commands retain parity.
Full-replay audit preceded ROM-authenticated expansion:302 native meshes and four
materials added; code2028/packet1456601 is covered. Native static482,760,448 bytes
fits the unchanged512 MiB ceiling. Boot-only3311 meshes/5245 variants remain explicitly
legacy-rendered; loading all boot/race textures together exceeds the ceiling.
All22 old native readbacks match;18 captures pass exact HUD oracle; seven new
original/native scene pairs reviewed with raster differences disclosed in the report.
General interactive/track/dynamic-format coverage is not established.

ARM prototype prepared locally at `native/generated/milestone4a/coverage-arm/rr_native_live`;
**no transfer, handheld run, deployment, commit or push**. Production and original
simulation/audio/physics/controls/saves/replays untouched. M3 remains performance-
BLOCKED at12/14 ms; H700 gameplay validation is pending and4B is unauthorised.
See [current report](NATIVE_LIVE_INTEGRATION_REPORT.md) and
[evidence](benchmarks/native-live-coverage-20261015/). Stop at this authorised gate.

## Historical initial Milestone 4A — asset-coverage blocker (resolved above)

Explicitly authorised despite M3 remaining performance-BLOCKED. M3's H700 12/14 ms
renderer targets remain unmet; they are not waived or marked passed. M4B is not
authorised. **Stopped at a significant integration blocker; no H700 deployment.**

The isolated host executable generates native commands from the original running
simulation, retaining original input/scheduler/physics/AI/collisions/audio and
checkpoint code. Native rendering begins at tick 1800; earlier boot/menus use the
existing renderer. It completed 3,855 native pictures through tick 5654, then stopped
at tick 5655: **mesh code 2028 / packet 1456601 is absent from the M3 asset pack**.
No missing geometry is silently omitted. Complete live/interactive coverage and
full 6006-tick replay validation remain blocked.

All 3,840 live command packets in the M3 racing window match the offline reference
exactly after normalising offline revision IDs; all 5,152,196 commands are accounted
for. Eleven world/HUD pairs are byte-identical, with independent HUD oracle passes.
All 18 authoritative checkpoint ticks through 5400 match CPU/RAM, board/video and
raw PCM/voice state. This is a successful prefix, **not full replay validation**.

Host unpaced/traced renderer median/p95 2.603/3.068 ms and interval 4.516/5.401 ms
are not H700 measurements or paced FPS evidence. First native frame 166.361 ms is
retained. Serial sim/render overlap is absent; audio output/controller feel and
physical presentation/stutter remain untested. No further optimization follows.

Read [4A report](NATIVE_LIVE_INTEGRATION_REPORT.md) and
[investigation/reproduction](native/live/INVESTIGATION.md).
Host binary: `native/generated/milestone4a/prototype-host/rr_native_live`;
SHA256 `05494648a73d39b93550ad8c93f351a7e28f69b9637f077a02061fe091713ef3`.
Production executable/launcher/controls and all six replay files match prior
hashes; recovery checks pass. No shared game/engine code, saves, production files,
commit, push or deployment changed. Exit private prototype for rollback.

## Slow-picture/HUD/Mali repeatability continuation — BLOCKED (2026-10-15)

**Current investigation complete; STOPPED at the failed repeated headroom gate.**
M2's seven exit checks remain complete. All 69 previous misses were analysed:
dense pictures have 1528 visible quads/181 draws, including 151 protected single-quad
batches. Fenced world completion on those ticks is 13.041 ms versus 8.654 ms overall;
final HUD stays ~4 ms. Near clipping, glyph count and sampled overdraw do not
explain the main deficit. Hardware-level cause of isolated tail waits remains open.

Original-order versus front-to-back **21 actual Mali frames match byte-for-byte**,
including priority, mirrors and HUD; independent HUD oracle passes. The depth-
discard candidate also matches all 21. Combined bandwidth changes match the
entire 3,840-frame host sequence/all 166 LUT revisions; ASan/UBSan passes.

RG8 pen and previous-output color invalidation were rejected for insufficient
measured gain. Dead-depth invalidation showed 0.274 ms paired component benefit
and was tested in three fixed alternating pairs against current optimized front1.
Candidate medians range **11.912–12.112 ms**, p95 **15.489–16.139 ms**,
p99 17.451–18.969, worst 22.815; completion misses per run [136, 77, 89].
All 23,040 timings retained; full completion/interval tables are in the report.
The unchanged 12/14 ms gate fails. Repeated one-fence gains 0.092–0.143 ms fall below the 0.2 ms selection threshold;
deadline misses increase in every pair. Depth discard is rejected and remains
opt-in diagnostic only; no further tuning after the fixed series. Original HUD/LUT remains selected.

Production/launcher/controls, physics/audio/saves/replays and recovery files remain
unchanged; no commit, push or deployment. M4 has not begun and needs explicit
authorisation. Read the [current race report](NATIVE_RACE_REPLAY_REPORT.md),
[method](native/scene/TAIL_HUD_METHOD.md), and
[evidence](benchmarks/native-tail-hud-20261015/).
Actual lossless review gallery:
`/home/bwebb/dev/namco22-decompile/raverace/native/generated/milestone3/tail-hud-review/index.html`.
Previous continuation entries below are historical.

## Depth/LUT/inter-picture Milestone 3 continuation — BLOCKED (2026-10-15)

**Current investigation complete; STOPPED at failed p95 gate.** M2's seven exit
checks remain complete. Matched one-fence full-race current optimized baseline
12.417/15.914 ms median/p95; constrained linear front-to-back candidate
**11.943/15.300 ms**, p9917.260, worst20.666;69/3840 completions >16.693 ms.
Median passes12 ms; p95 fails14 ms. All3,840 ticks and5,152,196 commands retained.
Candidate complete intervals12.086/15.458/17.415/20.851 ms median/p95/p99/worst,
71 deadline misses. No samples excluded or further performance tuning after failure.

Linear reversal only inside proven opaque compatible unprotected batches reduces
overdraw with unchanged draws; conservative D16 proof has27-step minimum separation
and zero ties, actual queried H700 depth24 bits. LUT palette/shadow folding was
byte-exact in host tests but gave only~0.023 ms composition benefit: rejected,
original dynamic LUT retained. No UBO was compiled or revisited. RAM logging
removes demonstrated synchronous output stalls without dropping lines; drain34.467 ms
is reported outside the lap. Historical943 ms stall did not reproduce and remains
unresolved. Selected peak RSS567192 KiB; no additional selected GPU allocation.

Full host combined sequence equals the current optimized output byte-for-byte;
linear permutation equivalence is proven across the lap. Independent CPU HUD
oracle and ASan/UBSan pass. **No new candidate Mali image comparison** was run
before the failed gate; earlier Mali captures validate the previous revision only.

Build `native/tools/build_depth_lut_probe.py OUT --linear`; final selection
`ENG_NATIVE_FRONT=1`, `ENG_NATIVE_FOLD=0`, `ENG_NATIVE_LOG=ram`, certified opaque
shaders, one final fence. It remains an isolated diagnostic renderer. Production,
physics, audio, saves and replay inputs preserved; no commit, push or deployment.
M4 has not begun and requires explicit authorisation.

See [current race report](NATIVE_RACE_REPLAY_REPORT.md),
[method](native/scene/DEPTH_LUT_GAP_METHOD.md), and
[evidence](benchmarks/native-depth-lut-20261015/). Previous entries below are history.

## Fragment/HUD Milestone 3 continuation — BLOCKED (2026-10-15)

Focused investigation complete; **STOPPED at the failed full-picture headroom
gate**. M2's seven exit checks remain complete. M4 is not authorised or started.
No commit, push, production deployment, physics/audio or replay changes.

All5403 material footprints are proven opaque across5,152,196 source commands,
with >=2.499 texel UV guard. The selected standalone path removes world discard
only under that exact pack proof and omits only HUD tiles containing all pen15.
Original cutout shader/fallback, dynamic texture LUT, shadows and priority remain.
An equivalent UBO LUT was tested but rejected as slower; keep it disabled.

Matched one-fence full-race baseline: **17.418/22.714 ms median/p95**. Selected
candidate: **12.310/15.648 ms**,p9918.804,worst22.349;95/3840 completions exceed
16.693 ms. Both fixed12/14 ms gates fail. Candidate intervals median12.636,
p9519.711,p9922.195,worst965.300 ms; all intervals retained. The largest gap is
between pictures (logging/bookkeeping/scheduling), not hidden from the result.
No more optimisation or favourable-run selection after the failed gate.

Full3840 source validation passes; all3840 host selected HUD images match the
original HUD exactly. World priority alpha is unchanged throughout; only ten
isolated world edge pixels differ across the host lap. Eleven actual Mali world
and composed captures are **byte-identical to the previous native Mali captures**;
the independent CPU palette/shadow/LUT oracle also matches every selected pixel.
Dynamic glyph invalidation GL fixture and ASan/UBSan playback pass.

Read [race report](NATIVE_RACE_REPLAY_REPORT.md),
[method and switches](native/scene/FRAGMENT_HUD_METHOD.md), and
[numeric/raw evidence](benchmarks/native-fragment-hud-20261015/).
Review original-left/selected-Mali-right images in the durable local directory
`native/generated/milestone3/fragment-hud-review/h700/index.html`.

Selected build: `build_hud_candidates.py` derives private native HUD sources;
`ENG_NATIVE_HUD_CULL=1`, **`ENG_NATIVE_HUD_UBO=0`**, certified opaque shader directory,
HUD CONTROL0, SPLITnone. Production/shared source is untouched. Resource budgets:
482570240 static world GPU bytes (<512 MiB),229376 dynamic world bytes,3336192
HUD fixed bytes; selected peak RSS567164 KiB, startup27.240 s. No additional GPU
allocation in the selected path. Raw proof/images stay ignored.

Production/launcher/controller, six replay files and five rollback checks remain
unchanged; HUDdefault/replayoff, sole Ports launcher retained, EmulationStation
restored, no game left running. A separately authorised M3 follow-up would need
to address dense-world submission/final-composition costs and attribute the
inter-picture stalls. Current report gives a bounded proposal; none is begun.
**Do not begin M4.**

## Focused Milestone 3 continuation — profiling complete, BLOCKED (2026-10-15)

The user authorised profiling and a conditional indexed-buffer rewrite. The
investigation is complete; **M3 remains BLOCKED and STOPPED**. M2's completed
seven-check gate remains valid. No M4, commit, push or production deployment.

Controlled full-window diagnostics isolate world completion at **12.213 ms median /
17.335 ms p95**, and subsequent HUD at **5.918 / 6.415 ms**. A cheap-fragment
control reduces world to6.101/9.156 ms. Removing all vertex texture gathers and
using four indexed ordinary corner invocations gives11.759/17.119 ms; its median
per-tick paired world saving is **-0.055 ms** (slightly slower). That does not
support a full persistent indexed-buffer rewrite as the next fix. The CPU-prepared
vertex control is diagnostic only, not a new accepted native backend.

The unchanged one-fence reference still fails: world+HUD **17.553 ms median /
22.435 ms p95 /23.767 ms p99 /27.180 ms worst**, against <=12/14 ms.
2197/3840 pictures exceed16.693 ms. No acceptance criterion was relaxed.
All3840 ticks,5,152,196 commands and exact material/transform/HUD source bindings
validate again. Original renderer/shader sources remain hash-identical; prior
full-sequence fidelity evidence remains valid. Six full-window diagnostic runs
and a full host vertex-control lap are accounted for. The incomplete controls
are not performance/visual acceptance evidence.

See [current race report](NATIVE_RACE_REPLAY_REPORT.md),
[profiling method](native/scene/PROFILE_METHOD.md), and
[logs/distributions](benchmarks/native-race-profile-20261015/).
Production executable/launcher/controller, six replay files and five rollback
checks are unchanged; HUDdefault/replayoff, EmulationStation restored. Shared
simulation, physics and audio were not modified or linked to the diagnostic.

Next proposal, **not implemented or authorised**: a bounded M3 fragment/HUD
investigation, first proving recipe opacity and measuring an opaque shader path
without alpha discard only where source coverage allows, while preserving an
alpha-tested path for cutouts. Profile the unchanged HUD's pen pass and final
LUT/shadow composition separately before proposing any equivalent compositor
change. Revalidate all ticks and fidelity, then a matched one-fence full-race
<=12/14 ms gate. M4 remains unauthorised; do not begin it.

## Revised Milestone 3 — BLOCKED at measured headroom gate (2026-10-15)

The authorised focused revision is implemented and tested. M2's seven exit checks
remain complete. **M3 is not complete: STOPPED at the failed H700 performance
gate; M4 is not authorised or started.** See the current
[race report](NATIVE_RACE_REPLAY_REPORT.md), [v3 protocol](native/scene/COMPACT_FORMAT.md)
and [raw/numeric evidence](benchmarks/native-race-revised-20261015/).

All 3840 ticks and 5,152,196 original commands validate. All 3840 revised host
pictures are byte-identical to prior native pictures. Eleven actual Mali captures
cover the full race and preserve the reviewed track, traffic, mirror and HUD
relationships. Static assets remain resident: 482,570,240 bytes (<512 MiB).
Compressed command payload is 107,235,244 bytes; buffering is a bounded three-slot
FIFO. ASan/UBSan two-pass host playback and malformed-stream checks pass.

Clean H700 world+HUD completion: **median 17.418 ms / p95 22.527 ms / p99 23.882 ms /
worst 32.617 ms**, versus the prospective <=12/14 ms median/p95 gate.
2203/3840 pictures exceed 16.693 ms. Intervals: median17.914 / p95 24.190 /
p99 28.476 / worst39.587 ms; offline capacity54.933 pictures/s, 1% low33.403.
These are not live FPS. Queue wait median0.0012 ms rules out packet starvation
in this run; final combined GPU wait median11.812 ms remains substantial.
One final fence is used, unlike the historical two-fence run; no matched-control
speedup claim is made. The failed gate stopped further performance tuning.

Production executable/launcher/controller hashes, default HUD/off replay modes,
all six replay files and five rollback checks remain unchanged; EmulationStation
is restored and no game is running. No commits, push, deployment, live simulation
integration or OS/clock changes. Proposed next work requires separate authorisation:
attribute world/HUD GPU costs, then consider a bounded native indexed-vertex
backend to replace repeated vertex-texture gathers. Do not start M4.

Review actual handheld comparisons in the durable local directory
`native/generated/milestone3/v3-h700-review/` (original left, Mali right).
The full 3840-frame browser review remains `native/generated/milestone3/review/index.html`;
v3's host output is proven identical to that review's native side.

## Historical initial Milestone 3 — BLOCKED (2026-10-15)

User authorised M3 only. M2's seven exit checks were confirmed complete before
work; its evidence and the user's frozen-image approval remain valid. M3 now
has complete source-command capture/playback and host comparison coverage, but
**does not pass its performance gate. STOPPED; M4 not started or authorised.**

Read [race report](NATIVE_RACE_REPLAY_REPORT.md), [scene API/reproduction](native/scene/FORMAT.md)
and [H700 numeric results](benchmarks/native-race-20261015/h700-summary.json).
All 3840 ticks 1800–5639 are captured, validated and rendered on H700:5,117,636
source mesh quads,34,560 direct polygons,15,666 reusable source mesh packets.
Original recording checkpoints and untouched headless/GLES endpoints agree,
including board/PCM/voices. Material/UV/gutter/index/instance/priority/hash gates
pass. Native static world resources are482,471,936 bytes, below the declared512 MiB
full-lap preload ceiling. No steady-state static mesh/material uploads occur.

Final corrected H700 process: completion median**32.729 ms**,p95**39.595 ms**,
p99**42.690 ms**,worst**57.123 ms**;3834/3840 exceed16.693 ms without simulation/audio.
Actual offline capacity31.165 pictures/s,1% low capacity20.924; these are not live
FPS. Packet read median 13.191 ms; derived completion minus reads still19.917 ms,
so storage alone does not explain away the rendering failure. Startup25.438s,
peak RSS567188 KiB. No further optimisation campaign after this failed gate.

The final full host reference/native review set covers3840 pictures. Open
`native/generated/milestone3/review/index.html` (original left/native right).
Nine PNG comparisons are in`native/generated/milestone3/comparisons/`.
An unintended fog difference was fixed to follow production GLES2's explicitly
disabled polygon-fog policy; earlier fog-on data is superseded. H700 frame
readbacks/sequence signoff remain open, so the visual gate stays unchecked.

Delivered code: private race capture/reference relinkers; bounded versioned
source and native scene protocol; offline full-lap material/mesh/command compiler
and stream packer; native GLES3 instanced renderer using resident original local
meshes, GPU transforms/near clipping and production HUD; actual-data validators,
analysis and durable sequence-review tools. No shared game/engine source changes.
Python syntax, host/ARM64 warning-clean renderer builds, packet negative fixtures,
full source/captured palette sampling, complete replay state checks and actual
H700 full-window execution pass their stated scopes. See report for exact commands,
hashes, intermediate capture fixes, remaining limits and raw logs.

At this historical stop, a focused revised-M3 proposal was **not yet implemented/authorised** (subsequently executed; see current entry above):
instance-deduplicated compact commands; versioned immutable HUD resources;
bounded prefetched/compressed transport; six-vertex ordinary quads and a separate
near-clipped path; reduced repeated instance fetches; full world+HUD measurement.
Every source ID/cull reason must remain accounted for. This requires separate
approval before restarting M3. M4 requires M3 completion and explicit approval.

Production unchanged: executable`6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`,
launcher/controller hashes and all replay sidecars match; five backups pass,
HUD`default`, replay`off`; EmulationStation restored(PID 28879), no game running.
Only the isolated userdata diagnostic directory was added; no launcher changes,
commit, push, deployment, save writes or clock/OS changes. HEAD`8fa1331` unchanged.

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
