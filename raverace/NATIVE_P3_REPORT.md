# P3 — Scheduling robustness and preparation headroom

**COMPLETE / performance gate FAIL / STOPPED.** Both final dense p99 values meet
14 ms, and the soak is clean; final-2 has one inferred repeat at the first native
picture (tick 1800), failing the required zero-repeat headline gate.

| Variant / step | Dense prep median / p99 ms | Inferred repeats | Settled underrun frames | Main gaps >50 ms |
|---|---:|---:|---:|---:|
| P2 control (reused) | 13.959 / 14.921 | 0 | 0 | 0 |
| A1 nice (rejected) | 13.741 / 15.359 | 0 | 0 | 0 |
| A2 RR (rejected) | 14.147 / 18.554 | 4 | 2657 | 0 |
| B1 (kept) | 13.100 / 15.412 | 0 | 0 | 0 |
| B1+B2 / final-1 | 12.587 / 13.705 | 0 | 0 | 0 |
| Final-2 (same build) | 12.346 / 13.492 | 1 | 0 | 0 |
| 15m29s soak, 7 laps | Per-lap p99 13.194–13.904 | 0 | 0 | 0 |

Selected: **P2 b + B1 + B2, default priorities**. One final device validation passes
all 21 sets, 4,207 packets and 36 Mali images. No criterion or analysis window is
changed; tick 1800 remains inside native acceptance. Initial blocked work is history.

## Method and scope

The continuation started clean on `feature/knulli-h700` at
`ea0e2ea6d7e83ec44d84dd197759fc8a6cdd9762`. The existing P3 `control` was reused,
without another baseline replay. All changes remain private under `native/p3`.
P2's play split, palette cache and selective gather, three-stage strict FIFO,
queue 2, panel pacing, asynchronous logging, pre-warm, gradual legacy release,
valve OFF, generated O2 with stack protection, affinity OFF and stock GPU remain.
Original simulation, physics, AI, audio algorithms, assets, shaders and fallback
are unchanged. No shared engine source, display timing or production file changed.

CPU is fixed at 1512 MHz with userspace during runs; original governor/min/max,
GPU policy and kernel RT defaults are recorded and restored by EXIT traps.
Cooldown uses SoC thermal zone 0 ≤55°C or 180 seconds idle, whichever occurs first.
Every run records clocks/thermal at nominal 10 Hz and a 1 Hz all-thread census;
headline process observation is OFF. The soak uses the existing low-impact,
SCHED_IDLE/nice-19 observer following an explicit game PID for each lap.
No game diagnostic output uses RAM-backed `/tmp`; preflight usage is retained.
Transfers and archiving occur only while games are stopped.

Reference replay: all 6,006 ticks; native 1800–6006; dense 3550–3590 inclusive.
Nearest-rank p95/p99 includes every sample; 41 dense samples make each run's p99
its maximum. Underruns count missing sample frames, not events. Full native uses
counter(6006)−counter(1799); settled uses counter(6006)−counter(actual release tick).
Every measured release completes at tick 2112. Repeat inference is unchanged from
P1b: max(0, round(swap-return interval / 16.7474996 ms)−1). It is a proxy, not a
physical scanout measurement; complete intervals and snapshot→swap ages remain.

## A — scheduling selection

`ENG_P3_PRIORITY=nice` and `=rr` were each smoke-tested to tick 1900 before timing.
Application receipts and kernel census both confirm all three actual settings:
nice main/present −10, prepare −5; RR main/present SCHED_RR 5, prepare nice −5.
Mali workers retain SCHED_OTHER/nice 0, SDLAudioP2 retains nice −20, and PipeWire's
existing data-loop retains SCHED_FIFO 83. Kernel RT limits stay 950000/1000000 µs.
The B1/B2 binaries also pass default-priority smokes before their timings.

Nice gives only 0.067 ms paired dense median saving, while native preparation
median and dense p99 regress. It is not selected as a convincing overall gain.
RR improves simulation and main/present access but regresses dense preparation,
produces four inferred repeats and 2,657 settled underrun frames. Native-window
cumulative run-queue wait, in ms (not exclusive service cost):

| Thread | Control | Nice | RR |
|---|---:|---:|---:|
| Main | 917 | 981 | 24 |
| rr-present | 716 | 538 | 25 |
| rr-prepare | 844 | 921 | 1077 |
| Mali CMAR backend | 660 | 1003 | 1261 |
| Audio callback | 250 | 253 | 786 |
| PipeWire RT data-loop | 9 | 4 | 4 |

The audio callback's largest census-interval wait rises from 7.38 to 23.14 ms
under RR. All threads continue making progress; no hang or prolonged starvation
is observed. The increased driver/audio waiting and shortages are adverse
scheduling signs, not evidence that raising game priorities improves delivery.
RR is rejected. **Default priorities are retained.** Replay tests cannot prove
interactive input responsiveness; no additional manual-control test is claimed.

## B — exact-output preparation changes

B1 uses checked 64-bit multiplication, shift/division and base add/subtract where
representable, retaining the original exact 128-bit fallback for overflow or
wide cases. UBSan passed 28,449,792 Cartesian boundary cases plus 100 million
random inputs against the original function, covering all defined shifts 0–127,
both signs, saturation, zero divisors and signed division overflow. The fast path
is enabled only during the native walk; legacy/fallback uses original arithmetic.
B1 is kept for its **0.812 ms paired dense median saving**, despite its tail miss.

B2 preserves the original float32-error enclosure and reason precedence. With
positive finite zoom and positive Z bounds, monotonic endpoint selection halves
projection divisions; near-plane and negative/nonfinite-zoom cases retain their
original handling. No reciprocal approximation or reassociation is introduced.
One million synthetic differential quads pass under UBSan. Validation builds
also compare original/candidate reason codes on every real replay quad.
B2 is kept for another **0.568 ms paired dense median saving**. Combined paired
saving versus control is **1.305 ms**; independent medians are not added.
All signed paired samples, including regressions, are retained.

Both native-only host builds pass all 21 authoritative sets, 4,207 packets and
36 host image hashes. The **single final device validation** independently passes
all 21 sets/63 domain records, all 4,207 packets (267,308,900 bytes) and all 36
sampled Mali world/composed image hashes. Validation timings are retained but
excluded from play comparisons: SHA, readbacks and dual culling checks are enabled.
The final play build performs no packet/image digesting on preparation/presentation.

## Final build and service costs

Final is P2 selected b + B1 + B2, with default priorities (`ENG_P3_PRIORITY=off`).
Binary SHA256: `0fdf81922672ee47be0c71cba245c5e6da841328de86a1d8b92310988ec40817`.
`r-b2-timing` is final-1, counted once; `r-final-2` is the only repeat. The same
verified binary/settings smoke gates both. No optional perf capture was needed.

Preparation and simulation service distributions, median/p95/p99/worst ms:

| Run | Window | Preparation | Simulation |
|---|---|---|---|
| P2 control (reused) | native | 9.610/12.401/13.928/16.818 | 11.607/13.460/14.212/18.347 |
| P2 control (reused) | dense | 13.959/14.500/14.921/14.921 | 13.544/14.215/14.459/14.459 |
| A1 nice (rejected) | native | 9.700/12.364/13.895/16.744 | 11.566/13.434/14.110/17.723 |
| A1 nice (rejected) | dense | 13.741/14.659/15.359/15.359 | 13.443/14.143/14.307/14.307 |
| A2 RR (rejected) | native | 9.784/12.469/14.181/18.554 | 11.355/13.178/13.808/17.425 |
| A2 RR (rejected) | dense | 14.147/14.770/18.554/18.554 | 13.233/13.836/14.225/14.225 |
| B1 (kept) | native | 9.015/11.658/13.325/15.638 | 11.470/13.306/14.040/17.884 |
| B1 (kept) | dense | 13.100/14.509/15.412/15.412 | 13.303/14.287/14.398/14.398 |
| B1+B2 / final-1 | native | 8.651/11.062/12.640/15.474 | 11.523/13.421/14.099/19.541 |
| B1+B2 / final-1 | dense | 12.587/13.360/13.705/13.705 | 13.487/14.433/14.615/14.615 |
| Final-2 (same build) | native | 8.632/11.123/12.702/14.848 | 11.446/13.271/14.116/17.942 |
| Final-2 (same build) | dense | 12.346/13.025/13.492/13.492 | 13.252/14.202/14.624/14.624 |

Swap intervals (median/p95/p99/worst ms), full-native audio and thermal context:

| Run | Swap interval | Native / settled underruns | Start / peak native °C |
|---|---|---:|---:|
| P2 control (reused) | 16.746/16.836/17.124/18.990 | 898 / 0 | 48.74 / 61.95 |
| A1 nice (rejected) | 16.746/16.840/17.137/18.224 | 824 / 0 | 49.39 / 61.22 |
| A2 RR (rejected) | 16.747/16.828/16.989/33.486 | 3509 / 2657 | 53.20 / 61.95 |
| B1 (kept) | 16.746/16.833/17.139/19.359 | 826 / 0 | 49.47 / 62.03 |
| B1+B2 / final-1 | 16.746/16.832/17.049/19.127 | 880 / 0 | 48.66 / 60.89 |
| Final-2 (same build) | 16.746/16.837/17.056/26.342 | 1289 / 0 | 48.34 / 61.22 |

All headline sampled CPU clocks are 1512 MHz, GPU 240 MHz, cooling states 0;
no run is clock-flagged. [All stage distributions](benchmarks/native-p3-20261016/resume-service-timings.csv)
retain preparation CPU, walk, compiler, GL CPU/draw/swap and snapshot→swap age,
native/dense mean/median/p95/p99/worst. Concurrent stage medians are not added.

## Remaining failure: first-native handover

Final-2 tick 1800 has a **26.342 ms** swap-return interval (one inferred repeat).
Preparation is 14.848 ms (walk 3.615, compiler 10.737), simulation 14.518 ms,
draw service **25.779 ms**, swap 0.118 ms. Presentation-thread CPU is only
**5.318 ms**; snapshot→swap age is 52.153 ms. The preceding legacy picture uses
25.744 ms presentation-thread CPU. This locates the event at the handover's draw
service with substantial non-CPU time; headline instrumentation cannot isolate
the exact driver/GPU/scheduling wait. It is not a >50 ms main-thread gap.

All three warmups completed at ticks 1740–1742. Legacy retirement starts after
16 native pictures and completes at 2112, so its deletion work is not executed
on tick 1800. The event is retained, with neighbouring samples and original
[stage evidence](benchmarks/native-p3-20261016/runs/r-final-2/handover-event.json).
No speculative handover fix or additional run is performed. A focused diagnostic
of ticks 1795–1805 is the recommended separately authorised next investigation.

## Fifteen-minute soak

Seven complete process laps run for **929.43 s**, including load/boot/legacy;
actual native exposure is **493.18 s (8.22 min)** across 29,449 native pictures.
The original reset entry does not return and replay completion exits, so the
existing per-process loop is retained without a simulation-reset rewrite.
All seven laps pass all 21 authoritative sets. Every lap has zero settled
underruns, inferred repeats and main gaps >50 ms. Full-native shortages total
6,158 sample frames, including handover carry-in; those are not erased.

| Wall-time bucket s | Native exposure s | Repeats | Settled underruns | Main gaps >50 ms | Peak °C | CPU/GPU cooling |
|---|---:|---:|---:|---:|---:|---|
| 0–300.00 | 140.91 | 0 | 0 | 0 | 62.837 | 0 / 0 |
| 300–600.00 | 147.02 | 0 | 0 | 0 | 63.080 | 0 / 0 |
| 600–900.00 | 176.61 | 0 | 0 | 0 | 64.133 | 0 / 0 |
| 900–929.43 | 28.64 | 0 | 0 | 0 | 63.323 | 0 / 0 |

All 9,271 session samples report four CPUs at 1512 MHz, GPU at 240 MHz and both
cooling devices at 0; peak sampled temperature 64.133°C. No sampled throttling or
rising repeat/settled-underrun trend occurs. The low-impact observer uses about
4.52% of one core. Sample interval median/p99/worst is 100.011/123.608/3211.462 ms;
coverage gaps are retained, with zero overlap with native game gaps. No >50 ms
game gap remains to attribute. This bounded soak does not establish permanent
elimination of P2's rare gap. The user answered **“Yes”** when asked whether settled motion and audio were
clean during the P3 soak; no additional details were supplied.

## History, limitations and remaining work

The initial attempt used `RR_P3_PRIORITY`; replay startup stripped it, so its
A1/A2 timings are invalid priority comparisons. The complete blocked report and
raw samples remain [history](benchmarks/native-p3-20261016/blocked-attempt-report.md).
The continuation uses `ENG_*` and confirms actual receipts/census before timing.
A smoke-checker setup error initially compared terminal tick 1900 with the full
reference's next tick 2100 by row index. It was corrected to compare the 18 shared
domain records and label the three terminal-1900 records; no same-tick mismatch
occurred. This configuration error was fixed without repeating a device run.

One timing per A/B variant and exactly two final timings limit repeatability
claims. Existing P2 profiling, host proofs and stage timers support these bounded
changes; no new sampling profile is presented. Preparation gains do not establish
the cause of P2's rare main-service gap. Simulation is now typically longer than
preparation in dense scenes; intermittent scheduling/presentation remains a risk.
Other courses, legacy fallback performance, broader asset coverage and product
behaviour are outside this investigation. Historical M3/P1/P1b/P2 gates are unchanged.

Full timings, repeat-event ticks/stage costs, clock/cooling histories, thread
census, checkpoint/packet/image receipts, source/build identities and every failed
or rejected observation are in the [evidence README](benchmarks/native-p3-20261016/README.md).
Percentiles are never pooled to replace a failed per-run gate. Further optimisation,
texture formats, broader coverage and productisation require separate authorisation.

## Exit gate and final state

| Gate | Result |
|---|---|
| Both final dense preparation p99 ≤14 ms | PASS: 13.705 / 13.492 ms |
| Both final settled underruns / main gaps >50 ms =0 | PASS: 0 / 0 |
| Both final inferred repeats =0 | **FAIL: 0 / 1 (tick 1800)** |
| Soak main gaps >50 ms / settled underruns =0 | PASS: 0 / 0 |
| Exact device validation | PASS: 21 sets / 4,207 packets / 36 Mali images |

The continuation completes in **48.94 minutes of device-session time**, within two hours.
All 13 completed continuation full replays pass all 21 authoritative sets; the
four short smokes have verified policies and matching shared/terminal states.
Exactly one device validation and two selected-build timings are run. All 14 P3
clock sessions (including original history) restore policies, RT defaults and ES.
Final all-47 protected hashes pass. Production SHA256 remains
`6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`.
All four CPUs are back to schedutil, min 480000/max 1512000 kHz; GPU
simple_ondemand/min=max 0; RT limits 950000/1000000 µs. EmulationStation PID14605
is restored; no diagnostic game, sampler or perf remains. Branch HEAD is unchanged.
No commit, push or production deployment. **STOPPED. Further work requires approval.**
