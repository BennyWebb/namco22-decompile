# G1b — Runtime ROM footprints and whole-ROM integration

| Exit item | Result |
|---|---|
| **Overall** | **BLOCKED: attract compiler slowdown; stopped, no G2 work** |
| ROM-only static pack | PASS: 2,421 models, 21,783 footprints, 112 MiB index atlas |
| Runtime direct footprints | PASS on reference: bounded LRU, ROM decode, immutable uploads |
| Host / Mali correctness | PASS: 21 checkpoint sets, 4,207 adjusted packets, 36 images; class (c) = 0 |
| Reference ticks 1800–6006 | Both runs: 0 repeats, 0 audio underruns, 0 main gaps >50 ms |
| Dense preparation p99 | 13.120 / 13.222 ms; better than C1e RGBA 13.417 ms |
| Earlier activated native pictures | Run 1: handover repeat 164; run 2: non-handover repeat 1578 |
| Whole-ROM residency | PASS: play Mali peak 154.656 MiB; provenance debug 167.734 MiB, both <200 |
| State activation / coverage | Native starts at 163; attract visits selectors 0–3; unsupported fallback 0 observed |
| Attract completion / pacing | FAIL: stopped after 356.234 s, 13,880 pictures; compiler reaches hundreds of ms |
| Production / restoration | Production and 47 protected paths exact; clocks/RT restored, EmulationStation running |

## Scope and implementation
Started on `feature/knulli-h700`, HEAD `62b871ba1cf45129e79c16aa46c0b0e6ada6c900`,
with a clean worktree. Changes stay in private `native/g1b/`, evidence and docs.
No shared engine, original simulation, physics, audio or controls were changed.
No commit, push or production deployment. Compact indexed textures remain adopted;
the retired fenced-completion gate was not reinstated or tested.

Reused G1's unchanged ROM-only cache: 97,116 quads, 21,783 static materials,
seven 4096² R8UI pages (117,440,512 bytes), mesh data 12,435,456 bytes.
All 564,081,621 padded texels match ROM decoding. Original build: 115.553 s;
cache key covers ROM and builder hashes. No recording builds or extends assets.
G1b adds only debug-mapping reserve to this pack, not replay-discovered materials.

On rr-prepare, static misses decode through the existing immutable ROM decoder.
Exact key is (bank, cmode, u, v, w, h); palette group is a separate material index.
The deterministic 1024² page uses an 8-pixel occupancy grid, 4,096 entries and
LRU eviction, pinning current-picture references. Two source-aware gutter texels
are preserved. Limits: 1,020² interior, 512 per-picture materials/uploads and
1 MiB slot texels; excess explicitly falls back rather than omitting geometry.
Each slot copies placements, generations, texels and UV metadata. rr-present
waits the existing resource-copy fence before updating one of three dynamic R8UI
pages and material textures. Later CPU eviction cannot alter queued slot bytes.
Host stress: 300 pictures, 599 intervening evictions; snapshot ownership exact.

Index material resolution reuses the exact-key P2 cache without palette-content
identity. Changed palette uploads remain slot-owned. Validation-only ID maps
normalise receipts; timing and attract runs unset that map and use no replay
catalogue. Private play builds retain no per-picture packet hashing.

## Correctness and activation
Host normal/provenance and Mali normal/provenance runs each complete 6,006 ticks,
all 21 checkpoint sets / 63 domain records, and 4,207 adjusted command receipts.
Raw Mali packets match the host. Adjustment changes pack mesh/material identity
and page representation only; original geometry, transforms, ranks and state remain.
Every runtime decode matches independent ROM pens: 3,015 footprints / 3,015 events
per run, 6,030 exact events across each normal/provenance pair.

Host 36-image residuals: class (a) 0, adjacent-source (b) 6,802, (c) 0.
Mali: (a) 0, (b) 839, (c) 0; 450 horizontal and 389 vertical adjacent choices,
no bank/material mismatch. Maximum channel difference 153 is an adjacent texel's
colour, not a wrong palette. All 36 MRT colour proofs and 11,059,200 sampled
ROM/palette colours match. All review contact sheets inspected: no missing world,
mirror or HUD corruption. Full images/overlays remain in ignored local galleries.

Activation reads immutable snapshot RAM: main 0x10008DA0, substate 0x1000A32E,
screen 0x1000A332, route 0x1000A046, major course selector 0x1000A048.
Main outside 1–5 uses legacy; otherwise complete command coverage decides.
Reference: 159 boot legacy + 3 live prewarm legacy + 5,844 native pictures;
first native 163, gradual legacy release completes 178, unsupported fallback 0.
The old tick-1800 audit switch is unset. Attract also activates at 163 and changes
course selectors without a drain or fixed replay tick. Course numbers below are
raw major selectors; they are not inferred names from the changing route word.

## Reference timing — all samples retained
CPU 1512 MHz, GPU stock 240 MHz, queue 2, default priorities/affinity off.
Each path/switch was smoked to 1900 with actual thread census receipts first.
Starts: 46.556 / 47.933 °C; no thermal/cooling/frequency flags. Nearest-rank tails;
dense = 3550–3590 (41 samples), matched race = 1800–6006 (4,207 samples).

| Metric, ms unless stated | Timing 1 | Timing 2 |
|---|---:|---:|
| Dense prepare median / p95 / p99 = worst | 12.074 / 12.954 / 13.120 | 12.403 / 13.031 / 13.222 |
| Dense compiler median / p99 | 7.990 / 8.767 | 8.083 / 8.877 |
| Race prepare / compiler / walk medians | 8.334 / 5.373 / 2.845 | 8.343 / 5.367 / 2.860 |
| Simulation / draw-wall / swap-wait medians | 11.061 / 4.061 / 12.133 | 11.118 / 4.038 / 12.145 |
| Swap interval median / p95 / p99 / worst | 16.746 / 16.844 / 17.119 / 18.634 | 16.746 / 16.833 / 17.034 / 19.147 |
| Snapshot→swap median | 33.360 | 33.358 |
| Race repeats / settled underrun frames / main gaps >50 ms | 0 / 0 / 0 | 0 / 0 / 0 |
| Peak native temperature, °C | 58.220 | 58.625 |
| Whole-run Mali peak / RSS HWM, MiB | 151.805 / 296.656 | 152.109 / 296.578 |
| Minimum MemAvailable / CmaFree, MiB | 569.102 / 2.277 | 568.930 / 5.344 |

Settled audio is measured from actual release tick 178, not clipped to 1800.
Neither run has any actual-native main gap >50 ms. Full-timeline repeat receipts:
run 1 tick 164 interval 30.575 ms (handover); run 2 tick 1578 interval 33.890 ms,
prepare 13.743, simulation 10.638, draw 7.179, swap 26.312 ms. The second is an
unresolved non-handover pacing exception; matched-window success does not erase it.
Net preparation improves versus C1e, but this is not an isolated cache-only A/B:
whole-ROM layout and earlier activation also differ. No cache-only gain is claimed.

Per reference run: CPU hits 45,486, misses 3,015, evictions 2,897; pending pen bytes
24,927,989, actual GPU pen bytes 24,526,439. Every matched-race picture: 9 hits,
0 misses/evictions, 75 pending bytes, 0 new GPU pen bytes and 48 UV metadata bytes.
Cache-read times: 1,625.332 / 239.609 ms; upload-submit 376.694 / 310.657 ms;
GL-ready 2,750.015 / 918.770 ms. Earlier validation read 198.968 ms. Cold storage
variation is retained. These timers exclude the earlier live lookup-file read.

## Attract blocker and remaining coverage
One unattended run, no recording/input: 13,880 pictures in 356.234 s; stopped
with TERM and clean drain after apparent freezing. Recovered telemetry proves
continued slow progress, not deadlock. The planned ~10 minutes was not completed.

| Main / course selector | Native | Legacy | Native repeats / underrun sample frames |
|---|---:|---:|---:|
| 0 or 65535 / boot | 0 | 159 | — |
| 1 / 0, both passes | 4,699 | 3 prewarm | 7,393 / 5,902,077 |
| 1 / 1 | 3,005 | 0 | 0 / 0 |
| 1 / 2 | 3,006 | 0 | 0 / 0 |
| 1 / 3 | 3,008 | 0 | 0 / 401 |

First course-0 stretch 163–4369 has zero repeats/underruns; second begins 13389,
first repeat 13393. On that second stretch, prepare median 292.577 ms, compiler
290.366, walk 2.222, draw 2.075, swap 0.069. There are 410 native main gaps >50 ms,
caused by queue backpressure behind preparation, and 488 repeat events. Tail
prepare reaches 440–520 ms. Last pictures have zero dynamic misses/evictions:
new footprint decoding is not causing that tail. Cache saturation/full probing
is a plausible suspect (32,768-entry recipe cache, no replacement); call-site
attribution is unproven. No speculative fix or further run was started.

Attract totals: 59,565 hits, 23,653 misses, 23,538 evictions, 460,220,747 pending
pen bytes, 192,950,645 actual GPU pen bytes and 1,033,040 UV metadata bytes.
Play peak Mali 154.656 MiB (debug validation 167.734), RSS HWM 297.871 MiB; minimum MemAvailable 564.203 MiB,
CmaFree 32.562 MiB. CPU/GPU clocks correct throughout, both cooling states 0,
peak SoC 56.276 °C. No writer/observer drops or errors; every picture accounted for.

Observed unsupported-format fallback count is zero; this is not complete coverage.
Remaining G2 input: point-RAM/solid and unsupported command layouts, absent meshes,
capacity/oversize fallback, menus/interactive transitions and visual correctness of
other courses. No dedicated alternative-course script was found. Attract's four
selectors provide coverage counts, not provenance validation of those courses.

## Evidence, safety and stop
See [evidence](benchmarks/native-g1b-20261016/README.md). Attract uses an opt-in
stream-to-existing-async-cookie trace transport to avoid finite shutdown-buffer
limits; host exact equivalence and device smoke pass. Timing uses preserved v2.
Output buffering initially obscured progress; stacks show prepare runnable and
main/present waiting. Subsequent complete telemetry corrects the initial hang label.
All timed output goes to persistent userdata, not /tmp; preflight /tmp was 16 KiB.
Device session ~37.3 minutes. Production SHA256 remains
`6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`.
All 47 paths, original schedutil 480–1512 MHz, GPU simple_ondemand bounds 0/0 and
RT defaults verify restored; EmulationStation PID 13907, no game or sampler left.
**STOPPED. Attract compiler attribution/fix requires separate authorisation; no G2.**
