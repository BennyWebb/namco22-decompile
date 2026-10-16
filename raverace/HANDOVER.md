# Rave Racer RG40XX H handover

Start a new session with: **“Read `raverace/HANDOVER.md`, inspect the current
repository state, then [state the new task].”** This file is a signpost, not a
substitute for checking current source, Git status, device state and the cited
reports. Update it when a milestone changes the active build or benchmark.

## Standing runtime and experiment rules

Use `ENG_*` for new runtime environment switches: replay strips `RR_*`/`SND_*`.
Before switch-dependent timing, smoke to~1900 and verify actual receipts/census.
Correctness failures, safety problems or decisions: STOP/report. Routine setup or
configuration errors: FIX AND CONTINUE within the user’s device-time budget.

## First real Native play session — read-only review / STOPPED (2026-10-16)

See [play-session report](NATIVE_PLAY_SESSION_REPORT.md) and [retrieved telemetry](benchmarks/native-play-20261016/README.md). One post-smoke session22:50:25–23:04:07 BST,48,876pictures,all four raw course selectors; user reports perfect look/feel/sound. Telemetry:48,714native/162boot-prewarm legacy,zero unsupported fallback keys;7inferred repeats (6native),1672settled audio-shortage frames (~34.8ms),one50.270ms main gap following47.200ms swap wait; cause unproven. Race prep p99 selectors0/1/2/3=12.92/12.94/14.14/14.24ms. Peak65.186°C,no cooling/clock flags; native EEPROM changed/persisted once. Transport errors/drops/incomplete joins0; bounded THREAD/SIM detailed profiler overflows40684/40685 documented. Retrieved text telemetry only; device access strictly read-only. Production SHA and47protected paths exact,clocks restored,ES running,no game; no commits/push or fixes. Recommend separately authorised rare-stall attribution/diagnostic retention only; no work begun.

## Milestone G2 — separate playable Native entry / COMPLETE / STOPPED (2026-10-16)

See [G2 report](NATIVE_G2_REPORT.md) and [evidence](benchmarks/native-g2-20261016/README.md).
Private `native/g2/`: bounded-pool cache index512KiB and immutable-slot prefix reuse.
Host21sets/4207packets/36provenance images pass,c=0. Single reference prepare median
8.305ms (target≤8.54),dense p9912.735; pacing0repeats/settled underruns/main gaps.
**Ports → Rave Racer (Native)** now runs the separate app at
`/userdata/system/rave-racer-native-g2/`; production entry remains untouched.
First-launch ROM-only cache compiler verified on host/device; cold22.024s,
verified hit0.689s. Native EEPROM/config copies live in
`/userdata/saves/rave-racer-native/`. Session summaries/logs in app `sessions/`
and `latest-summary.{txt,json}`; bounded async transport,10Hz resources.
CPU1512 session profile/watchdog restoration; GPUstock,q2,index,state activation,
default priorities/fallback. Corrected7170-picture START+SELECT smoke passes;
7008native,zero native repeats/settled underruns/main gaps; one boot/prewarm repeat.
Initial setup failures retained/explained. All47production paths and30app hashes
verify; stock clocks/EmulationStation restored,no game. No commit/push.
Removal:`/userdata/system/rave-racer-native-g2/remove.sh` (saves/evidence retained).
**STOPPED:** user can play from separate entry; no further milestone authorised.

## Milestone G1c — attract PASS / reference preparation regression / STOPPED (2026-10-16)

See [G1c report](NATIVE_G1C_REPORT.md) and [evidence](benchmarks/native-g1c-20261016/README.md).
Private `native/g1c/` confirms and fixes recipe-cache saturation: old table fills
at12839; bounded exact-key pool/generation/probe policy prevents full-table scans.
3072² dynamic page shares ROM-aligned source rectangles; three copies cost27MiB
(+24MiB), zero evictions. No replay recipes added to static assets.
Host:21 sets/4207 packets/36 images pass, class(c)=0; raw packets/colour exactG1b.
Device:one reference run state/pacing pass; 928.459s attract,55443pictures, all
selectors≥4 complete visits, zero repeats/settled underruns/main gaps>50ms.
Probes mean1.00175,max3; occupancy plateaus39946; total Mali peak178.863MiB.
User reports looking/sounding great. **Overall no-regression condition remains
BLOCKED:** reference preparation median9.032 versus8.334–8.343ms, densep99
13.362 versus13.120–13.222ms, despite clean pacing. Do not call this an
unqualified G1c pass. Next approved investigation should isolate sparse-cache
access versus repeated enlarged slot-copy costs; no such work has started.
Production/47 paths and22 runtime asset hashes exact; governors/RT restored,
EmulationStation running, no game/sampler. No commit/push/production deployment.
STOPPED; no G2 or further optimisation without separate authorisation.

## Milestone G1b — reference PASS / attract BLOCKED / STOPPED (2026-10-16)

See [G1b report](NATIVE_G1B_REPORT.md) and [evidence](benchmarks/native-g1b-20261016/README.md).
Private `native/g1b/` resolves direct footprints from ROM into bounded LRU R8UI
pages with immutable slot uploads and three fence-protected GPU copies. No replay
recipes added to the unchanged 2,421-model / 112 MiB static whole-ROM pack.
Host/Mali: all 21 sets, 4,207 adjusted packets, 36 provenance images pass, class(c)=0.
Both reference timing windows 1800–6006: zero repeats/settled underruns/main gaps;
dense preparation p99 13.120/13.222 ms. Full activated timeline retains one
non-handover repeat at 1578 in run 2, separately from run 1's handover repeat 164.
State activation starts at 163, release completes 178. Attract visits selectors
0–3 with no unsupported fallback, but its second course-0 pass suffers compiler
median 290 ms, severe repeats/audio shortages. Stopped after 356 s, not a completed
10-minute gate; recovered samples prove slow progress rather than deadlock.
Cache saturation is a suspect, not yet proven; no speculative fix or G2 work.
Observed play Mali peak 154.656 MiB (debug 167.734) is within budget. All samples retained.
Production/47 paths exact; CPU/GPU clocks and RT defaults restored, EmulationStation
running, no game/sampler. No commit/push/production deployment. Separate approval
is required for blocker attribution/fix or broader coverage.

## Milestone G1 — ROM assets PASS / host integration BLOCKED / STOPPED (2026-10-16)

See [G1 report](NATIVE_G1_REPORT.md) and [evidence](benchmarks/native-g1-20261016/README.md).
**User decision:** compact palette-free index atlas ADOPTED; fenced≤14ms gate RETIRED
for vsync-paced runs. Acceptance is pacing/audio/main gaps/preparation tails. C1e's
historical measurements remain unchanged; this is a policy change, not a retroactive pass.

Private native/g1 builds ROM-only2421models/97116quads/21783materials,112MiB atlas;
564081621padded texels exact,115.553s host build. Cache ignored and ROM+builder keyed.
Index exact-key cache and immutable RAM-state activation/live warmup implemented,
but **not accepted**:6006host ticks/all21sets exact, all4207 native-window pictures
fall back for direct materials absent from static point-ROM models.0/4207 native
receipts,0/36images; native parity/provenance gate NOT PASSED. Three existing1×1
reference recipes are absent; do not patch replay-derived asset exceptions.
Stopped under standing host correctness rule: no ARM build/device transfer/run or
attract test. Need a defined ROM-only static-direct catalog or bounded direct-material
approach before rerunning host validation; then hardware gates remain outstanding.
Actual≤200MiB GPU residency, preparation improvement, state/attract fidelity and
course/reason coverage are unverified. No G2 work started. Production/47files exact,
stock clocks and EmulationStation verified, no game running; no commit/push/deploy.

## Milestone C1e — device NO-GO / STOPPED (2026-10-16)

See [C1e report](NATIVE_C1E_REPORT.md) and
[evidence](benchmarks/native-c1e-20261016/README.md). Both stock index play runs:
zero inferred repeats, settled audio underruns and main gaps>50ms. Mali fidelity:
819 residual pixels, all adjacent-source class(b), class(c)=0; all21 sets and4207
adjusted receipts pass. Atlas-associated Mali allocation falls459.34→58.76MiB
(7.817×); settled total Mali539→95–96MiB, legacy-overlap peak596→196MiB.
**Fenced world+HUD p95 fails:** index240MHz16.271ms,420MHz16.263ms versus14ms.
No conditional GO; GPU execution versus driver/presentation dependency unresolved.
Index compiler time increases slightly; no optimisation performed. Private native/c1e
contains reproducible builds, capture/debug-memory refinement, safe runners and analysis.
All raw telemetry and failed setup attempts retained; ROM-derived readbacks remain ignored.
User reports colour replay looks good. Production/47 protected files unchanged;
CPU/GPU bounds/governors and RT defaults restored, EmulationStation running, no game.
M3/P3 gates remain unchanged. No commit/push/production deployment. STOPPED.
Further profiling, texture changes or product work requires separate authorisation.

## Milestone C1d — host fidelity PASS / device-ready / STOPPED (2026-10-16)

See [C1d report](NATIVE_C1D_REPORT.md) and
[evidence](benchmarks/native-c1d-20261016/README.md). User withdrew the exact-layout
control; no alias-separated control is built or required. C1d adopts matching
lowp palette precision in its private index derivation (earlier sources retained).
ENG_C1_DEBUG=provenance records actual bank/U/V/material in an integer target.

All5875 residuals are class(b), adjacent source texels in the same material/bank;
class(a)=0,class(c)=0.2777horizontal/3090vertical/8diagonal; all inside original UV
bounds, no gutters/wrap required. All-world material/bank mismatches0. Debug colour
proofs match normal rendering exactly and11059200 sampled ROM/palette colours pass.
Four full6006-tick host replays each pass21sets/4207adjusted commands; both RGBA runs
also retain4207original packets. Normal36colour images match C1b/C1c baselines.
No source-identity exception, tolerance, class(c) fix or replay retry was needed.

**Ready for unchanged C1b Part2 device go/no-go; DO NOT START without authorisation.**
Host gate does not pass H700 timing/audio/memory targets or historical M3/P3 gates.
Only native/c1d plus docs/evidence changed; no SSH/device transfer/run, clocks,
services, production/shared engine/saves/replays/controller changes, commit or push.
STOPPED at the host readiness gate. Further device work requires explicit approval.

## Milestone C1c — host attribution BLOCKED / STOPPED (2026-10-16)

See [C1c report](NATIVE_C1C_REPORT.md) and
[evidence](benchmarks/native-c1c-20261016/README.md). Normalised usampler texture()
produces exactly the same36images as C1b integer-fetch/highp. Matching RGBA's
lowp palette qualifier removes846527/852400 original differing pixels;5873 remain
plus2new residuals. Residual adjacency is unproven; no pixel is waived as acceptable.

Literal layout control:22/36images exact,426RGB differences/max145. All3 new host
replays pass21sets/4207adjusted commands; control also4207original raw packets.
Original RGBA colour dedup aliases incompatible pen indices:5244physical texels
have no common pen satisfying shared palettes (3438inside at least one footprint).
Witness page4/(1927,3897),RGB38/51/76: material635/group1 needs pen3;4664/group2
needs pen4. A source-exact shared-layout pen control needs separated resources or
remapping; current conflicted pack is diagnostic only. No redesign was implemented.

All36compact side-by-sides/overlays and six reviewed contact sheets retained in
native/generated/c1c/review; complete locations/classification limits in evidence.
Gate NOT PASSED; do not proceed C1b devicePart2. Recommend a separately authorised
control revision, then exact residual source-texel provenance; device targets unchanged.
Host only: no SSH/device transfer/run/clock/service or production changes, no commit
or push. P3/C1b sources remain intact. STOPPED pending explicit authorisation.

## Milestone C1b — sizing PASS / host fidelity NO-GO / STOPPED (2026-10-16)

See [C1b report](NATIVE_C1B_REPORT.md) and
[evidence](benchmarks/native-c1b-20261016/README.md). ROM-only compact pen atlas:
2,421 models / 97,116 quad packets / 21,783 footprints; whole ROM seven pages,
112 MiB, reference 58.69 MiB (7.79× including one palette versus458.35 MiB RGBA).
All5,407 reference materials/120,048,149 texels match exactly including gutters.
Sizing gate PASSES. Runtime point-RAM/direct/dynamic UV counts remain unknown;
three reference footprint keys are absent from static ROM enumeration.

Private native/c1b selectable prototype derives from P3 b2; production/shared
engine unchanged. Both host6006-tick replays pass21sets; RGBA retains4207original
packet hashes and36prior host images;4207pre-batch command hashes match after
zeroing page. Index raw packets differ by atlas page grouping/material UV mapping.
**Fidelity fails:0/36 exact,852400 differing RGB pixels,alpha exact,max error162.**
All side-by-sides retained under native/generated/c1b/image-review with manifest.
No per-pixel cause is proven, so STOP under the standing correctness rule.
No ARM/device replay, smoke, timing, fence or420MHz run. Device performance and
measured residency are unmeasured; do not treat sizing success as a renderer GO.

Read-only final device check: production SHA
`6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`,
ES PID14605, CPUs schedutil480–1512MHz/GPU simple_ondemand, no diagnostics running.
No device changes needed restoration. No commit/push/deployment.
STOPPED. Focused host fidelity attribution requires separate authorisation.

## Milestone C1 — BLOCKED / host design decision / STOPPED (2026-10-16)

See [C1 report](NATIVE_C1_REPORT.md) and
[evidence](benchmarks/native-c1-20261016/README.md). Direct texture-map flattening
cannot fit the specified single 4096×4096 plane: the ROM map has sixteen distinct
bank planes (256 MiB expanded). Reference recipes span fifteen banks (240 MiB);
with palette this gives only 1.909× reduction versus 458.35 MiB RGBA, below 10×.
Nonzero-bank reference recipes return different pens at the same original UV.
Audit decodes/hashes all banks and passes 65,536 independent scalar checks.

No replacement renderer, cache or runtime switch implemented; no new replay,
packet/image validation, GPU timing or device clock change. P3 is unchanged.
A revised compact addressing representation requires a decision; do not silently
substitute a 256 MiB array or repeat the rejected three-lookup shader.
Read-only hardware check: production SHA unchanged
`6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`,
ES PID14605, CPU schedutil/GPU simple_ondemand. No commit/push/deployment.
STOPPED. Further design/implementation requires explicit authorisation.

## Milestone P3 — COMPLETE / performance FAIL / STOPPED (2026-10-16)

See [P3 report](NATIVE_P3_REPORT.md) and
[evidence](benchmarks/native-p3-20261016/README.md). Selected isolated build is
P2 b + B1 checked projection + B2 conservative extrema, default priorities:
`native/generated/p3/b2-play/arm/rr_p1b`, SHA256
`0fdf81922672ee47be0c71cba245c5e6da841328de86a1d8b92310988ec40817`.
Corrected ENG_* smokes verify applied policies. Nice lacks convincing overall
benefit; RR worsens worker waits, repeats and audio; both are rejected.

Both final dense preparation p99 values pass: 13.705/13.492 ms. Both have zero
settled underruns and main gaps >50 ms. Final-2 has one inferred repeat at tick
1800, so the zero-repeat gate FAILS. First-native draw service is 25.779 ms versus
5.318 ms presentation-thread CPU; the exact non-CPU wait remains unisolated.
The transition stays inside acceptance. No further optimisation was attempted.

Exactly one final device validation passes all 21 checkpoint sets, 4,207 packets
and 36 Mali images. Host projection proof covers 128449792 UBSan comparisons;
B2 has differential and full-replay parity. The 929.43-second, seven-process-lap
soak has zero repeats, settled underruns or main gaps >50 ms, peak 64.133°C,
CPU 1512 MHz, GPU 240 MHz and cooling states zero. Native exposure is 493.18 s;
this is a 15-minute wall-time replay loop, not 15 minutes of native-only play.
User confirms settled motion/audio clean. The old blocked attempt is retained
as history in the report/evidence; its invalid A runs are not comparisons.

Continuation used 48.94 minutes of device time. All 47 protected hashes pass;
production SHA remains
`6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`.
CPU schedutil 480–1512 MHz, stock GPU policy and RT defaults restored; EmulationStation
PID 14605 restored, no game/sampler/perf running. No shared engine, gameplay,
production, saves, replay or controller changes; no commit or push.
STOPPED. Any further investigation or implementation requires explicit authorisation.

## Milestone P2 — COMPLETE / performance FAIL / STOPPED (2026-10-16)

See [P2 report](NATIVE_P2_REPORT.md) and
[evidence/methods](benchmarks/native-p2-20261016/README.md). Isolated final build
is `native/generated/p2/b-play/arm/rr_p1b` (`v2-b-play` on the diagnostic device),
SHA256 `7f313d01fd5c76c8a0af9babc4e60a794968a85f01fca086b829bf242fccf5c3`.
Selected: complete play/validation split, exact palette revision/material cache,
and selective palette gathering. Metadata, guarded Z reuse and permutation sorting
preserve exact output but lack net measured benefit and are rejected for adoption.
Private derivation/reproduction scripts are under `native/p2`; production unchanged.

Fixed CPU1512MHz, GPU stock, queue2/valveOFF/O2/stack protection/no affinity.
Nine headline runs and all five separate compiler experiments completed. Final
headline dense prep p99 is14.310/14.739/14.811ms: all exceed14ms, gate FAIL.
All three final runs have settled underruns0, main gaps>50ms0 and inferred repeats0;
full-native carry-in remains776/898/924 audio sample frames. All corrected validation
builds pass6006 ticks/all21 checkpoint sets/exact4207 packets/36 Mali images.
All46 complete hardware replays (including attribution/preliminary/soak) independently
pass authoritative state; play builds intentionally omit packet/image hashing.

Continuous1852.85s/14-process replay-loop soak:986.39s native exposure,3 inferred
repeats,2131 settled underrun frames and one64.068ms main gap at lap11/tick3966.
Actual game PID/stacks captured; simulation62.793ms, no queue block, short prepare/draw.
Bracketing samples miss the instant; exact CPU/scheduling cause UNKNOWN, with no
sampled reclaim-counter increase. Lap13/tick3515 is a presentation-side late event.
Peak67.778°C, CPU1512MHz/cooling0 throughout; GPU cooling active later. No CPU-triggered
1416MHz repeat required. User: “Looks and sounds good.” Quantitative soak gate fails.
No safe single-process reset exists; per-lap reload/reclaim counters are retained.

All47 protected hashes pass; production SHA remains
`6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`.
All35 independent sessions restore clocks/production hashes and ES. Final CPU
schedutil480–1512MHz/GPU simple_ondemand restored, ES PID3285, no game/sampler/perf.
No commits, pushes, production deployment, simulation/audio/fallback/coverage,
texture-format or display-timing changes. Historical P1/P1b/M3 gates remain unchanged.
STOPPED. Further optimisation, texture work, broader coverage and productisation
require separate user authorisation.

## Milestone P1b — COMPLETE / performance FAIL / STOPPED (2026-10-16)

See [P1b report](NATIVE_P1B_REPORT.md) and
[evidence/methods](benchmarks/native-p1b-20261016/README.md). Private native-only
asynchronous logging, offscreen three-copy warmup, gradual legacy cache retirement
and default-OFF valve are implemented under native/p1b. Stock clocks, affinity OFF,
O2, original gameplay/audio algorithms and fallback lookup/recreation are retained.

All three headline q2 OFF runs have zero native main gaps >50 ms and 0/0/1 inferred
repeats, but 1451/2364/1961 missing audio sample frames: zero-audio gate FAIL.
The nine-run matrix, observer run, separate syscall trace and 1867.31-second,
14-lap stock replay-loop soak are complete. All21 checkpoint sets/4207 packets
pass every replay. Soak:11 inferred repeats,45564 missing audio frames,one~50ms
main gap; peak64.862°C,no sampled cooling activity,GPU240MHz throughout.
Dense preparation and handover audio carry-in remain limiting. Soak observer
followed supervisor rather than child: global telemetry valid, game stacks absent;
coincident reclaim is not a proven cause of the remaining stall. Correct and
validate observer targeting before a future residual-stall reproduction.

Final47 protected hashes pass; production SHA remains6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e.
ES restored(PID14183),stock governors unchanged,no game/sampler/tracer running.
No commit,push or production deployment. Historical P1/M3 gates are unchanged.
P2/compiler/texture optimisation and further work require separate authorisation.

## Milestone P1 — COMPLETE under reduced scope / performance FAIL / STOPPED

See [P1 report](NATIVE_P1_PIPELINE_REPORT.md) and
[evidence/methods](benchmarks/native-p1-20261015/README.md). Private hybrid
main/rr-prepare/rr-present FIFO prototype, per-slot native/HUD state, rotating
GPU resources with reuse fences, no normal per-frame glFinish, legacy GPU
release/lazy fallback recreation. Accepted presentation-paced59.7104Hz policy;
O2 generated sources with O1 switch and stack protection, opt-in clocks,
affinity OFF. Production source/executable remains unchanged.

Host/ARM6006 ticks, all21 checkpoint sets, exact4207 packets;36 sampled Mali
images match per queue depth. Eight user-reduced runs completed (one normal and
one diagnostic fence per stock/both-clock × queue2/3); repeats and both30-minute
soaks deferred. Best observed normal both-clock queue2:16 inferred repeated
vblanks,199.391ms worst interval,17,284 missing output sample frames. No clean-audio
or zero-repeat pass. Queue2 snapshot→swap age~33ms versus queue3~50ms. Legacy
release saves~52–57MiB RSS, but transition MemAvailable does not rise; preparation
p99~18ms and GPU completion p95~16.1ms remain limiting. Multi-second stalls and
sampler pressure-read gaps prevent a complete cause claim; continuous ≥10Hz
coverage and the historical2.74s D-state root cause are not established.

Manual City racing/controls work, no observed corruption;9806 unsupported pictures
fall back through legacy without exiting. Audio stutters and long stalls remain.
This is not native City coverage. All47 protected files verified at shutdown;
stock governors restored, EmulationStation running, no game/sampler. Final analysis
retrieved all evidence without new tests. Detailed scope, caveats and hashes are
in the report. No production deployment, commit/push, P2 compiler/texture work or
further tests/soaks authorised. M3 remains historically performance-blocked.
**STOPPED; next work requires explicit user authorisation.**

## Stage 1b environment/build-flag audit — COMPLETE / STOPPED (2026-10-15)

See [headroom report](NATIVE_STAGE1B_HEADROOM_REPORT.md) and
[evidence/methods](benchmarks/native-stage1b-20261015/README.md).
Stock native GPU stays at 240 MHz. GPU-max saves ~2.96 ms median rendering;
CPU performance saves ~0.59 ms simulation, generated-source O2 a further ~0.86 ms.
Retain stack protection; affinity OFF. Flag timings use one run/configuration at
user request, so repeatability remains unproven. All 50 H700 replays pass 6,006
ticks/all 21 checkpoint sets; host/ARM variants preserve 4,207 exact packets.
Best-clock O2 model still misses 70/298 pictures at 10/20% contention. Panel is
**59.7104 Hz**, below arcade refresh (~one omitted presentation/5.16 s with vsync).
Some stock production runs show GPU thermal limiting. All 47 protected hashes
match; original governors restored, EmulationStation up, no game running.
Production unchanged; no commit/push. **M3 remains blocked. Stage 2/M4B and any
architecture/presentation/production implementation require explicit approval.**

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

## Historical: Milestone 2 focused revision authorisation (2026-10-15)

The user authorised amendment of M2 for the focused **offline native mesh/material
consolidation** proposal. Read the [revised scope and unchecked gates](NATIVE_RENDERER_ROADMAP.md)
and [latest progress entry](NATIVE_RENDERER_PROGRESS.md) before implementing.
This amendment is documentation only; the revised compiler/backend is not yet
implemented. The initial probe failure below remains valid historical evidence.

Use actual source UV footprints and palette state to compile deterministic RGBA
material groups with fresh native UVs/seams/gutters, then a genuine reusable
mesh/instance frozen-scene backend. Start with measured draw/memory feasibility
and a small terrain/car visual gate. Working targets: <=128 world draws and
<=128 MiB native asset residency; tick-2500 world completion median <=12 ms,
p95 <=14 ms across repeated real-H700 processes. Targets are prospective and
must not be presented as achieved results. Preserve priority/scissor/dynamic/HUD
coverage and source provenance. A projected-triangle replay is not the deliverable.

No runtime cache-atlas/array revival or dependent map/pen/palette shader. No
Milestone 3 scene protocol/live integration. Stop at a failed gate or completion;
no commits, push, deployment or production changes without separate approval.

## Native renderer Milestone 2 stopped FAILED/RETHINK (2026-10-15)

**Milestone 2 was authorised; its first measured design failed the headroom gate.
STOPPED. No Milestone 3, deployment, commit or push.** Milestone 1 remains complete.
Read [frozen-scene report](NATIVE_FROZEN_SCENE_REPORT.md),
[progress](NATIVE_RENDERER_PROGRESS.md), [roadmap gates](NATIVE_RENDERER_ROADMAP.md)
and [build/test commands](native/render/README.md).

- Separate `native/render/frozen.c` GLES2 resident **ceiling probe**, no emulator
  linked: 670 actual RGBA resources, 7,998 frozen vertices, persistent VBO/EBO;
  ordinary 2D material grouping reduces original 1,280 draws to 701.
- All three modes across all three final H700 processes match the actual
  tick-2500 Mali world RGBA exactly. Source inspection also validates 1,633 quads,
  6,468 raw vertices, 50 ROM models and 68 object/transform/view tuples.
- Final separate-buffer run medians: streamed original 28.494 ms, resident
  original 28.107 ms, grouped resident **23.409 ms completion**, 11.132 ms thread
  CPU, 11.467 ms submission. Thirty rotating mode triplets per process, no final
  samples discarded. No transfers while measuring. Explicit completion spans
  include feeding/waits; these are **not exclusive GPU timings or live FPS**.
- Resident modes do no per-frame uploads, texture allocation or geometry work.
  Crucially, timed input is already projected/clipped captured geometry, **not
  the final reusable local-space mesh/instance backend**. That backend and HUD
  composition were not implemented after the initial design failed the budget.
  Do not call this a completed native renderer or infer all native designs fail.
- Even this probe exceeds 16.693 ms without transforms, simulation, HUD or audio.
  Stop and propose a bounded offline native mesh/material consolidation design
  before continuing M2. No new packer/atlas/array/dependent sampling redesign was
  implemented; details distinguish the proposal from prior rejected experiments.
- Numeric evidence `benchmarks/native-frozen-20261015/`; game-asset outputs stay
  under `/tmp/rr-m2/` and device `/tmp/rr-m2-results-final/`. Side-by-side PNG
  `/tmp/rr-m2/analysis-final/side-by-side.png` (640x480 each, incumbent left).
  Keep original frozen capture, recordings and their sidecars untouched.
- Host llvmpipe candidate modes equal its own reference, but 941 pixels differ
  from Mali capture; portable equivalence remains unproven. Real Mali comparisons
  are exact. No full scene-set coverage, live performance or audio claims.
- Production executable hash remains
  `6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`;
  launcher/controller hashes unchanged, all backup checks pass, single Ports
  launcher retained. Final frontend PID **5664**, no game running, GPU HUD default,
  replay off. Private test binaries only in device `/tmp`; no OS/clock/settings
  changes, shared simulation/renderer edits, deployment or commit.

**Next authorised action: none. Revised Milestone 2 requires separate approval;
Milestone 3 is not authorised and M2's exit gate remains unmet.**

## Native renderer Milestone 1 completed (2026-10-14)

**Milestone 1 COMPLETE. STOPPED. Milestone 2 requires separate explicit user
approval; it has not started.** No commits, deployment or production changes.
Read [roadmap and checked exit gates](NATIVE_RENDERER_ROADMAP.md),
[progress/reproduction commands](NATIVE_RENDERER_PROGRESS.md),
[asset investigation](NATIVE_ASSET_INVESTIGATION.md) and
[native asset v1 format](native/assets/FORMAT.md).

- Isolated offline converter/inspector in `native/assets/asset_tool.py`; original
  ROM geometry, UV/material/lighting attributes, vertex/index buffers, actual
  palette RGBA bakes, versioned manifests, source provenance and SHA256 checks.
- Actual inventory: 2,421 models, 97,116 quads, 388,464 vertices, zero exclusions;
  112 material keys and 21,783 source rectangles. Static/dynamic transforms,
  palettes/solid overrides, point RAM and direct polygons documented.
- Terrain identity proven at actual replay tick 2500: course 6 / segment 10,
  tag 1211 -> executed DSP PC 0x4425 ADDK 69 -> object 1280. Incumbent geometry
  matches 50 emitted quads / 182 valid raw vertices with zero discrepancies.
  Private traced vs uninstrumented endpoint state and polygon RAM match exactly.
- Representative visible assets are terrain 1280 and racing car body 1945;
  **144 material recipes** validate. Earlier car-select tag 1765 was mistakenly
  treated as a direct ROM object ID; object 1765 is a flat panel, not the car
  representative. Preserve this correction when reading earlier pack evidence.
- Two final visible packs have matching manifest/payload hashes, manifest SHA256
  `954b624365e8a80773fa9e4de20a6d868260dff96392d786cfa5111752dd97eb`.
  Actual captured oracle: 670 resources / 6,524,539 texels, zero RGBA differences.
  Topology/indices/UV/material checks and malformed-input gates pass. Fresh native
  headless build/regressions and existing H700 playable build pass.
- Geometry buffers cost 11.11 MiB; full-inventory RGBA rectangles would cost
  2.02 GiB. Actual frozen scene uses 24.89 MiB RGBA. Proposed bounded 128 MiB
  asset residency is documented; complete track union, palette variants,
  native scene fidelity and Mali performance remain future unknowns.
- Generated packs `/tmp/rr-m1-visible-pack{,-repeat}`; previews
  `/tmp/rr-m1-visible-preview/`: `model-1280.png`, `model-1945.png`, `textures.png`.
  Preview tool `native/tools/preview_assets.py` is a CPU asset inspector, omitting
  game lighting/fog. It is not the Milestone 2 frozen-scene GLES renderer.
  ROM-derived outputs remain outside Git (repository outputs must use ignored
  `native/generated/`). Preserve existing recordings, palettes and captures.
- Raw evidence `/tmp/rr-m1-visible-validation.log`,
  `/tmp/rr-m1-terrain-validation.json`, `/tmp/rr-m1-durable-trace/validation.json`;
  durable source trace/geometry comparison tools and commands are in the progress
  report. `/tmp` artifacts are local and may need regeneration after cleanup.
- Read-only device verification succeeded after authentication: production hash
  `6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`
  matches the accepted GPU HUD playable. EmulationStation PID 3154, no game,
  one Ports Rave Racer launcher, all backup checksums pass, controller config
  matches backup, GPU HUD `default`, replay `off`; reference replay/sidecars
  present. Later asset closure/preview work was host-only. Recheck live state
  before future device testing; no new performance baseline was measured.

## GPU-native world texture sampling gate completed (2026-10-14)

Read [GPU_NATIVE_TEXTURE_INVESTIGATION.md](GPU_NATIVE_TEXTURE_INVESTIGATION.md)
and [isolated measurements](benchmarks/gpu-native-20261014/summary.json).
**Retain production; stop the measured three-dependent-lookup prototype.**

- Starting HEAD `1c40c2f`; production executable hash unchanged. Standalone
  diagnostics only; no renderer integration, deployment or commits.
- CPU source/palette model reproduces all 6,524,539 allocated texels in the
  preserved 670-resource tick-2500 scene. Raw and pre-oriented integer GPU
  layouts both match its Mali world framebuffer with zero differing pixels.
- Thirty alternating warmed pairs per layout: raw completion median 81.002 ms
  versus RGBA 37.530; oriented 77.942 versus RGBA 37.371. These include command
  feeding and explicit diagnostic completion; they are not exclusive GPU
  shader times or full-game FPS. All source uploads precede measurement.
- Oriented pens/map require 20.0625 MiB padded, plus 128 KiB palette. Immutable
  ROM data does not remove the need for snapshot-consistent palette updates.
- Native llvmpipe differs at three pixels from its own RGBA reference; source
  texels agree. Boundary/interpolation fidelity remains unresolved. Exhaustive
  targeted tests, dynamic updates, batching and live five-pair benchmarks were
  not entered after the performance stop. No broader correctness claim.
- Source/tools and raw measurements are preserved in the linked report. ES
  restored (PID 6271), production hash verified. Further work needs a materially
  different justified sampling approach, not speculative live integration.

## Mali-G31 texture-array driver investigation completed (2026-10-14)

Read [MALI_TEXTURE_ARRAY_DRIVER_INVESTIGATION.md](MALI_TEXTURE_ARRAY_DRIVER_INVESTIGATION.md)
and [archived measurements](benchmarks/mali-texture-driver-20261014/README.md).
**Retain production; reject bounded cohort retention.** No production changes.

- Source baseline HEAD `41fb382`; production reference `00d2b73`, executable
  SHA256 `6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`
  unchanged. Original atlas/source remains an independent OFF-default fallback.
- The larger upload bucket includes reaping/deletion that production charges
  to eviction. Tick-1965 diagnostic: reaping 22.22 ms, search/create 5.23 ms,
  texel upload 14.46 ms; storage API only 2.46 ms. Expensive deletions occur
  after retirement fences signal, so earlier reads of those allocations do
  not explain their cost. Controlled actual-payload same/other/unrelated
  in-flight updates do not show mandatory whole-prefix waits; internal driver
  copies/renaming/bookkeeping remain unproven.
- One private OFF-default retention flag keeps completed empty two-layer
  storage within the same 128 MiB physical cap. Native + Mali 1,880-picture
  gates, exact state/PCM/voices, all 12,036 ordered misses and TSan 566-picture
  gate pass. Idle storage is searched before pending retirements; accepted
  normal runs have zero blocking fence polls.
- Five accepted alternating three-way racing windows: production/original/
  retained FPS 52.900 / 55.957 / 55.801; 1% lows 24.899 / 22.151 / 21.486;
  p99 32.685 / 33.807 / 35.648 ms; median worst 79.034 / 101.651 / 101.133 ms;
  peak RSS 286.961 / 334.145 / 393.242 MiB. Retention reduces cohort births
  7494 → 6320 but worsens lows/p99 in every pair and leaves the burst intact.
- Accepted rounds 1/3/4/5/6 replace the entire transfer-confounded round 2.
  Round-one production has whole-process audio underruns and pre-window
  gaps at 120/1740; report discloses this, rather than claiming all processes
  have zero audio. All accepted racing windows have zero resyncs; all retained
  processes have zero audio underruns. Do not transfer artifacts during tests.
- Stop after this one remedy; no larger pool/new allocator or renderer redesign.
  Driver API durations are CPU wall, not exclusive GPU execution. Cap/RSS
  estimates overlap on shared memory; private driver allocations are unknown.
- Device artifacts remain under existing
  `/userdata/system/rave-racer-world-atlas-20261014/driver-*`; host
  `/tmp/rr-mali-array/`. Archives and standard-library tools are preserved.
  Final checks: no test/game processes, one Ports launcher, backup hashes
  unchanged, frontend restored with diagnostic environment cleared (PID 3015).
  No commits, deployment or rollback needed.

## H700 production GPU HUD integrated and deployed (2026-10-14)

Read [GPU_HUD_PRODUCTION.md](GPU_HUD_PRODUCTION.md) for final configuration,
regressions, [confirmation benchmark](benchmarks/gpu-hud-production-20261014-summary.json)
and rollback instructions. The user confirmed original HUD/gameplay, steering,
controller operation and audible synchronised sound.

- `playable-knulli`: GPU HUD/default glyph-quads ON, CPU fallback selectable;
  all validated three-stage/geometry/memory/C352/tile-run flags preserved;
  batching, pre-decoding and wake experiment OFF.
- Installed executable SHA256
  `6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`.
  Existing launcher uses `gpu-hud-mode=default`, recording OFF, exact controller
  mapping and audio ON. Source changes remain uncommitted.
- Native full replay: 2,002 RGBA pictures exact against CPU; 46 frozen/synthetic
  cases pass. ThreadSanitizer through 1700: no application race, exact pixels,
  state/PCM/voices and checkpoint prefix (existing Mesa-only suppression).
- Final ARM64: 2,002 fixed pictures plus 913 adaptive/live-audio pictures exact;
  all 63 recording state records match; final pixels, geometry, misses, PCM and
  voices match pre-GPU references; CPU/serial fallback checks pass. Verification
  readbacks cause one diagnostic resync, excluded from performance claims.
- Two alternating final confirmation pairs: accepted GPU prototype 52.728 FPS,
  consolidated 52.939; 1% low 25.214 / 25.297; p99 32.802 / 32.657 ms;
  image age median 38.706 / 37.370 ms. Every normal benchmark run has zero audio
  underruns and resyncs, simulation ~59.904 Hz. Prior five-pair 52.714 FPS benefit
  retained; tiny consolidation difference is not a new optimisation claim.
- Fresh previous GPU playable/config backup:
  `/userdata/system/rave-racer-backups/20261014-pre-consolidated-gpu-hud/`.
  `restore.sh` restores the manually accepted GPU trial. Older CPU and
  pre-parallel-prep backups/hashes untouched. Mode `off` selects original CPU
  HUD in the new binary without changing controller configuration.
- Device artifacts `/userdata/system/rave-racer-gpu-hud-consolidated-20261014/`;
  validation `validation-20261014-152511-6403`, benchmark
  `ab-20261014-153427-7324`, startup `deployed-startup.log`; host
  `/tmp/rr-hud/consolidated/`. All preserved.
- Last final check: EmulationStation PID 23050, no game running, one active
  `/userdata/roms/ports/Rave Racer.sh`. No commits.

## Earlier GPU HUD interactive trial (superseded by production integration above)

Read [GPU_HUD_PROTOTYPE.md](GPU_HUD_PROTOTYPE.md) and
[the five-pair summary](benchmarks/gpu-hud-20261014-summary.json).

- Default-OFF `RR_GPU_HUD` adds compact snapshot metadata, a lazy glyph atlas,
  one tile-quad batch into an indexed HUD target, and palette/shadow/priority/LUT
  composition. No world geometry/cache changes. Runtime selectors are
  `ENG_GPU_HUD=1 ENG_GPU_HUD_GEOMETRY=1`; unset/off retains the original CPU path.
- Five alternating pairs against the installed multicore binary: median FPS
  40.420 → 52.714 (+30.4%); 1% low 19.134 → 25.151; p99 41.035 → 33.027 ms;
  median worst 85.466 → 79.755 ms; image age median/p95
  50.585/67.119 → 37.945/55.108 ms. Simulation remains ~59.904 Hz.
- HUD preparation 11.070 → 0.048 ms; HUD upload/update CPU 5.538 → 1.003 ms;
  GLES-owner CPU 23.265 → 17.013 ms. Peak RSS 293.836 → 288.047 MiB.
  All ten runs have zero audio underruns and match endpoint JSON.
- Fixed-through-6006 verification: 2,002 pictures byte-exact, same final capture,
  geometry, texture miss sequence, board, PCM and voices. Adaptive live-audio
  verification: 2,078 selected pictures exact through 5639; normal CPU fallback
  matches PCM `a18439c518e08e3d`, voices `c12e04d613b280fc`, endpoint state.
- Both architectures passed 14 captures + 32 synthetic cases. Direct packed
  tilemap shader had ~9.18 ms warmed span versus ~3.30 ms reference; retained
  glyph-quad final pass is ~2.775 versus ~3.276 ms at racing tick 2502. Dirty
  elapsed queries were disjoint and rejected; no exclusive GPU time claim.
- A fake early mismatch was captured as the EmulationStation Ports screen: its
  asynchronous restart raced the next test. Intermediate pens were correct.
  The durable benchmark waits for wrapper + child shutdown and rejects a
  competing frontend. Do not reintroduce the old conditional stop/start race.
- Device artifacts `/userdata/system/rave-racer-gpu-hud-20261014/`; host
  `/tmp/rr-hud/`. Headline run `ab-20261014-135149-14295`; guarded fixed run
  `geometry-validation-20261014-134641-14211`; live
  `live-verify-20261014-141046-19437`; fallback/audio
  `final-checks-20261014-141928-20309`. Preserve captures and diagnostics.
- Candidate `bin/rr_hud_geometry_capture`, SHA256
  `fef8f66dc7e87c18668a32b5c43f473d14c98b9323cb9236d47380700ce32b52`.
  Deployed to the existing Ports → Rave Racer launcher at the user's explicit
  request for physical controller/audio validation. GPU glyph-quads and the
  validated three-stage production optimisations are enabled; batching and
  texture pre-decoding are compiled OFF. Verification/capture diagnostics are
  disabled. `gpu-hud-mode=on`, `replay-mode=off`.
- Prior executable (SHA256 `77536f56ad17de9cf8772175c73442d9cb246d87aeb6fbf6c8d7f07c3d6ab53e`),
  exact launcher and mode configuration are preserved under
  `/userdata/system/rave-racer-backups/20261014-pre-gpu-hud-interactive/`.
  Exit the game with START+SELECT and run that directory's `restore.sh` over SSH
  to restore the previous playable executable and configuration. The earlier
  pre-parallel-prep backup remains untouched.
- Launcher explicitly loads the exact built-in RG40XX-H controller mapping from
  `controller-input.cfg`; startup confirms recognition, three-stage mode,
  glyph-quads with verification OFF and zero audio underruns. ES restored.
- Physical controller, HUD/menu, audio and full-race validation remain pending
  user confirmation. This is an authorised interactive trial, not acceptance
  as the permanent default. One Ports launcher; no commits or optimisation work.


## Texture preparation investigation completed (2026-10-14)

Read [TEXTURE_PREDECODE_INVESTIGATION.md](TEXTURE_PREDECODE_INVESTIGATION.md)
and [the complete measurements](benchmarks/texture-predecode-20261014-summary.json)
before further texture-cache work. Batching remains OFF; no more batching work.

- Actual HEAD at start: `309e053` (`Multitexture followup`). No commits made in
  this investigation. The working tree adds default-OFF CPU pen-bake/complete-ROM
  preparation and bounded private GPU preload diagnostics, plus tools/reports.
- Fresh five-pair deployed/newer-unbatched reconciliation: 41.090 / 40.953 FPS.
  Historical 34.48 FPS does not reproduce as a build regression. Long historical
  swap scopes and simulation pauses are documented; simple 256 MiB `/tmp`
  occupancy was tested and did not explain it. Initiating historical cause is
  unresolved. Audio anomalies correlate with simulation gaps, often before the
  racing window; whole-process counters must not be labelled racing-only.
- Final five pairs compare the **installed multicore executable directly** with
  startup-prepared pen bakes: FPS 41.278 / 41.122, 1% low 19.853 / 23.142,
  p99 40.313 / 37.287 ms, median worst 104.110 / 71.758 ms. All five improve
  low/worst pacing; average FPS is unchanged. Additional RSS ~110 MiB, startup
  ~3.55 s preparation; active GPU budget remains 64 MiB. Every endpoint matches.
- Complete replay through 6006: 14 selected HUD frames + final capture identical,
  texture semantics `2a6578ef363d13f3`, PCM `0c24e8fca4b6400e`, voices
  `b76f90445ba87e18`, board `f9154a83`. 136,188,011 live RGBA texels checked
  byte-exactly. Corrected code-generation candidate validated separately.
- Initial same-binary A/B had a compiler artefact: verification's second call
  stopped inlining the ordinary tile-run decoder. It was corrected with a flag-
  gated always-inline attribute; final deployed-baseline pairs supersede it.
- Current Rave game table does **not** enable conditional Sys22 map fixup.
  Complete-ROM representation needs 65,747 combinations, 32-bit map indices,
  20.05 MiB resident. A 16-bit attempt safely did not activate and is excluded.
  Corrected raw/row/frame verification passed; one probe saved ~6 ms of first-
  burst decode, with weaker hitch improvement. No five-pair claim for that mode.
- Independent 8 MiB GPU preload: exact live-RGBA equality required before adopting
  unused private objects. 80 valid objects used; deliberately corrupted data
  rejected and normal uploads/pixels/state preserved. One GPU-only probe reduced
  upload scope 23.548 -> 1.348 ms but did not improve mean FPS. Not production.
- Persistent-cache feasibility sample: source SHA-256 + decoder/version/mode,
  canonical keys and CRCs, ~110 MiB raw -> 33.5 MiB gzip. Handheld cold/warm read
  + inflate medians 2.227 / 2.115 s, excluding validation/allocation/import.
  No learner/loader implemented. Production must learn/derive generic keys,
  retain missing-entry fallback and preserve actual GPU tier/palette selection.
- Stop with `RR_TEXTURE_PREDECODE_EXPERIMENT=OFF` by default. No deployment,
  extra Ports launcher or commits. Known executable and backup hashes unchanged;
  EmulationStation restored (last verified PID 27340), no test game running,
  `/tmp` 16 KiB, pressure diagnostic removed. Existing captures/tools preserved.
- Device: `/userdata/system/rave-racer-predecode-20261014/`; host:
  `/tmp/rr-predecode/`. Final headline run `final-ab-20261014-092916-29756`.
  Report names all exact validation/probe directories and reproducible flags.

## Current state (2026-10-13)

- Repository: `BennyWebb/namco22-decompile`, branch `feature/knulli-h700`.
  Latest checked commit: `31a8a67` (`Multitexture issue investigation`).
  The current working tree contains the default-off four-texture prototype,
  validation tools and follow-up report; check Git status before new work.
- Target: Anbernic RG40XX H, Knulli Scarab, AArch64 H700/Cortex-A53,
  Mali-G31, 640×480, SDL2 `mali` framebuffer driver, GLES2, glibc 2.40.
  Matching Buildroot host toolchain:
  `~/dev/knulli-scarab/output/h700/host/`.
- The normal playable build has original 3D geometry/textures, original
  2D/HUD, audible audio, controller input, an optional performance overlay,
  deterministic input recording/replay, and three-stage multicore presentation.
  The user tested HUD, audio and controls on the device and reported that
  controls felt responsive. Do not describe it as a 60-FPS renderer.
- CMake preset `playable-knulli` enables the validated WRAM reader, C352,
  ordered submission, geometry index/projection, threaded presentation and
  texture-row optimisations. The additional `RR_PARALLEL_FRAME_PREP` flag is
  **OFF by default** in source; explicitly set it `ON` to reproduce the active
  multicore binary. `RR_PARALLEL_FRAME_PREP` requires
  `RR_THREADED_PRESENTATION=ON` and `RR_GEOMETRY_INDEX_ORDER=ON`.

## Build and deployment references

From the repository root, build the current multicore ARM64 configuration in
a separate directory, leaving the installed device executable untouched:

```sh
cd raverace
cmake --preset playable-knulli -B /tmp/rr-multicore-arm64 \
  -DRR_PARALLEL_FRAME_PREP=ON
cmake --build /tmp/rr-multicore-arm64 -j4
file /tmp/rr-multicore-arm64/rr_gles2
readelf -d /tmp/rr-multicore-arm64/rr_gles2 | grep NEEDED
```

The preset uses `raverace/cmake/knulli-aarch64.cmake` and the matched
Buildroot sysroot. Do not substitute Ubuntu's generic AArch64 glibc. Native
development uses `cmake --preset playable-native` from `raverace/`; set the
parallel flag explicitly there too. See [multicore architecture and exact
validation](MULTICORE_RENDERING_PIPELINE.md) before deploying or rerunning
benchmarks.

On the handheld, the sole normal EmulationStation entry is
`/userdata/roms/ports/Rave Racer.sh`. It launches
`/userdata/roms/ports/rave-gles2/rr_gles2`; its persistent log is
`/userdata/roms/ports/rave-gles2/rave-racer.log`. The previous known-good
executable and launcher were preserved under
`/userdata/system/rave-racer-backups/20261013-pre-parallel-prep/`. The active
binary's recorded SHA-256 is
`77536f56ad17de9cf8772175c73442d9cb246d87aeb6fbf6c8d7f07c3d6ab53e`;
verify before relying on that identity. Do not overwrite the backup or
clean user replay/EEPROM data. Last known SSH address was `192.168.1.69`;
check reachability and that no game is running before device work. Restore
EmulationStation after testing.

## Reproducible workload and measured baseline

The primary representative workload is the player's complete first-lap
recording at
`/userdata/system/rave-racer-replays/race-20261013-122242-25136.rec`.
Measure ticks **1800–5639**; metadata, original EEPROM image, endpoint and
state checkpoints are sidecars beside the `.rec` file. The dense-opening
scripted window (ticks 1400–1700) remains a historical stress test. The
scripted 1400–3600 run has a wall-facing tail and is **not** representative
racing FPS. See [recording and replay](INPUT_RECORDING_REPLAY.md).

Five alternating physical replay A/B pairs, same traced mode and original
HUD/audio enabled, measured the bounded three-stage build against the
previous threaded presentation path:

| First-lap ticks 1800–5639 | Previous | Three-stage |
|---|---:|---:|
| Median rendered FPS | 33.431 | 41.279 |
| Median 1% low FPS | 17.890 | 20.624 |
| Median p99 presentation interval | 46.521 ms | 39.159 ms |
| Median largest hitch | 92.670 ms | 101.590 ms |
| Simulation rate | 59.904 Hz | 59.904 Hz |
| Presentation-thread CPU/picture | 29.364 ms | 22.532 ms |
| Preparation CPU/picture | — | 14.767 ms |
| Snapshot age to swap-return | 38.505 ms | 49.847 ms |
| Peak process RSS | 277.926 MiB | 293.172 MiB |

The split improves median FPS by 23.5% and the 1% low by 15.3%, while raising
software image age by about 11.3 ms. That image age is not measured
controller-to-photon latency. The rare texture burst near tick 1965 remains;
worst-frame latency did not improve. The GLES-owner thread is still the
largest serial rendering stage, about 22.5 ms CPU per picture in the matched
run, so this split alone cannot sustain 59.904 rendered FPS. No reliable Mali
GPU utilisation counter or `perf` executable was available on the device.

The user's later interactive session ran 23,604 emulated ticks, exited
normally, and reported correct HUD/audio and responsive controls. Its final
log reported zero audio underruns and emulation faults, 59.9049 simulation Hz
and 47.9843 rendered FPS across a different player-driven route. Do not compare
that overall FPS directly with the first-lap window.

The detailed A/B logs, timestamp CSVs and `/proc` samples remain under
`/userdata/system/rave-racer-multicore/`, especially
`ab-20261013-162808-17838` (final five pairs),
`replay-correctness-20261013-164429-22215` and
`diagnostic-20261013-165148-22678`. Use
`raverace/tools/RaveRacer-Multicore-Replay-Benchmark.sh` and
`raverace/tools/analyse_multicore_replay.py` as documented in the multicore
report. Preserve equivalent instrumentation when comparing FPS.

## Latest frame pacing experiment (2026-10-13)

See [frame pacing and image age experiment](FRAME_PACING_LATENCY_EXPERIMENT.md).
The new `RR_PREP_WAKE_ON_SNAPSHOT` CMake flag is OFF by default and requires
`RR_PARALLEL_FRAME_PREP=ON`. It lets the preparer wake when a fresh snapshot
arrives within the existing 6 ms grace period. Five alternating handheld
first-lap A/B pairs measured 41.262 versus 41.544 median rendered FPS, 20.411
versus 20.655 1% low FPS, 39.296 versus 39.364 ms p99 presentation interval,
and 49.126 versus 49.338 ms median image age (prior wait versus wake-up).
Median worst hitch worsened from 96.078 to 100.550 ms. The change moves about
0.98 ms from READY wait to PREPARED wait, without a meaningful latency or
pacing benefit. **Do not deploy this experiment based on these results.**

The installed playable executable and prior backup are unchanged. The active
binary hash above was reverified; EmulationStation was restored and the sole
Ports launcher remains `Rave Racer.sh`. Same-GPU replay frames 1965 and 3600,
all recorded checkpoints through tick 6006, native fixed-cadence texture event
digests and ThreadSanitizer checks passed. The five-pair data is under
`/userdata/system/rave-racer-multicore/ab-20261013-172218-26240`, and the
separate scheduler pair under `ab-20261013-174532-8107`. The GLES owner
remains the throughput limit; first-time texture bursts still dominate the
worst hitches. No repository commit was made.

## Ordered GLES2 submission investigation (2026-10-13)

See [ordered multi-texture experiment](GLES2_ORDERED_MULTITEXTURE_EXPERIMENT.md).
The first-lap diagnostic trace found 860.83 draws per presented picture;
contiguous four-texture groups could reduce this to 207.58 while retaining
scissor and mutation barriers. Mali-G31 reports 16 fragment texture units.
A bounded four-sampler prototype cut short-window submission time, but the
full-lap equivalent tick-2500 capture differed at 91 pixels. Shader-only
reproduction identified the fragment path; a straight-line sampler variant
still differed in batched mode. **The experiment failed the pixel-identity
gate, was removed from source, and was not deployed.** The alternating test
stopped at pair 2 rather than collecting five performance pairs for an invalid
candidate. Only the diagnostic trace behind `RR_GLES_SUBMISSION_PROFILE`
remains in the working tree. The active device executable and backup were
preserved; verify their hashes and EmulationStation before the next device
session. No commit was made.

### Shader-equivalence follow-up (2026-10-13)

The original deterministic tick-2500 pair is
`/tmp/multi4-ab-20261013-182049-28305/{baseline-2,candidate-2}.ppm`.
`candidate-2.ppm` is identical to `/tmp/shader-2500.ppm` (multi-sampler
shader, batching disabled). Pixel-by-pixel comparison finds 91 of 307,200
changed pixels, in two thin diagonal bands on the brown road-side surface
(x=299–526, y=218–254). Changed channel samples: R=90, G=78, B=27;
maximum absolute errors R=44, G=40, B=30; mean absolute error among changed
samples R=5.36, G=4.68, B=7.22. Whole-frame mean absolute errors are
R=0.001569, G=0.001188, B=0.000635. Use the untracked
`tools/compare_multitexture_capture.py` for the full signed histogram.

A temporary diagnostic build captured pre-compositor scene RGBA in
`/tmp/shader-split-20261013/{scene4_off,scene4_shader}.rgba`. Its 91 changed
pixels map exactly to the final capture after vertical flip. Scene differences
are RGB only (R=90, G=78, B=27, A=0); all changed pixels have alpha zero in
both variants. White-batch rendering is pixel-identical, so the mismatch
requires the real texture sample path. The original shaders use the same
`highp` UV precision, perspective division, texture multiplication, alpha
discard and priority-alpha handling; textures use `GL_NEAREST` and
`GL_CLAMP_TO_EDGE`. In shader-only mode all sampler uniforms address texture
unit 0. UV precision or nearest-texel selection remains a hypothesis, not an
identified cause.

The standalone probe in untracked `tools/mali_sampler_probe.c` ran on the
actual Mali-G31 (OpenGL ES 3.2, driver v1.r20p0). All 32 combinations of
conditional versus straight-line sampler selection, shared versus separate
texture units, slots 0–3 and two alpha priorities were RGBA pixel-identical
over 156,252 non-clear pixels. This synthetic case does not reproduce the
scene failure. A straight-line shader-only build with submission profiling
enabled also matched baseline, while another build without profiling differed;
same-binary `glFinish` toggles changed the mismatch pattern but never fixed
it. A fixed color adjustment would be unjustified. No precise shader cause or
correction has been established, and representative-frame equivalence has not
been demonstrated. Keep multi-texture batching disabled.

Temporary source/builds are under `/tmp/rr-shader-repro/`; local capture copies
are under `/tmp/shader-split-20261013/`, with device copies under
`/userdata/system/rave-racer-multicore/shader-split-20261013/`. They did not
replace the deployed playable. The installed executable hash verified before
testing was `77536f56ad17de9cf8772175c73442d9cb246d87aeb6fbf6c8d7f07c3d6ab53e`;
each completed device run restored EmulationStation. The final device-state
check was interrupted, so verify the hash and process state next session.
No credentials were stored in the repository, and no commit was made.

### Shader-equivalence resolution (2026-10-13, later follow-up)

See [tick-2500 resolution](SHADER_EQUIVALENCE_RESOLUTION.md). The earlier
fragment-shader attribution is superseded: polygons **978 and 1351** use a
history-dependent cached texture bake, either 594×80 / source-column step 1
or 298×80 / step 2, with different submitted U scales. Changing only these
resources using the same single-sampler shader reproduces every original
91-pixel RGBA pair exactly. The isolated four-triangle case changes 102 pixels;
11 are covered by later scene polygons. Applying captured coverage/order
reconstructs the original entire candidate scene byte for byte.

The replay's dropped presentations affect texture-cache residency. The sticky
higher-tier policy can therefore return different texture tiers at the same
simulation tick, despite an identical geometry digest. An off-mode diagnostic
run reproduced the original candidate, while another off-mode run reproduced
the original baseline. Do not treat a fixed replay tick as proof of identical
draw resources.

A frozen complete tick-2500 scene (**1,280 draws, 7,998 vertices**) passed exact
RGBA equivalence for original-program replay, conditional four-sampler and
straight-line four-sampler shaders, all samplers addressing texture unit 0.
Packed float32 UV/UVQ diagnostics also matched shader variants bit for bit for
fixed inputs. No Mali shader correction is supported or needed for this case.

The durable [standalone case](tools/shader_equivalence_case/README.md) contains
exact vertices, texture data, shaders and sample coordinates. Full captures and
the temporary renderer diagnostic patch are under `/tmp/rr-primitive-results/`.
Existing captures were preserved; prior temporary renderer sources were restored
and rebuilt. The installed playable hash above was reverified and
EmulationStation restored. No production change, deployment or commit occurred.

### Ordered batching follow-up (2026-10-13, latest)

See [bounded batching follow-up](GLES2_MULTITEXTURE_FOLLOWUP.md). The user chose
to preserve the intentional cache-history policy and validate exact draws while
auditing independently selected resources. A trace identified the road key's
cap-1024 bake at tick 2499 and subsequent budget eviction. Another normal-runtime
pair differed at 187 pixels, all covered by polygons whose U/V bake resources
differed; source signatures, positions, colours and depth matched.

The four-explicit-sampler prototype is restored behind
`RR_GLES2_MULTITEXTURE_BATCH=OFF` and runtime `--gles-multitexture off|shader|batch`.
It requires ordered submission; batch mode requires frame VBO upload. No cache
policy, polygon ordering, shader colour correction, extra thread or queue change
was introduced. Existing texture mutation barriers remain effective.

Correctness passed: the controlled four-unit case; 2,296 exact live-reference
scenes through replay tick 6006; twelve original-HUD captures and deterministic
simulation/board/PCM/voice/ordered texture events in off/shader/batch and the actual
feature-disabled build; and 650 exact constrained-cache scenes with 262 mutation
barriers, 41,618 uploads and 41,489 evictions. No pixel tolerance was used.

Five alternating first-lap pairs (ticks 1800–5639) measured median **34.476 versus
35.276 rendered FPS (+2.32%)**, 18.189 versus 19.064 1% low FPS, 43.491 versus
40.704 ms p99 interval, 105.257 versus 97.376 ms median worst hitch, 59.665 versus
57.897 ms median image age, and 73.968 versus 71.454 ms p95 age (baseline/batched).
Draws fell 878.938 → 213.337; submission wall time 11.219 → 7.414 ms. Peak RSS rose
295.031 → 296.430 MiB. Median simulation Hz was 59.898/59.887; median underruns
zero in both. Pair 1 had 80,229/3,243 underrun samples and a baseline 1.504-second
hitch; the other eight runs had zero underruns. No run was discarded.

The separate diagnostic measured complete submission CPU 11.302 → 7.117 ms, but
the shader alone added 1.645 ms to the median 3D GPU command span. Swap wall time
in the unprofiled paired test rose 4.081 → 6.834 ms. Three dense-opening pairs
improved FPS 48.407 → 52.964 (+9.41%) while 1% lows/p99 worsened slightly. These
results do **not** establish a substantial representative racing FPS increase.
They are contemporaneous comparisons; do not mix their absolute FPS with the
older handover benchmark. Instrumented GPU runs are not headline FPS evidence.

**Retain the validated prototype disabled by default; it was not deployed.**
The improvement is repeatable but modest. The installed playable and backup
hashes above were verified unchanged, EmulationStation restored, and the single
active Ports launcher preserved. No commit was created. Artifacts are on the
handheld under `/userdata/system/rave-racer-multitexture-resume-20261013/` and
locally under `/tmp/rr-multi-resume/`; summaries and reproduction commands are
linked from the follow-up report. Existing captures/tools remain preserved.

## Correctness and next-session guardrails

The multicore build passed native and ARM64 builds; native headless and
snapshot/serial/thread/drop regressions; same-GPU equivalent-frame pixels,
ordered geometry, texture events, board/simulation digests and PCM/voice
checks; and the full recorded replay's 63 state records. Native ThreadSanitizer
reported no application race in its frame-1700 test, with a documented Mesa
internal suppression; Mali code was not instrumented. Review
[multicore architecture](MULTICORE_RENDERING_PIPELINE.md) for exact scope and
limitations, and [performance audit](ARM64_GLES2_PERFORMANCE_AUDIT.md) for
older architectural findings. Historical reports predate later improvements;
do not treat their performance figures as the current baseline.

For any new change: inspect the actual build flags and Git diff first; keep
the known-good device executable until the candidate is validated; compare
simulation, board, audio and equivalent-frame visual state; use the human lap
for racing performance; and report simulation rate separately from actual
presentation timestamps. Do not infer GPU saturation from FPS or move CPU
texture decoding across threads without an immutable, versioned resource
handoff. The next optimisation has not been selected by this handover.

## World RGBA atlas investigation — 2026-10-14

The committed GPU HUD production executable remains deployed and unchanged
(SHA256 `6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`).
The world-atlas prototype is rejected for production deployment and stays OFF.
See [architecture, correctness and decision](GPU_WORLD_ATLAS_INVESTIGATION.md)
and [five-pair results](benchmarks/world-atlas-20261014/RESULTS.md). No commits
or additional permanent launchers were created.

Compatible-dimension two-layer RGBA arrays preserve original UVs, bake tiers,
cache policy and ordered updates. Native complete recording, Mali controlled
first lap and TSan pass exact pixels/state/PCM/voices. Ordinary 2D atlas UV
remapping is not exact at nearest-sampling boundaries. Native zero-area fan
cases require original draw boundaries when merged after reordering; the
smallest reproductions are preserved in `tools/world_atlas_case/minimal/`.

Five alternating complete first-lap pairs: median FPS 52.931 -> 56.019,
world draws 860.7 -> 439.0, but 1% low 25.151 -> 21.699 FPS and worst hitch
91.9 -> 106.6 ms. Tick-1965 upload/driver time rises about 19 -> 53 ms with
similar decode time and miss count. RSS adds 44.2 MiB. Simulation remains
59.904 Hz; all ten runs have zero audio underruns/resyncs and identical state.

The optional source is behind `RR_GLES_WORLD_ATLAS=OFF` in the H700 preset
and runtime mode `off`. Preserve the captures and tools; these experiments
are complete evidence, not a request to restart atlas micro-optimisation.
The existing world renderer, GPU HUD, multicore/geometry optimisations and
controller configuration remain the playable reference.
