# P3 — Scheduling robustness and preparation headroom

**Status: BLOCKED / STOPPED EARLY. Exit gate NOT MET (required tests unperformed).**

The priority switch used `RR_P3_PRIORITY`, which replay startup removes. The census
caught this: A1/A2 actually ran with default priorities. This is an implementation
configuration error, not evidence that nice or real-time scheduling failed to help.
Per the user’s “if something fails, stop and report rather than extending the matrix”
rule, no replacement scheduling runs or further device experiments were started.

| Run/step | Dense prep median / p99 ms | Repeats | Settled underrun frames | Main gaps >50ms | Status |
|---|---:|---:|---:|---:|---|
| control | 13.959 / 14.921 | 0 | 0 | 0 | P2 control |
| a1 | 13.998 / 14.728 | 0 | 0 | 0 | INVALID: requested priority absent |
| a2 | 13.739 / 14.544 | 1 | 0 | 0 | INVALID: requested priority absent |
| B1 native projection | — | — | — | — | Host proofs/parity PASS; no device timing |
| B2 conservative culling | — | — | — | — | Host proofs/parity PASS; no device timing |
| Final validation / two timings | — | — | — | — | Not run; no final build selected |
| 15-minute soak | — | — | — | — | Not run after blocker |

## Scope and measurement

Started clean `feature/knulli-h700` at `fdf4d15c44686b3f497b26d6ece47b3b7275ed2f`.
P2 selected b control SHA256 is
`7f313d01fd5c76c8a0af9babc4e60a794968a85f01fca086b829bf242fccf5c3`.
Only private `native/p3` sources/build derivations were added; no shared engine,
simulation, physics, audio algorithm, shaders, fallback or asset-format changes.
Queue2 strict FIFO/panel pacing, valveOFF, O2/stack protection, no affinity,
P1b asynchronous output/warmup/gradual release and GPU stock remain intact.

Reference all6006ticks; native1800–6006, dense3550–3590 inclusive. Nearest-rank
percentiles retain every sample;41dense pictures make per-run p99 equal maximum.
Underruns are sample frames. Settled counter is6006 minus actual release tick2112.
Inferred repeats use unchanged P1b rounded swap-return intervals at59.7104Hz;
they are not direct scanout measurements. The full intervals and stage times remain.

CPU fixed1512MHz userspace, GPU simple_ondemand. All three native runs are unflagged,
with four CPUs1512MHz, GPU240MHz and both cooling states0 at every sampled point.
Starts are48.743/53.360/52.226°C, within the new≤55°C rule;180s idle fallback unused.
Every run retains preflight MemAvailable and/tmp usage. No game output goes to/tmp,
and no transfers/archive overlap timed games. Clock sampler nominal10Hz plus1Hz
all-thread census is identical across the three runs; process observer otherwiseOFF.

## Part A — priority-switch failure

[Replay startup](src/rr_headless.inc) lines178–188 deliberately strips all`RR_*`
and`SND_*`environment variables. The private switch therefore disappeared before
native activation. There are no`P3-PRIORITY`receipt lines, and all three game
stages remain SCHED_OTHER/nice0 throughout native census samples in A1/A2.
The requested-mode text file alone is insufficient evidence of actual scheduling.
The switch code exists in the tested executable; its entry branch correctly exits
when the environment is unset. No nice/RT promotion actually occurred.

The source-only correction uses **`ENG_P3_PRIORITY=nice|rr`**, defaultOFF. It is
not transferred or hardware-validated. Private scripts and source now use that name;
[tested original header](benchmarks/native-p3-20261016/build/priority-header-tested.txt)
and binary identities preserve exactly what ran. Native-only activation after
startup avoids unintentionally promoting newly created audio/driver threads.
A future retry must verify three actual policy receipts and all-thread census
before interpreting timings. No shared replay sanitisation is changed.

All14census identities per run retain baseline policies, including Mali workers
SCHED_OTHER/nice0, SDLAudioP2 SCHED_OTHER/nice−20 and existing PipeWire data-loop
SCHED_FIFO83. Their CPU/runqueue/context-switch records are retained. There is
no observed promotion-related starvation because promotion never happened;
starvation under A1/A2 and interactive input responsiveness remain untested.
RT defaults stay950000/1000000µs; snapshots match before/after every run.

Observed default-priority service distributions (median/p95/p99/worst ms):

| Run | Window | Preparation | Simulation |
|---|---|---|---|
| control | native | 9.610 / 12.401 / 13.928 / 16.818 | 11.607 / 13.460 / 14.212 / 18.347 |
| control | dense | 13.959 / 14.500 / 14.921 / 14.921 | 13.544 / 14.215 / 14.459 / 14.459 |
| a1 | native | 9.605 / 12.297 / 13.998 / 16.807 | 11.516 / 13.436 / 14.198 / 17.623 |
| a1 | dense | 13.998 / 14.576 / 14.728 / 14.728 | 13.420 / 14.310 / 14.551 / 14.551 |
| a2 | native | 9.579 / 12.349 / 14.078 / 17.083 | 11.587 / 13.515 / 14.280 / 17.665 |
| a2 | dense | 13.739 / 14.494 / 14.544 / 14.544 | 13.412 / 14.292 / 14.589 / 14.589 |

| Run | Swap interval median/p95/p99/worst ms | Full native / settled underrun frames |
|---|---|---:|
| control | 16.746 / 16.836 / 17.124 / 18.990 | 898 / 0 |
| a1 | 16.745 / 16.850 / 17.147 / 18.894 | 898 / 0 |
| a2 | 16.745 / 16.863 / 17.181 / 32.570 | 1629 / 0 |

A2-labelled run repeats once at tick1857 (32.570ms interval; simulation12.413,
prep11.146, draw14.048, swap17.855ms). This is a default-priority observation,
not an RT-induced result. Signed paired differences are retained but must not
be interpreted as nice/RT benefits. No priority variant is selected.

## Part B — host-validated, not adopted

**B1:** checked64-bit multiplication and×16 range checks permit exact int64 shift,
division and checked base add/subtract. Overflow, including INT64_MIN/−1 division,
falls back to the original128-bit expression. Shifts64–127 preserve arithmetic
sign-fill; original valid shifts0–127, unsigned left shift and final saturation
semantics remain exact. UBSan passes **28,449,792 Cartesian boundary cases plus
100,000,000 random inputs**; half the random population stresses geometry-size
fast-path operands, half full-width operands. This covers both negate choices,
zero/negative divisors, clamp/overflow boundaries and every defined shift.
ARM disassembly confirms additional SDIV paths while retaining wide fallback calls;
static instruction counts are not claimed as performance savings.

The fast path is gated strictly around the native scene walk, with a thread-local
flag reset at function entry, normal completion and unsupported longjmp handling.
Legacy/fallback uses the original projection function. Both the initial prototype
and the tightened native-only guard have separate retained host replays; neither
B build was transferred to the handheld.

**B2:** retains exactly the original conservative float32 error enclosure. For
positive finite zoom and positive Z bounds, numerator/divisor monotonicity selects
the four extrema directly, halving corner divisions without reciprocals or
reassociation. Initial±1e100 clamps and reason precedence remain. Near-plane
returns are unchanged; negative/nonfinite zoom uses the original corner loop.
UBSan differential testing passes one million quads, exercising all five reasons.
Validation builds also run old/new reason functions on every real replay quad
and abort on any disagreement. Exact packet parity checks flags/order as well.

Both native-only B host builds finish6006ticks with **all21sets/63domain records,
4,207exact packets and36sampled images** matching the original host reference.
These are host software-GL image checks, not newly captured Mali validation.
[Host evidence](benchmarks/native-p3-20261016/host-parity.json) and test/source
identities are retained. The unchanged projection helper’s unit proof remains
applicable after adding the native-only guard. No step is kept as a measured
optimisation: their required paired dense device measurements did not occur.

## Final and soak

No final build is selected. **Zero device validation-build replays** were run;
exactly one remains required if work resumes. Final two timings and15-minute soak
were not started. The intended fast protocol reuses the selected variant’s timing
as final-1, then runs only one additional final timing, avoiding a third sample.
No optional perf capture, extended matrix or thermal soak was started.
P2 performance results and historical failed gates remain unchanged.

## Remaining work and disposition

The immediate blocker is the switch namespace, not a diagnosed scheduling or
preparation limitation. Source correction is reviewable under native/p3 but remains
unmeasured. A separately authorised continuation should first verify actual nice/
SCHED_RR policies, then perform the uncompleted bounded matrix. Current results
cannot establish whether priority prevents the P2 main-service gap, whether either
B step improves dense preparation, or whether the P3 presentation/audio gate passes.
No claim of smoothness or14ms success is made.

All three completed device replays pass all21authoritative state/audio sets;
play builds deliberately produce no packet/image receipts. No samples are removed.
Setup-only name/exFAT extraction issues and invalid priority runs remain retained.
Raw evidence, all service distributions, thread census summaries, build logs,
unit proofs and host parity are in the [evidence README](benchmarks/native-p3-20261016/README.md).

## Final integrity and stop

Device session elapsed **12.88minutes**, including host-analysis idle/reconnection,
well below the2hour target; only3device replays occurred. All47protected hashes
pass before and after. Production SHA256 remains
`6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`.
CPU governors/min/max restored to schedutil480000–1512000kHz; GPU simple_ondemand
with original min/max0. RT defaults unchanged; EmulationStation PID6823 running,
no game/sampler/perf remains. Archive hashes verify recovered evidence. Source
branch/HEAD unchanged; only private P3 files/report/evidence and HANDOVER updated.
No commit, push or production deployment. **STOPPED; continuation requires explicit approval.**
