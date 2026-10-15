# Controlled Milestone 4A live integration — hardware validation complete, real-time gate failed

## Stage 1 architecture/CPU audit — COMPLETE / STOPPED (2026-10-15)

**GO recommendation for a limited three-stage parallel-execution prototype; separate
user authorisation is required. No architecture or CPU optimisation was implemented.**
M3 remains performance-BLOCKED; Stage 2 and Milestone 4B remain unauthorised.

The playable port already has simulation, preparation and GL workers. The native
bridge serialises those stages. Same-frame H700 control measurements put original
world preparation at3.839 ms median CPU versus native walk+compile10.591 ms: paired
additional cost6.722 ms. Compiler7.231 ms median CPU is dominated by conservative
culling and material/palette resolution (~25% /24% of instrumented wall time), not
instance deduplication (~2.5%). Original GLES backend/HUD costs are separate.

Three-stage service modelling has only0.161 ms p99 headroom with assumed0.1 ms
handoffs. A three-tick latency absorbs measured base-case tails;10% CPU contention
causes163 late pictures. This is feasibility evidence, **not proven59.904 FPS**.
The unchanged production threaded baseline presents53.774 FPS with602 unpresented
ticks; picture dropping is not native acceptance. All three H700 runs complete6006
ticks and pass21 checkpoint sets. Host command parity covers all4207 native packets
and the original3840-picture M3 window. All47 protected hashes are unchanged,
EmulationStation is restored, and no profiling game remains. No commit/push or
production deployment. **STOP after Stage 1.**

See [architecture feasibility report](NATIVE_ARCHITECTURE_FEASIBILITY_REPORT.md)
and [retained samples and reproduction](benchmarks/native-stage1-20261015/README.md).

## Controlled H700 4A validation — COMPLETE / STOPPED (2026-10-15)

**Full H700 replay determinism passes; real-time gameplay/audio acceptance fails.**
The isolated prototype completes all 6006 ticks with original simulation, board and
PCM/voice checkpoints. It does not achieve59.904 FPS. M3 remains performance-BLOCKED;
its existing <=12 ms median/<=14 ms p95 acceptance is not waived or retrospectively
passed. No4B work, speculative optimisation, production deployment, commit or push.

### Complete replay, visuals and controls

- Exact prepared ARM binary and validated expanded assets were staged under
  `/userdata/system/rave-racer-native-live-4a-20261015`, with every bundle hash
  verified. Production executable/launcher/settings/saves/replays were not targets.
- Exact prepared-build clean capture run:6006 ticks,4207 native pictures1800–6006,
  **5,699,037 native source commands**, exit0/zero traps. All **21 authoritative
  checkpoint sets (63 domain records)** match CPU/register/RAM, board/video and
  original raw PCM/C352 voice state through 6006. Both telemetry replays and both
  production-copy baselines also match all 21 sets. The original3840-picture M3
  window's5,152,196-command byte parity remains the prior validated host evidence;
  hardware timing did not introduce command-file dump I/O.
- All18 sampled H700 world/HUD pairs pass the independent original byte palette/
  shadow/LUT CPU compositor oracle exactly. Host-versus-Mali rasterization differs:
  none of36 world/composed readbacks is byte-identical; composed mean RGB error
  ranges0.00675–0.16608/255, sparse maximum errors up to255. Reviewed difficult/new
  scenes1965,3600,5655,5889 show no conspicuous missing geometry, mirror, priority
  or HUD corruption. This is sampled fidelity, not an all-frame pixel proof.
- The user observed good visuals with slow movement and garbled/stuttering audio
  in the initial prefix, and **no perceived control delay** in a separate interactive
  boot/menu check. That session was bounded to1500 ticks, before native activation.
  Controller mapping/detection and raw input events are logged. It cannot establish
  native race input responsiveness or general interactive playability. Boot/menus
  remain legacy-rendered and1800 remains a diagnostic activation tick, not a state
  detector. The broader coverage/residency risks in the host phase below remain.

### Low-overhead performance, every sample retained

Primary result is the second no-capture replay (`timing-repeat`), exit0. Same
original serial/coupled policy, audio on, no screenshot readbacks, command dumps,
component fences, concurrent archival or bulk transfer.6006 presentation records
and6006 existing simulation-profiler records are retained;4207 native completion
records cover the full native phase, including the first native picture. All
medians are standard; p95/p99 are nearest-rank. Times below are milliseconds.

| Native phase metric (4207 samples) | Median | p95 | p99 | Worst | >16.693 ms |
|---|---:|---:|---:|---:|---:|
| Original simulation | 10.432 | 13.198 | 15.483 | 19.056 | 16 |
| Original 68K core component | 4.294 | 5.081 | 6.841 | 12.107 | 0 |
| Original DSP component | 2.822 | 3.799 | 6.026 | 8.118 | 0 |
| Original sound CPU/bus component | 1.724 | 2.709 | 3.519 | 9.453 | 0 |
| Original C352 audio mixing component | 1.054 | 1.786 | 2.263 | 4.945 | 0 |
| Live scene walk | 3.293 | 4.278 | 4.864 | 7.080 | 0 |
| Live command compilation | 7.277 | 9.720 | 11.026 | 12.490 | 0 |
| Complete scene preparation | 10.611 | 13.961 | 15.627 | 18.182 | 5 |
| World submission | 1.472 | 2.556 | 3.184 | 5.511 | 0 |
| HUD submission | 0.846 | 1.076 | 1.274 | 4.244 | 0 |
| Final GPU/driver fence wait | 7.536 | 8.741 | 9.231 | 20.675 | 1 |
| World plus HUD completion | 10.187 | 12.071 | 13.441 | 24.458 | 1 |
| Renderer thread CPU time | 2.669 | 3.797 | 4.381 | 6.256 | 0 |
| SDL swap | 0.054 | 0.065 | 0.109 | 15.043 | 0 |
| Actual presentation interval | 31.601 | 38.636 | 41.615 | 49.866 | 4207 |

The four original device/core component rows are portions of simulation, not
additional stages to add to its total.

Completion uses the existing single final `glFinish`. Fence wait is CPU waiting for
GPU/driver completion, **not isolated GPU execution time**. World and HUD submission
can overlap GPU work; they are not separate GPU-duration measurements. Swap-return
intervals are presentation-call observations, not DRM scanout/photon timestamps.
GPU completion excludes original simulation and live command generation; complete
frame intervals include them. There is no across-frame simulation/GPU overlap in
this serial bridge. The legacy PROFILE banner says “no glFinish”; that banner is
inaccurate for the native branch. Native FRAME/LIVE records describe the real fence. Legacy PROFILE cache/quads
counters are placeholders in the native branch (including misleading GPU_end=0);
use native source counts, actual retained-cache report and kernel Mali counters.

The repeated native phase averages **31.45 presented FPS**; all 4207 frame intervals miss16.693 ms. Simulation+scene preparation together have median21.034 ms/p95 26.485 ms and exceed the entire deadline on4206/4207 pictures **before rendering**. Thus renderer-only improvements cannot by themselves resolve this serial CPU budget. Command compilation is a measured7.277 ms median/9.720 ms p95 stage, not an inferred tiny overhead. Its internal causes require separate profiling; no rewrite was attempted. Dense/protected draw work remains, but is not the sole live bottleneck.

The first no-capture run (`timing`) gives a close repeat: world+HUD median/p95/
p99/worst10.182/12.086/13.445/23.884 ms, two renderer misses(ticks1800,3482);
presentation31.578/38.361/41.947/55.202 ms,4207 misses. The repeat has one renderer
miss, tick 1800 (cold native24.458 ms); it is retained. These live renderer-only
median/p95 values meet12/14 **in this live scheduling context**; they do not rerun
or retrospectively pass the historically failed offline M3 benchmark, and do not
satisfy complete real-time gameplay. M3 status remains unchanged.

The first telemetry replay returned2 only after gameplay completed: cleanup called
the logger twice and its second exclusive-create failed. All6006 timing/simulation
rows,4207 native samples and21 checkpoint sets had already been written. An
idempotent private flush guard fixed that instrumentation bug; the repeat exits0.
The original prepared binary and shared simulation/rendering code were unchanged.
All failed and successful runs remain in the evidence, including every slow frame.

### Production-copy comparison

`baseline-clean` uses the exact copied production binary and the same serial,
coupled, audio-on replay policy, without unrelated archival/bulk transfer. It is
**not the normal production launcher's threaded/independent configuration**; no
claim is made about normal production FPS or sound quality. Baseline has no matched
native final GPU fence; compare completed replay-tick intervals, not GPU completion.
Both rows below use the same completed-tick endpoints for ticks1800–6006.

| Completed tick interval | Median | p95 | p99 | Worst | >16.693 ms |
|---|---:|---:|---:|---:|---:|
| Production copy, serial diagnostic | 28.385 | 37.268 | 41.238 | 85.776 | 4207 |
| Native prototype, serial diagnostic | 31.609 | 38.651 | 41.571 | 49.873 | 4207 |

The prototype is slower on the complete serial pipeline despite its lower renderer
cost. All6006 ticks are retained too: prototype completed-tick intervals median/
p95/p99/worst30.777/40.390/43.907/190.227 ms (5460/6005 misses); clean baseline
27.360/39.658/43.381/173.255 ms (5455/6005 misses). Boot and transition outliers are
not hidden. The earlier baseline overlapped archival of old RAM files and has a
5684 ms worst stall; it is retained as a confounded run and not used to claim an
improvement. There is no arbitrary warm-up/sample exclusion.

### Memory and startup

The unchanged native static figure is **482,760,448 bytes (460.396 MiB)**, within the
existing512 MiB ceiling. It excludes retained legacy textures (~66,001,964 requested
bytes at exit), native dynamic buffers229,376 bytes, requested HUD allocations
3,336,192 bytes (Mali chose depth24), lookup4,517,232 CPU bytes, other legacy buffers,
source ROMs, simulation, staging and driver allocations. Requested application
figures do not account for all driver/page/heap costs.

| Observed run | Peak RSS (KiB) | Peak VmHWM (KiB) | Peak Mali bytes | Minimum MemAvailable (KiB) |
|---|---:|---:|---:|---:|
| Capture | 744612 | 744644 | 617852928 | 124796 |
| Timing | 744644 | 745024 | 617582592 | 122564 |
| Timing repeat | 743952 | 743952 | 617582592 | 117240 |
| Clean production copy | 277632 | 278196 | 152190976 | 577716 |

RSS includes mapped Mali backing on this unified-memory device: **do not add RSS
and GPU values**. Full-live GPU allocation is about589 MiB and process peak about
727 MiB, substantially above the native-only figure.1Hz polling can miss shorter
peaks; VmHWM and kernelOOM evidence are retained. Native requested-resource ceiling
passes; whole-process512 MiB compliance is neither claimed nor achieved.

Capture-run memory was already high before native activation: legacy phase peak
616,443,904 Mali bytes/739,448 KiB RSS, versus617,852,928/744,612 during native.
Across the native phase, first/last sampled GPU allocations617,721,856→617,852,928
bytes are approximately flat. This is stability over one replay, not a long-session
leak proof. The repeat's minimum available memory117,240 KiB (~114.5 MiB) and low
CMA headroom leave limited tolerance for other applications or staging.

The first exact prepared-build run was killed by kernelOOM after logged tick 300,
before native activation: kernelRSS612,360 KiB, sampled Mali543,862,784 bytes.
A shell sampler's16 KiB fork-stack allocation triggered the OOM decision; the system
had4528 KiB free,764 KiB free CMA, no swap and unreclaimable memory. Only the simulation
record300 survived buffered I/O; it matches, but board/audio triplet and timing
buffers did not survive SIGKILL. No complete checkpoint or distribution is claimed
for that failed run. All retained kernel/memory logs remain available.

Approximately192 MiB of **pre-existing M2 diagnostic files** occupied RAM-backed/tmp.
Nine enumerated trees were copied into persistent diagnostic storage, SHA256-verified,
then their RAM copies removed. No save/config/replay was removed. A persistent C
sampler replaced repeated shell forks. Both environmental changes are disclosed;
no renderer residency, ceiling, swap, clock/governor or gameplay optimisation was
made. The same prepared executable then completed the entire replay. Old diagnostic
files remain recoverable under `stale-m2-tmp-archive` with manifests/verification logs.

Native preload was36.949s in the failed memory-pressure run,24.456s in the clean
capture run and24.957s in the repeat. Startup precedes tick1; it is reported separately
from frame distributions, not erased. The native first-frame and legacy→native
transition are included in timing. SSH connectivity intermittently stalled; local
logging and auto-restoration continued, including after an SSH client timeout.

### Audio, controls, integrity and remaining work

The repeat has **3,596,071 underrun samples**, zero overruns/resets and an empty
output queue at its last timing sample. The first timing run has3,602,645 underruns;
clean serial baseline2,927,303, zero overruns/resets. ALSA output48kHz stereo with
512-sample buffer opened successfully. The CPU audio stages and raw output hashes
are unchanged, but insufficient wall-clock progress starves audible playback.
User-observed garbled/stuttering audio supports that failure; deterministic PCM
hashes do not certify real-time sound or original wall-clock simulation speed.

The input check is limited to original boot/menu handling before native 1800.
The user reports no delay. There is no instrumented input-to-photon latency measure,
full interactive native race, alternative-track validation, display scanout proof,
or original real-time pacing acceptance. General coverage still fails explicitly
for unseen point-RAM/solid/missing recipes. This is not production-ready playability.

Final checks pass: **all 47 protected file SHA256 values are unchanged**, including
production executable/launcher, controller configuration, HUD/replay settings,
ROM/EEPROM source files and all six original replay files. EmulationStation is
running, no diagnostic game remains, HUD mode isdefault and replay modeoff.
The interactive session exits0 at1500/zero traps,0 native pictures;4 button events
(SELECT/south) and95 axis events were recorded. The user reports no control delay.
No shared engine/game sources were edited. No commit, push or production deployment.

- [x] Isolated H700 transfer and hash validation.
- [x] Complete6006-tick H700 replay and all 21 authoritative checkpoint sets.
- [x] Sampled hardware visual/HUD fidelity and disclosed raster differences.
- [x] Full timing distributions, every available sample, memory and audio diagnostics.
- [x] Bounded original boot/menu controller test and physical feedback.
- [x] EmulationStation restored and production integrity verified; STOPPED.
- [ ] Consistent59.904 FPS, original real-time pacing and smooth audible playback.
- [ ] Interactive native race controls, unseen states and general game coverage.
- [ ] M3 performance acceptance (remains BLOCKED; not waived or retrospectively passed).


Recommended next work, **only with separate authorisation**:
1. Profile the live walk/command-compilation stages internally; the aggregate CPU
   cost already exceeds the available frame budget before GPU completion.
2. Design a safe immutable scene handoff if pursuing cross-frame CPU/GPU overlap;
   preserve every original tick, authoritative state, audio and event ordering.
3. Define a whole-live memory budget, release/partition retained boot residency and
   validate bounded state/track working sets without increasing the static ceiling.
4. Retest real-time audio, native interactive controls and actual presentation only
   after the measured complete-pipeline deficit is addressed. No audio/physics/timing
   shortcut, speculative GPU rewrite or production replacement is justified here.

Evidence: [hardware validation](benchmarks/native-live-h700-20261015/), including
raw FRAME/LIVE logs,6006-row telemetry/simulation CSVs, all deadline flags, checkpoint
files,1Hz memory traces, kernel logs, initial failure and comparison runs.
Review images (host left/H700 right) persist under
`raverace/native/generated/milestone4a-hardware/results/validation-clean/review/`.
Prototype/build scripts and exact measurement limits are in the evidence README.
**STOP. M3 remains performance-blocked; no Milestone 4B begins.**


## Prior host-only coverage phase (historical; diagnostic transfer subsequently authorised)


## Focused 4A asset-coverage continuation — COMPLETE / STOPPED (2026-10-15)

The tick-5655 asset blocker is resolved for the complete recorded replay. **M3
remains performance-BLOCKED** at its unchanged <=12 ms median / <=14 ms p95 H700
world-plus-HUD gates. This completes the authorised host coverage investigation,
not overall M4A hardware acceptance or general interactive coverage. No M4B work,
commit, push, transfer, handheld run or deployment occurred.

### Acceptance evidence

- [x] Audited all **6006 source ticks / 7,097,784 commands** before expansion.
  Found 19,279 mesh identities and 10,652 exact recipe/palette variants. Against
  M3: 3,613 meshes / 5,249 variants absent, explicitly partitioned into native-use
  **302 meshes / four variants** and legacy-boot-only **3,311 / 5,245**.
  Neither portion contains point-RAM or solid-format quads. These observations
  concern this replay only.
- [x] ROM-authenticated all 3,613 absent mesh identities against captured local
  positions/UV hashes. Appended all 302 native-needed meshes (including all 203
  code-2028 packets, specifically packet1456601). Other newly needed codes are
  2029,2018,2006,1278,2066,1276,1277. Added four exact materials, using three new
  unique padded images; one reuses existing exact image content. Pack totals:
  **15,968 meshes / 5,407 materials / nine pages**. Original mesh/material IDs,
  material binary prefix, UV placement and eight existing pages remain unchanged.
- [x] Native static GPU footprint **482,760,448 bytes (460.396 MiB)**, an increase
  of 190,208 bytes, with **54,110,464 bytes** below the unchanged 512 MiB ceiling.
  Full boot+race distinct padded texture content alone is **605,170,708 bytes**;
  full simultaneous residency cannot fit in the current disjoint-rectangle RGBA atlas design. No limit was raised. Original rendering
  remains explicit for ticks1–1799, with native rendering on ticks1800–6006.
- [x] Host prototype completed **6006 ticks**, **4207 native pictures** and
  **5,699,037 native-window source commands**, exit0, zero traps. All **21 complete
  authoritative checkpoint sets through 6006** match CPU/RAM, board/video RAM,
  raw PCM and C352 voice hashes. Audible output was off; original mixing and PCM
  checkpoint generation remained active. This does not validate physical audio.
- [x] Original **3840 ticks1800–5639 / 5,152,196 commands** retain byte-identical
  compact commands after the existing normalization of only offline font/style/
  palette revision IDs. All **22 existing world/composed readbacks** match exactly.
- [x] Seven new-scene pairs reviewed at5655,5867,5874,5878,5889,5921,6006 against
  a fresh original-renderer replay. Geometry, tunnel textures, mirrors, priorities
  and HUD show no conspicuous missing objects. Original GLES2 and native GLES3
  rasterization is **not pixel-identical**: composed changed RGB pixels2.96–23.11%,
  mean absolute RGB error0.278–1.600/255, maxima248–255. These aggregate differences
  are disclosed, not waived or called exact fidelity. The independent original
  palette/shadow/LUT CPU oracle matches **every pixel in all 18 native captures**.
  Final subjective approval of original-versus-native appearance remains open.
- [x] Isolated ARM64 prototype built locally for review, **not transferred or run**.
- [x] Coverage risks, bounded residency proposal and reproduction documented.
- [ ] General interactive/menu/alternative-track/dynamic-format coverage proven.
- [ ] H700 live performance, controls, sound output and physical pacing measured.

Evidence: [native-live-coverage-20261015](benchmarks/native-live-coverage-20261015/),
especially `assets-validation.json`, `reproduced-validation.json`,
`host-checkpoints.txt`, `hud-validation.json`, `visual-comparison.json`,
`coverage-budget.json` and `run-summary.json`.

### Coverage limits and bounded residency proposal

The audited replay is one deterministic input path. Boot/title/attract/selection
states use the original renderer; their absent native assets are inventoried, not
silently dropped. The activation tick 1800 remains a diagnostic boundary, unsuitable
as a general interactive state detector. Other tracks, reverse/alternate routes,
car selections, traffic combinations, race results, continues and arbitrary palette
changes have not been exhaustively sampled. ROM-authenticated geometry does not
prove that all lighting/packet combinations will compile in unseen live states.
Point-RAM(code5), solid formats and future unknown recipes still fail explicitly;
none was exercised here. Lookup limits remain32768 meshes/8192 materials, commands
4096, instances512 and direct512; observed native maxima2157/111/15 fit. Full
boot+race material count10652 already exceeds the current resolver's8192 limit.

A future full-native solution should build ROM-authenticated state/track bundles,
retain stable global asset identities but resolve them to bounded resident slots,
and load before state transitions. Enforce **512 MiB total native static residency**,
reserve geometry/material tables first, pack texture bundles into the remainder,
and refuse admission before allocation if a required working set exceeds it.
Use a bounded staging buffer, fenced retirement and explicit transition loading;
retain required current/next-state assets and evict only unused resources. A scene
must never omit an object on a cache miss. Dynamic palette changes need explicit
validated resources or a bounded pen/palette representation. Per-state worst-case
working-set sizes, transition peaks and stalls require separate measurement before
this proposal can be implemented or accepted. No streaming rewrite was attempted.

The native-only ceiling is not whole-process VRAM/RSS: the original boot texture
cache remains live (~66.8 MB at end), plus native dynamic buffers229,376 bytes,
HUD requested-format attachments3,336,192 bytes, lookup4,517,232 CPU bytes, source
ROMs, simulation and driver allocations. Thus combined legacy+native texture
residency can exceed512 MiB even though native static resources meet their existing
ceiling. Releasing/partitioning boot residency is future integration work; no claim
of whole-process512 MiB compliance or H700 memory safety is made.

### Review artifacts and reproduction

All paths below are under the repository, **not `/tmp`**. Generated ROM-derived
assets and images remain ignored by Git.

- Pack: `raverace/native/generated/milestone4a-coverage/race-live-v2/`
- Lookup: `raverace/native/generated/milestone4a-coverage/lookup/live.lookup`
- Host: `raverace/native/generated/milestone4a/coverage-host/rr_native_live`
- ARM: `raverace/native/generated/milestone4a/coverage-arm/rr_native_live`
- Review PNGs: `raverace/native/generated/milestone4a-coverage/review/`
  (`tick-5655-original-left-native-right.png`, plus5867,5874,5878,5889,5921,6006).
- Full source capture/audit, original/native logs, checkpoints and command dumps:
  `raverace/native/generated/milestone4a-coverage/`.

SHA256 ARM: `a9e84cf5f0d0576efbcb011a880c340e1cb38dd990055891601bd50146bc2f04`.
SHA256 pack manifest: `2872f4e1984a21e610e5f764c5cd14198fa3b00ffd023bc0a55fdce9a2e2b362`.
The prepared binary needs the expanded pack, lookup, certified shaders, original
ROMs and compatible runtime libraries; it is not a standalone production package.
Rollback is to exit the private prototype. No original simulation/engine sources,
physics, audio, controls, saves, production or replay files were edited. No contact
with the handheld occurred during this continuation; earlier production hashes are
historical read-only evidence, not a fresh device attestation.

Tools: `build_race_capture.py --first 1 --last 6006`,
`audit_live_coverage.py`, `extend_live_assets.py`, `validate_live_assets.py`,
`export_live_lookup.py`, `build_live_prototype.py`, `validate_live_evidence.py`,
and `check_native_hud_pixels.py --live-states .../original-captures`.
Exact commands are in the evidence README. **STOP here.** Hardware transfer/run
and any subsequent integration or optimization require separate authorisation.


## Historical initial 4A investigation (superseded coverage blocker)



**STOPPED at a significant integration blocker.** The isolated host prototype
renders commands generated by the original running simulation, but the M3 asset
pack cannot cover the complete original input replay. No H700 build was transferred,
deployed or run. H700 playability and initial hardware gameplay measurements remain
unestablished. Milestone 4B has not begun.

The user explicitly authorised 4A despite M3's performance failure. **M3 remains
BLOCKED**, with its <=12 ms median / <=14 ms p95 H700 world-plus-HUD targets unmet.
The ultimate target remains consistent 59.9040599 FPS with original gameplay.
Host timing results below neither pass nor waive those gates.

## Delivered implementation

`native/tools/build_live_prototype.py` privately relinks the existing GLES game
build as `rr_native_live`. The generated copies change presentation call sites;
original translated CPU/DSP/game, physics, AI, collisions, input handling,
simulation scheduler, sound/audio mixing and replay/checkpoint implementations
are linked from the original build. Shared `engine/`, `raverace/src/` and
`raverace/include/` sources are unchanged. No commit, push or production replacement.

The serial prototype retains the original SDL controls, input replay, pacing,
overlay and swap code. It requests GLES3 for the existing M3 shaders and rejects
the threaded presentation option. Before tick 1800 it uses the original renderer;
from tick 1800 it walks the **current simulation's** display list and direct FIFO,
then compiles native commands in memory. This diagnostic activation tick is not a
race-state detector. Different interactive menu/race timing may encounter unsupported
assets earlier; this is not yet a general-purpose playable port.

`native/live/compiler.inc` resolves immutable mesh identities and exact material/
palette variants, preserves the original painter ranks, protected batches, scissors,
near clipping, direct geometry and conservative culls, and emits the M3 compact
command layout. The existing optimized M3 GPU path performs the same guarded
front-to-back ordering, opaque shading and transparent HUD glyph omission.
Rejected depth/color-discard, RG8, UBO and folded-LUT options are not selected.
The original live HUD routine reads current character RAM, tilemap, palette,
scroll, shadows and gamma/fade LUT; recorded HUD frames are not substituted.

`export_live_lookup.py` exports only immutable mesh/material identities and exact
palette values from the proven asset pack: 15,666 mesh entries, 5,403 material
entries, 4,510,408 bytes. It contains no camera path, transforms or scene-command
stream. The renderer no longer opens `frames.rsc`, `hud-fonts.bin` or
`hud-styles.bin`. Commands originate from live game state, even though recorded
human **inputs** provide deterministic validation of that running game.

The original scene latch and direct FIFO consume happen once per emulated video
boundary. No simulation ticks, logic budgets, source commands or audio samples
are skipped by the bridge to manufacture speed. The existing pacing/presentation
policies are retained; the test uses the existing unpaced correctness mode and
is not evidence of cadence preservation on hardware. Serial simulation and
rendering do not run concurrently; GPU execution can overlap CPU submissions
within rendering, but exact hardware overlap is not measured here.

## Blocking evidence and stop

The fresh simulation run rendered **3,855 native pictures, ticks 1800–5654**.
At **tick 5655** it emitted a mesh absent from the M3 pack:

| Field | Value |
|---|---:|
| Model/code | 2028 |
| Point-ROM packet | 1456601 |
| Geometry records emitted at failure | 1467 |
| Packed meshes with code 2028 | 0 |
| Process exit | 2 |

Raw diagnostic:

```text
[NATIVE-LIVE-BLOCKER] tick=5655 reason=mesh absent from immutable M3 pack code=2028 packet=1456601 emitted=1467
[NATIVE-LIVE] completed_native_pictures=3855
```

This is a live asset-coverage failure, not a performance gate or a simulation
hash mismatch. The M3 pack covers the captured racing window, not every mesh the
running game can request. Even the unchanged original input replay requests an
uncaptured object shortly afterwards. Continuing would require extending and
validating live asset coverage or authorising an explicit fallback policy.
Silently omitting the object, substituting recorded pictures, or shortening the
replay would not satisfy this experiment. **No further integration, asset expansion,
optimization or replay retry was undertaken after this blocker.**

The existing compiler also explicitly rejects unsupported point-RAM/solid formats
and out-of-capacity/unmapped material variants. Those are known coverage limits;
this run's actual stopping cause is the missing mesh above. No unsupported format
was silently represented as a textured static object.

## Determinism and visual fidelity

The original recording and its EEPROM/sidecars were used unchanged. Every emitted
300-tick checkpoint through **tick 5400** matches the original recording exactly:
**18 checkpoint ticks / 54 domain records**, covering CPU/register/RAM state,
board/video memory, cumulative raw PCM, C352 voice state and sample counts.
Audio output was disabled for the host correctness run; mixing and checksum logic
remained active. Audible synchronisation, underruns and controller feel were not
physically validated.

**Full replay validation does not pass:** the run exits at the asset blocker before
the recording's final tick 6006. The successful checkpoint prefix must not be
reported as all 21 original checkpoints or as a validated final game state.
The unmodified full-replay validator retains its original strict requirements.

All **3,840** freshly generated native command packets for ticks 1800–5639 are
byte-identical to M3 after normalising only the offline font/style/palette revision
IDs. These IDs are not used to select live HUD content. All geometry, transforms,
brightness, depth/rank, source identities, instance keys, material IDs, direct
vertices, clipping/scissor flags, culls and batches match exactly. Totals remain
**5,152,196 source commands**, **3,677,980 visible commands** and **1,474,216 explicit
existing culls**. There are no new content omissions. Draw counts remain
105 median / 172 p95 / 241 maximum; protected single-quad batches are preserved.

Eleven host world/composed capture pairs at the established representative ticks
are **byte-identical** to the selected M3 host renderer, including priority alpha.
The independent original byte palette/shadow/LUT CPU oracle matches every sampled
composed pixel. This is sampled host raster validation plus full command parity,
not a new Mali capture, every-frame live raster comparison or user visual signoff.

## Initial diagnostic measurements — HOST ONLY

Platform: x86_64, Mesa llvmpipe ES3.2. Startup preloads native resources in
**1612.950 ms** before simulation begins. The unpaced serial run includes detailed
simulation/HUD profiling, per-picture command dumps and eleven image captures.
**Every sample is retained**, including the first native-frame stall. These are
instrumented host observations and cannot predict H700 frame rate.

Ticks 1800–5639, milliseconds:

| Measurement | Median | p95 | p99 | Worst |
|---|---:|---:|---:|---:|
| Live list/geometry walk | 0.258 | 0.354 | 0.444 | 1.166 |
| Live command compilation | 0.555 | 0.787 | 0.951 | 1.403 |
| Native renderer through final GPU fence | 2.603 | 3.068 | 3.544 | 166.361 |
| Consecutive native completion interval | 4.516 | 5.401 | 5.948 | 166.361 |
| World submission span | 0.485 | 0.699 | 0.835 | 162.303 |
| HUD submission span | 1.513 | 1.821 | 2.123 | 3.668 |
| Final GPU/driver wait | 0.564 | 0.740 | 0.969 | 2.285 |

One renderer completion and one completion interval exceed 16.693 ms in this host
window. The first native picture takes 166.361 ms; its world submission accounts
for 162.303 ms. This exposes a first-use/transition hitch on the host, not proven
H700 stuttering or an exclusive shader-compilation timer. No warm-frame selection
hides it. The first native interval uses start-to-completion because it has no
previous native completion; subsequent intervals include simulation, live command
work, swap, telemetry/checkpoint/dump costs between native completions.

Renderer completion starts after live command generation. Walk and compilation
are listed separately; they must not be silently omitted from gameplay cost.
Unfenced submission spans overlap GPU work and are **not independent GPU timings**.
Compilation timing excludes its diagnostic packet-file write, which remains in
end-to-end intervals. RAM native telemetry retains 1,989,367 bytes and drains in
1.086 ms after termination. Original diagnostic logging/checkpoint writes also
remain; no low-overhead hardware headline benchmark is claimed.

Original simulation profiling separately reports median/p95 wall time
0.822/1.069 ms; C352 mix 0.152/0.219 ms and sound-board processing 0.200/0.297 ms.
That profiler's 4,256-tick cohort differs from the 3,840-tick rendering table and
must not be combined as matched per-frame medians. Original swap profiling averages
about 0.002 ms on the offscreen host; it is not a physical scanout timestamp or
vsync measurement. Existing legacy texture/quad counters do not describe native
buffers: their zero native quads/uploads do **not** mean no world was drawn or
no GPU resources were resident. Use the native command/frame records instead.

Native static GPU accounting remains 482,570,240 bytes; native dynamic resources
229,376 bytes and original requested-format HUD accounting 3,336,192 bytes.
The original boot renderer also retains its own texture cache (~66 MB in this
run), so native-only M3 memory figures cannot be used as this live prototype's
total footprint. CPU lookup metadata adds ~4.51 MB, plus bounded command/capture/
trace storage. Actual H700 RSS, physical depth allocation, CPU/GPU concurrency,
presentation deadlines, sound output and visible stuttering remain unmeasured.

## Isolation, reproduction and rollback

Host executable:

`/home/bwebb/dev/namco22-decompile/raverace/native/generated/milestone4a/prototype-host/rr_native_live`

SHA256: `05494648a73d39b93550ad8c93f351a7e28f69b9637f077a02061fe091713ef3`.

The builder requires a compatible existing GLES CMake build and relinks original
objects plus generated private main/GL/presentation/HUD/geometry copies. It does
not edit the shared source or original executable. `--rebuild` is restricted to
the ignored milestone4a output tree. Resource lookups and game images stay ignored.
`native/live/INVESTIGATION.md` gives exact commands and evidence locations.

Runtime requires explicit `ENG_LIVE_PACK`, `ENG_LIVE_SHADERS`, `ENG_LIVE_LOOKUP`
paths. `ENG_LIVE_FROM` defaults to 1800. Optional `ENG_LIVE_CAPTURE` and
`ENG_LIVE_COMMAND_DUMP` enable diagnostics. The `ENG_` namespace deliberately
leaves the original replay startup's `RR_*` environment sanitisation untouched.
Use fresh checkpoint/dump output paths. No command in this experiment copies an
executable into Ports, modifies a launcher, writes production saves or replaces
input replay files.

Rollback is to exit the private executable and run the unchanged production
launcher. Nothing was deployed on the handheld; no second Ports launcher was
created. Read-only H700 checks confirm production executable, launcher, controller
configuration and all six original replay files match their prior SHA256 values;
five recovery backup checks pass. HUD mode remains default, replay mode off,
and EmulationStation remains running.

## Exit gates and required next decision

- [x] Roadmap records explicit 4A exception while leaving M3 performance-blocked.
- [x] A private host prototype renders commands from the running original simulation.
- [x] Full M3-window live command parity and sampled host visual/HUD fidelity pass.
- [x] Partial authoritative state/board/PCM/voice checks pass through tick 5400.
- [ ] Full original replay reaches tick 6006 with all authoritative checkpoints.
- [ ] Asset coverage supports the full intended live/interactive workload.
- [ ] ARM/H700 prototype and initial physical gameplay measurements complete.
- [x] Production protected, evidence documented and stopped at the integration blocker.
- [ ] Ultimate consistent 59.904 FPS / original behaviour acceptance established.

Lifting the blocker requires a separately authorised continuation to extend live
asset coverage, validate the original complete replay and then obtain explicit
hardware transfer/run authorisation under the user's no-deployment constraint.
No such continuation or Milestone 4B work is started by this report.

Evidence: [native-live-4a-20261015](benchmarks/native-live-4a-20261015/).
