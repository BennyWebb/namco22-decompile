# G1c — Attract cache saturation fixed

| Item | Result |
|---|---|
| **Overall** | **BLOCKED only on reference preparation no-regression condition; STOPPED** |
| Attract gate | **PASS:** 928.459 s, 55,443 pictures, compiler bounded; every selector ≥4 complete visits |
| Attract repeats / settled underruns / native main gaps >50 ms | **0 / 0 / 0**, including course transitions |
| Recipe cache | Peak 39,946; 1.00175 probes/lookup, max 3; zero full/probe-bound fallbacks |
| Dynamic page | 3072², three GPU copies / 27 MiB; 39 misses, **zero evictions** |
| Host correctness | All 21 sets, 4,207 packets, 36 provenance images, class (c) = 0 |
| Reference state / pacing | PASS; all 21 sets; zero repeats, settled underruns and main gaps |
| Reference preparation | **REGRESSION:** median 9.032 vs 8.334–8.343 ms; dense p99 13.362 vs 13.120–13.222 |
| Attract memory / thermal | Mali peak 178.863 MiB; RSS HWM 360.910 MiB; peak 58.787°C; no sampled throttling |
| Production / cleanup | 47 protected paths exact; clocks/RT restored, EmulationStation running, no game |

## Attribution first

Started clean on `feature/knulli-h700`, HEAD `24746a29e3a74cae8d8b103c421330b9bd47fd36`.
Read HANDOVER/G1b; changes are private `native/g1c/`, evidence and documentation.
No shared engine, simulation, physics, audio/control logic or production changes.

Host attract passed 13,400 with the old algorithm instrumented before fixing.
Saturation is confirmed: first full table at tick 12,839, occupancy 32,768.
Ticks 13,393–13,520 average 1,940.438 probes/lookup, with 8,230 full-table scans.
New keys could not be inserted, so misses repeatedly scanned all 32,768 entries.
Host compiler medians there were 1.655 / 1.582 ms (initial / separate-stream repeat);
repaired host median 0.358 ms, maximum three probes. Absolute host times are attribution only, not H700 performance.
Together with G1b's ~290 ms compiler / ~2 ms walk on that phase, this identifies
cache saturation as the slowdown mechanism; no unrelated compiler rewrite was made.

## Bounded cache and source working set

Capacity = nextpow2(2 × 21,783 static entries × 128 palette groups) = 8,388,608.
Compact 32-bit cells cost 32 MiB, referring to a 65,535-entry exact-key pool
(2,097,120 bytes). Every hit compares all seven recipe integers. Only static
lookup results, including misses, are cached; per-picture dynamic IDs never are.
Pool exhaustion or 64 probes advances generation in O(1); generation wrap clears
cells. Occupancy stays ≤min(capacity/2,65,535), independent of play length.
The forced-collision/pool/wrap unit test passes 400,002 lookups; no timing sample
is used to excuse a mismatch. RGBA lookup remains the original implementation.

Attract has 23,653 logical footprints; separate padded rectangles sum to ~36.7 MiB
per ordinary selector, largely overlapping source regions, not simultaneous demand.
A larger page alone would waste memory. Runtime allocation rounds source bounds
outward to multiples of 256, preserving bank/cmode and two source-aware gutters.
Logical recipe/group keys remain exact; UV/provenance origins account for the
shared allocation. No replay recipes enter the unchanged ROM-only static pack.
Observed physical set: 39 rectangles, 5,556,848 padded bytes, fitting a 3072² page.
Three fenced GPU copies cost 27 MiB, **+24 MiB** versus G1b. Immutable slot copies,
current-picture pinning and deterministic LRU remain. Host and device evictions: 0.
Every observed logical crop matches ROM decoding: **192,647,935 padded texels exact**.
Ownership stress: 300 pictures, 586 interleaved evictions, queued bytes preserved.

## Host and reference validation

Host normal/provenance complete 6,006 ticks, all 21 sets / 63 domain records,
4,207 adjusted receipts and all per-decode ROM hashes. Raw packets and all 36 colour
images also match G1b byte-for-byte. Provenance a/b/c = 0/6,802/0; colour proofs pass.
Old/fixed host attract common 135 periodic domain records match exactly.
No new Mali provenance replay was requested; these are host image gates, not a new
hardware image proof. User observed the device attract: “It's looking and sounding great.”

One ARM smoke to 1900 confirms cache/page/async transport and actual nice0/policy0
on main/rr-prepare/rr-present. One reference timing run, queue2, fixed CPU1512MHz,
stock GPU240MHz, default priorities/affinity off; all 21 checkpoint sets pass.
Nearest-rank tails, no outliers removed; race 1800–6006, dense 3550–3590 (41 ticks).

| Reference metric, ms | G1c | G1b runs |
|---|---:|---:|
| Race prepare median | 9.032 | 8.334 / 8.343 |
| Dense prepare median / p99 | 12.893 / 13.362 | 12.074 / 13.120; 12.403 / 13.222 |
| Race compiler median | 6.084 | 5.373 / 5.367 |
| Swap interval median / p95 / p99 / worst | 16.746 / 16.837 / 17.069 / 19.110 | Full distributions retained |
| Snapshot→swap median / p99 | 33.353 / 33.665 | 33.360 / 33.358 medians |

Full activated timeline: zero repeats/underruns/main gaps; tick-1578 repeat does not
recur. Its historical cause remains unproven. Legacy release is tick178; settled
audio starts there, not at1800. Preparation rises ~0.69 ms median; sparse-cache
access and larger repeated slot copies are possible contributors, not isolated
attribution. Although pacing stays clean and dense p99 remains below14ms, the
explicit G1b no-regression condition is **not met**. No further change/run was made.

## Complete attract result

930 s supervisor limit yields **928.459 s / 15.474 min** of presentation, 55,281
native +159 boot legacy +3 prewarm pictures; unsupported fallback 0. All samples
and transitions retained. Visits complete: selectors0/1/2/3 =5/4/4/4; selector1
has an additional partial visit. No main gap >50 ms, repeat or settled underrun.

| Selector | Native / legacy | Prepare / compiler p99, ms | Occupancy peak | Probes mean / max | Evictions | Mali / RSS peak, MiB | Peak °C |
|---|---:|---:|---:|---:|---:|---:|---:|
| 0 | 16,231 / 150 | 9.362 / 6.218 | 39,946 | 1.00110 / 3 | 0 | 178.863 / 360.719 | 58.544 |
| 1 | 14,994 / 0 | 8.696 / 5.628 | 39,946 | 1.00110 / 3 | 0 | 178.863 / 360.719 | 58.625 |
| 2 | 12,024 / 0 | 11.958 / 7.960 | 39,946 | 1.00311 / 3 | 0 | 178.863 / 360.719 | 57.734 |
| 3 | 12,032 / 0 | 11.044 / 7.320 | 39,946 | 1.00178 / 3 | 0 | 178.863 / 360.719 | 58.787 |

The remaining 12 legacy pictures have boot selector65535. Repeats/settled underruns/
main gaps are zero in every selector. Per-visit compiler p99, in visit order:
0:5.714,6.511,5.596,6.486,5.667; 1:5.415,5.827,5.321,5.891,(5.327 partial);
2:8.057,7.607,8.087,7.661; 3:7.644,7.230,7.482,7.301 ms. No accumulating growth;
occupancy plateaus at39,946, full fallbacks and generation resets both0.
GPU pen upload total16,670,544 bytes; CPU pending copies13,265,054,192 bytes.
Minimum MemAvailable502.125 MiB; CmaFree reaches0, without a measured stall/error.
Mali stays below200 MiB; minima and per-selector resources are retained separately.
All9,304 samples: CPU1512MHz, GPU240MHz, CPU/GPU cooling states0. Start44.774°C.
Observer p99 interval100.957 ms, worst106.908; game/observer drops/errors0.

## Integrity and stop

See [evidence](benchmarks/native-g1c-20261016/README.md). Device session ~24.8 min.
22 runtime asset hashes and47 protected paths verify; production SHA256 remains
`6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`.
All4 CPUs restored schedutil480000–1512000, GPU simple_ondemand bounds0/0, RT
950000/1000000; EmulationStation PID21797, no game/sampler. No commit/push/deploy.
**STOPPED:** attract objective passed; isolate reference preparation cost only with
separate authorisation. No G2, broader coverage or further optimisation begun.
