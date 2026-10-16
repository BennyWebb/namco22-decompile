# G1b evidence — 2026-10-16

**Result: reference-window gates PASS; whole milestone BLOCKED by attract compiler
slowdown. STOPPED.** See [report](../../NATIVE_G1B_REPORT.md). No G2 implementation.

## Sources and workloads

Branch `feature/knulli-h700`, initial/final HEAD
`62b871ba1cf45129e79c16aa46c0b0e6ada6c900`. Private implementation/tools:
`../../native/g1b/`. No shared engine, original gameplay/audio/control changes.
Static cache is the unchanged G1 ROM-only pack (see ../native-g1-20261016/assets.json):
2,421 models, 97,116 quads, 21,783 footprints, 112 MiB pens, 11.859 MiB mesh data.
Cache key `edfbde03b0d5e68d18bf8041fe2f31786a4063af6bd47e200fd09d5261221108`.
No recording material catalogue or recording data enters static/dynamic assets.
Runtime direct materials use the in-memory ROM decoder; palette is not cache identity.

Reference replay is the unchanged
`/userdata/system/rave-racer-replays/race-20261013-122242-25136.rec`, full 6,006 ticks.
Matched analysis 1800–6006; dense 3550–3590; settled audio starts at actual release
178. Nearest-rank p95/p99, no exclusions/outlier removal. Per-picture originals
are retained even outside the matched window. Reference timing 2's repeat at1578
is not hidden, relabelled handover, or evidence of globally repeat-free activation.

## Run order, build identities and setup

Five device smoke runs: smoke-validation/play, smoke-v2-validation/play, then
smoke-stream. Each ends at1900 and verifies actual main/prepare/present nice0,
SCHED_OTHER and switch receipts. Initial v1 → v2 adds raw major-course snapshot
and per-picture CPU cache counters only. Host colour-final/provenance-final prove
raw packets and readbacks byte-identical to v1, plus checkpoint parity.

Full device order: colour-index validation, provenance-index validation, timing-1,
timing-2, attract. Exactly two full reference timing runs; no fences, no clock420,
no additional soak, no timing rerun. Every attempt/smoke retained in runs/.
All reference captures use v2 validation; both timing runs use preserved v2 play.
`session/final-binaries.json` identifies those binaries. Full validation and timing
all pass 21 checkpoint sets; timing compiles out per-picture packet validation.

The attract-length log volume would overflow the inherited finite RAM trace
shutdown transport. Before starting attract, added opt-in default-off
`ENG_G1B_STREAM_TRACE=1`: native traces enter existing asynchronous memory cookies,
rr-writer alone drains persistent storage. Historical `TRACE_DRAIN mode=sync`
means cookie streaming, not synchronous file I/O on a game thread.
Logging-only source delta, host full-replay exact equivalence and replacement
1900 smoke pass: see logging-equivalence.json, runs/colour-stream, smoke-stream.
Attract uses that binary (`session/stream.sha256`); timing uses v2, never mixed.
stdout/stderr are separate for attract to prevent combined-file record interleave.

Routine build setup corrections (missing SHA ABI header/extern and play-transform
hook handling) were fixed before running hardware; build logs retained. No failed
correctness gate was bypassed. The only interrupted full run is attract, described
below. No unsupported material was added to the static catalogue.

## Correctness evidence

host/ and mali/ contain validation.json, classification.json, pixels.csv,
capture-proof.json, dynamic-texel-parity.json and provenance-ID-normalisation.json.
All21 sets/63 records, 4,207 raw host/Mali packets and adjusted reference receipts
pass. Adjustments normalise whole-ROM/dynamic material IDs, mesh IDs and page
representation, retaining geometry, transforms, ranks, depth, brightness and flags.
A reference-identity mapping is used only by validation; play/attract unset it.
Dynamic recipe IDs are resolved from decoded keys, not a replay material catalogue.

Each normal/provenance pair independently checks 6,030 runtime decode events,
3,015 unique footprints/run, against Python ROM pens including source-aware gutters.
Host residual classes a/b/c =0/6802/0; Mali=0/839/0. All same original material/bank,
839 adjacent texels (450 horizontal,389 vertical), maximum channel delta153.
36 MRT colour proofs and 11,059,200 sampled ROM/palette colours exact per platform.
Full colour/provenance readbacks remain ignored, identified by readback-manifest.json.

All 36 side-by-sides/difference overlays and six contact sheets per platform:
`../../native/generated/g1b/host-review/` and `../../native/generated/g1b/mali-review/`.
Every contact sheet was visually inspected. These are ROM-derived local review
artifacts and are not published into Git. No new human visual verdict was received
before final reporting; user confirmed availability/readiness, not a result.

## Timing, coverage and interrupted attract

runs/device-timing-{1,2}/ include all samples, summary.json, repeat-events.json,
coverage-summary.json, picture-coverage.json and dynamic-per-picture.json (large
files losslessly gzipped). Both matched race windows:0 repeats/underruns/main gaps.
Dense preparation p99=13.120222/13.222303ms versus C1e RGBA13.416826ms.
Full activated timeline: repeat164 handover in run1; repeat1578 non-handover in run2.
No actual-native main gap>50ms; settled audio0 starting atrelease178.
Cache/atlas/activation changed together, so no isolated cache-only speedup claimed.

Attract: no recording/controller input; planned600s timeout but manually stopped
with SIGTERM after356.234s/13880pictures on severe slowdown. Exit0 reflects clean
signal handling/drain, NOT successful completion of600s. No new device run started.
`runs/device-attract/attribution.json` labels termination and all native stretches.
Complete telemetry shows continued progress; initial buffered-tail and sleeping
thread evidence appeared frozen. Main/present wait on preparation, not deadlocked.
Kernel stacks/maps/syscalls saved in hang-stacks.txt (historical filename retained).

All course selectors0/1/2/3 reached. Second selector0 stretch at13389 has prepare
median292.577ms, compiler290.366ms; walk2.222ms/draw2.075ms. Main blocked median
282.328ms. First repeat13393;410 actual-native main gaps>50ms;488 repeat events,
7393 inferred repeated vblanks;5902478 underrun sample frames across native pictures.
No new dynamic misses in that second stretch; compiler function-level cause unknown.
Full recipe-cache probing is a hypothesis only; no speculative fix was attempted.
Fallbacks:159 boot-state,3 prewarm,0 unsupported. No dedicated alternative-course
script found; attract selector visits do not prove other-course image correctness.

## Clock, observer and restoration method

Every run: save all4 CPU governors/min/max + GPU bounds/governor and RT defaults;
fix CPU1512MHz, keep GPU simple_ondemand, strict FIFO q2, default priorities and
no affinity; EXIT trap restores clocks, checks47 protected paths, restarts ES.
Start≤55°C or180s idle; temperatures/cooldown receipts retained. Both timing starts
46.556/47.933°C; attract45.341°C. Every measured CPU sample1512MHz/GPU240MHz,
CPU/GPU cooling states0. Attract peak SoC56.276°C; timing peak58.625°C.
Low-impact long-lived SCHED_IDLE/nice19 sampler follows actual game PID at100ms,
collecting RSS/HWM/MemAvailable/CmaFree/Mali pages, clocks, thermals and cooling.
Attract sample intervals median100.000ms, p99100.424ms, worst108.580ms.
All game and observer rings drain with0 drops/errors. Timing output stays on
persistent userdata; /tmp before runs16KiB. No transfers/archives during timed runs.

Observed play Mali peak154.656MiB includes native and retained overlap; debug
validation peak167.734MiB also passes target200. Atlas112MiB + three1MiB dynamic pages is not confused with total Mali.
Mali and RSS may overlap physically; do not add them as independent residency.
Reference load variation retained: cache read1625.332/239.609ms (cold/warm),
GL-ready2750.015/918.770ms. Attract read208.269ms/GL-ready866.608ms.
These timers exclude an earlier lookup-file read, and are not full process startup.

session/final-state.txt: production SHA256
`6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`,
47 paths exact, all CPUs schedutil480000–1512000, GPU simple_ondemand0/0, RT
950000/1000000, ES PID13907; no game/sampler left. Each run's restoration receipt
is0. Device staging uptime41231.57→final43468.85 (~37.3min).

telemetry-manifest.json records source/stored SHA256 and lossless compression;
source-sha256.json records private tool hashes. ROM assets/binaries/readbacks stay
ignored locally. No commits, pushes, production changes or G2 work. STOPPED.
