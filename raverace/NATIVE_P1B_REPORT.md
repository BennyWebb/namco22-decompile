# P1b — stall elimination and native pacing

**COMPLETE — performance exit gate FAIL — STOPPED.** Native replay only, stock
schedutil/simple_ondemand, affinity OFF, O2 unchanged, strict FIFO panel pacing.
Initial clean branch feature/knulli-h700 at95a4ad281579aac697b07a2c591d543dcc19ba46.

Implementation is private under native/p1b and ignored native/generated/p1b.
Memory-cookie output to a low-priority writer, native offscreen prewarm/resource
rotation before activation, gradual native-only cache retirement, optional valve.
Original fallback lookup/recreation, physics, audio, simulation algorithms unchanged.
No clock changes, production replacement, commits or pushes authorised.

Fixed measured-run starting SoC temperature ceiling50°C; record actual every run.
All6006 ticks,21 checkpoint sets and4207 native command receipts required per run.
Headline matrix: stock q2/q3 valveOFF ×3 alternating, q2 valveON ×3; one q2
observerON attribution run. Separate syscall tracing for remaining stalls.
Stock q2 valveOFF low-impact observer30-minute replay-loop soak authorised anew.
No timing result or deferred previous-soak pass is inferred from P1.

Implementation and bounded investigation COMPLETE. Strict performance exit gate FAIL. P2 remains unauthorised.

## Completed headline matrix

The nine observer-OFF runs are complete. All use the same isolated ARM64 binary
SHA256 `cf9721afb5e7c3152ff9954ca634a98e2228c7669252714be8ff0d3215394458`.
Every run preserves all 6006 simulation ticks, all 21 authoritative checkpoint
sets (63 records), all 4207 native packets, and every presented picture. All have
zero native main publication/source-tick gaps over 50 ms, zero dropped log records,
zero writer errors, and unchanged production hash and governor settings.

| Run | Start °C | Inferred repeats | Native underrun sample frames | Transition maximum main gap ms |
|---|---:|---:|---:|---:|
| reference-1-q2-off | 48.257 | 0 | 1451 | 33.465 |
| reference-1-q2-on | 49.229 | 0 | 1378 | 30.435 |
| reference-1-q3-off | 48.824 | 0 | 3068 | 31.426 |
| reference-2-q2-off | 48.824 | 0 | 2364 | 33.488 |
| reference-2-q2-on | 49.715 | 0 | 2406 | 31.895 |
| reference-2-q3-off | 49.472 | 0 | 3469 | 33.922 |
| reference-3-q2-off | 49.958 | 1 | 1961 | 29.579 |
| reference-3-q2-on | 49.472 | 0 | 2386 | 28.815 |
| reference-3-q3-off | 49.877 | 0 | 2147 | 30.861 |

The strict zero-audio gate **FAILS** in all three queue-2 valve-OFF runs. Missing
stereo output sample frames are counted at 48 kHz; these are not callback counts.
The total 5776 missing frames represent 0.1203 seconds across those three races.
All simulated audio checkpoint states still match; this proves emulation
correctness, not uninterrupted physical playback. No criterion is waived.

The single inferred repeat is tick 3633 in reference-3-q2-off: 27.607 ms interval,
17.368 ms preparation, 12.369 ms compiler, 4.680 ms walk, 4.366 ms draw and
9.832 ms swap wait. There is no coincident >50 ms gap or new audio deficit at that
row. The exact physical-vblank/driver cause remains unproven.

“Inferred repeat” uses round(interval / 16.7475 ms) minus one, clamped at zero.
Swap-return timestamps are a presentation proxy, not measured physical scanout.
Small sustained overruns can accumulate drift without an individually inferred
repeat; zero inferred repeats therefore does not prove perfect cadence.

## Pooled observer-OFF service distributions

Each cell is median / nearest-rank p95 / p99 / worst, in milliseconds. All
12621 native samples per configuration are retained. GL CPU is render-thread CPU
service; draw wall includes driver submission, and swap wait is reported separately.
No per-picture GPU fence is enabled in these paced runs. Snapshot age ends at swap
return, not independently measured scanout.

| Metric | Queue 2 OFF | Queue 3 OFF | Queue 2 valve ON |
|---|---|---|---|
| Swap-return interval | 16.746 / 16.935 / 17.316 / 27.607 | 16.746 / 16.885 / 17.455 / 20.956 | 16.746 / 16.897 / 17.314 / 19.150 |
| Simulation | 11.665 / 13.532 / 14.344 / 18.759 | 11.614 / 13.424 / 14.277 / 24.589 | 11.647 / 13.434 / 14.207 / 20.062 |
| Simulation blocked | 3.904 / 5.507 / 5.937 / 12.304 | 3.849 / 5.468 / 5.881 / 13.701 | 3.934 / 5.565 / 6.020 / 11.302 |
| Native walk | 3.575 / 4.553 / 4.991 / 7.913 | 3.634 / 4.595 / 5.048 / 11.263 | 3.549 / 4.544 / 5.039 / 7.207 |
| Native compiler | 8.325 / 10.270 / 12.090 / 15.764 | 8.543 / 10.630 / 12.424 / 15.095 | 8.301 / 10.280 / 12.164 / 14.091 |
| Preparation wall | 12.097 / 14.923 / 17.149 / 21.778 | 12.369 / 15.297 / 17.395 / 20.058 | 12.034 / 14.992 / 17.254 / 19.540 |
| Preparation CPU | 11.869 / 14.656 / 16.948 / 21.417 | 12.156 / 14.899 / 16.990 / 19.242 | 11.811 / 14.621 / 16.885 / 19.113 |
| GL CPU | 4.506 / 6.604 / 7.236 / 13.144 | 4.738 / 6.638 / 7.244 / 13.401 | 4.467 / 6.565 / 7.157 / 13.344 |
| Draw wall | 4.067 / 6.197 / 6.924 / 18.195 | 4.290 / 6.190 / 6.906 / 18.203 | 4.021 / 6.161 / 6.935 / 18.252 |
| Swap wait | 12.096 / 13.312 / 13.599 / 14.250 | 11.833 / 13.150 / 13.408 / 14.261 | 12.141 / 13.312 / 13.587 / 14.135 |
| Snapshot → swap age | 33.371 / 33.561 / 33.971 / 44.775 | 50.110 / 50.276 / 51.550 / 60.757 | 33.385 / 33.527 / 34.143 / 44.099 |

## Remaining limiting work

Audio deficits concentrate at handover and ticks 3550–3590. Tick-1800 audio
telemetry is captured on main before the first native compile/submission; some
of the deficit is inherited from the still-slow legacy phase. It remains counted
because the authorised analysis window starts at 1800. Legacy optimisation is
out of scope.

In the dense window, native preparation medians are roughly 17.0–17.5 ms,
including about 12 ms compilation and 4.7 ms walking. Prepared-queue waits and
swap blocking fall to about 0.05 ms: preparation underfeeds presentation.
Packet SHA receipts are computed synchronously on rr-prepare; their diagnostic
CPU cost is included, together with bookkeeping (~0.24 ms residual in a sampled
dense window). These measurements do not establish the cost of an uninstrumented
compiler. No compiler, audio algorithm or clock optimisation was undertaken.

The valve never activates in normal hardware runs; it cannot cure an underfed
queue. A separate host fixture deliberately stalls presentation and verifies
replacement accounting while retaining every simulation tick/checkpoint/packet.

## Observer result

The observer-ON queue-2 run has zero inferred repeats and no native >50 ms game
gaps. Its 1329 sample intervals have median 100.000 ms, p99 101.548 ms and maximum
110.396 ms; there are no >200 ms observer gaps and no >50 ms individual reads.
There is no coincident observer/game gap. Observer peak RSS is 5744 KiB; measured
CPU consumption is approximately 4.75% of one core. Both reader and writer use
SCHED_IDLE/nice 19 successfully. There are no output drops/errors. A single
comparison supports low observed impact, not universal non-perturbation.

## Memory and clocks in the attribution replay

The observer run peaks at 780072 KiB process RSS, with minimum MemAvailable
117072 KiB. Stable-native median RSS is 719152 KiB versus 776440 KiB in the last
legacy segment. Global Mali allocation median falls from 152218 to 137636 pages
(~56.96 MiB at 4 KiB/page); these allocations overlap process accounting and
must not be added to RSS. Legacy release completes at tick 2112. Available-memory
headroom remains modest (~115–120 MiB), so lower RSS is not treated as equivalent
to that much additional free memory. Full phase windows are in memory-phases.json.

All sampled GPU frequencies are 240 MHz; both CPU/GPU cooling states remain zero
in this replay, with peak observed temperature 61.946°C. Governors stay stock;
per-core CPU frequencies are retained in observer-timeline.csv. Soak stability
still requires its separate evidence.

## Separate syscall attribution

The full traced replay also passes all 21 checkpoint sets and 4207 packets.
Main TID 12652, prepare 12679 and present 12678 make **zero regular-file/stderr
writes, zero fsync/fdatasync calls and zero write-mode file opens** during the
native interval. Their 20792 write calls are eventfd notifications from existing
driver/IPC machinery. Thus “no file/stderr writes” is demonstrated without
claiming that GL/SDL cease all write syscalls.

Ptrace is highly intrusive here: 7590 inferred repeats, 1395 main publication
gaps >50 ms and 5329618 missing audio frames. These are retained attribution
samples and are excluded from all headline performance populations. The 129
>50 ms traced calls include 127 futex waits, a non-game ALSA control open and a
non-game eventfd write. The traced pauses do not prove the cause of the isolated
untraced soak stall. Raw per-TID traces remain in ignored generated storage;
derived critical-write/open/slow-syscall CSVs and archive digests are published.

## Continuous replay-loop soak

The stock queue-2 valve-OFF session lasts **1867.31 seconds (31m07s)**, completing
14 full 6006-tick replays: 84084 source ticks, 58898 exact native packets, and all
21 authoritative sets on every lap. Native exposure totals **987.03 seconds**.
Original-state reset uses a new process each lap, with no cooldown or ES restart
between laps. This is a continuous thermal replay-loop session, not a claim of
30 minutes of continuously resident native rendering or single-process leak
validation. Startup, legacy and native exposure are retained, not excluded.

| Wall minutes | Native seconds | Inferred repeats | Missing audio frames | Main gaps >50 ms | Peak °C |
|---|---:|---:|---:|---:|---:|
| 0.00–5.00 | 140.97 | 0 | 5138 | 0 | 61.460 |
| 5.00–10.00 | 149.41 | 0 | 5244 | 0 | 62.432 |
| 10.00–15.00 | 173.21 | 0 | 6555 | 0 | 62.918 |
| 15.00–20.00 | 170.98 | 8 | 14510 | 0 | 64.862 |
| 20.00–25.00 | 140.96 | 0 | 4682 | 0 | 62.756 |
| 25.00–30.00 | 145.26 | 3 | 7986 | 1 | 62.270 |
| 30.00–31.12 | 66.24 | 0 | 1449 | 0 | 62.270 |

Total: **11 inferred repeats, 45564 missing audio sample frames (0.94925 s), one
native main publication gap >50 ms**. All log transports report zero drops/errors.
Lap 8 has eight single-repeat events; lap 12 has one two-repeat event; lap 13 has
one single-repeat event. Every event's tick and coincident service timings are
retained in each lap's repeat-events.csv. Buckets include the partial final
67.31 seconds; rates per native minute are in five-minute-buckets.csv.

Preparation medians remain 12.02–12.11 ms across the 14 laps; there is no sustained
median slowdown. Repeats occur in later bursts, so this is not evidence of
uniformly smooth operation or a passed stability gate. Peak temperature is
64.862°C. All 18676 GPU samples are 240 MHz, CPU frequencies vary from 480 to
1512 MHz under schedutil, and both cooling states remain zero. These observations
do not show thermal throttling; they cannot exclude every scheduling or memory
contention event. Global MemAvailable minimum is 118716 KiB.

**Observer coverage limitation:** the soak's follow-child mode stayed on supervisor
PID 12882 for all samples. Its global VM/memory/clocks/thermal and own cadence are
valid; its process RSS/I/O and empty thread-stack records are **not game evidence**.
The analyser now reports this explicitly. The separately targeted observer-q2
run has valid game-process records. Soak observer intervals have maximum 114.019 ms,
with no >200 ms observer gap coincident with any game gap. That establishes that
observer sampling did not itself pause alongside the game, not causality.

The remaining stall is lap 12, tick 5848: main publication gap 50.160 ms,
source-tick completion gap 50.061 ms, queue wait 38.617 ms. The preceding tick
5846 has a 50.186 ms swap-return interval, including 15.324 ms draw wall and
34.269 ms swap wait. Prepare/present start gaps also reach about 50 ms. Between
global samples at uptime 6879.217 and 6879.317, allocstall_normal increases by 1,
pgmajfault by 4, direct scan/steal by 37 pages and kswapd scan/steal by 323 pages.
This is **coincident reclaim activity**, not proof that this process stalled in
reclaim. Missing game stacks and lack of a matched syscall reproduction prevent
root-cause attribution. The cause remains **unknown**; no speculative fix was made.
The old multi-second D-state stall is not reproduced in the headline matrix.

## Correctness, implementation and final integrity

Private sources are in native/p1b; original game/engine/generated sources and
CMake files remain unchanged. Build scripts reuse the P1 ownership/pipeline and
O2 generated objects with stack protection. No shader/compiler algorithm,
physics, audio algorithm, fallback lookup/recreation or clock change is made.
Final host queue-2/queue-3 validations pass all checkpoints, packets and 36 sampled
host image hashes. Separate Mali image validation matches all 36 prior reference
captures. Sampled equality is not exhaustive visual or other-course coverage.

A host fixture injects a 150 ms presentation delay at tick 2500: one valve
activation replaces unshown ticks 2501–2508 while preserving all 6006 simulation
ticks, all checkpoints and all 4207 packet digests. Every retained picture is
presented once in order. Normal hardware valve-ON runs never activate it.
A ring-pressure fixture appends 64 MiB in 0.558 ms, maximum append 0.0235 ms,
drops/counts 1008 overflow chunks and fully drains the identical reserved 128 KiB
checkpoint output. The writer does not acquire producer FILE locks.

The final read-only device check confirms all **47 protected file hashes**, stock
GPU simple_ondemand/min=max=0 and all four CPU schedutil governors, EmulationStation
PID 14183, and no rr_p1b, memory_sampler or strace process. Production executable
SHA256 remains `6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`.
No game output is written to RAM-backed /tmp during timed runs; preflight df/du
and memory checks are retained. No concurrent transfers/archival occur during
measured games. Scheduler-only pauses between runs allow retrieval, never a
pause of a running game. The temporary Wi-Fi outage does not stop the autonomous
soak; logs are recovered after completion and the user reconnects Wi-Fi.
No commit, push or production deployment was performed.

## Exit decision and bounded next step

**Investigation COMPLETE; P1b performance gate FAIL.** All three headline q2 OFF
runs pass the >50 ms main-gap and handful-of-isolated-repeats subgates, but every
one fails zero native audio underruns. The soak adds a two-repeat event and one
unresolved ~50 ms stall. The transition target is met narrowly in all three q2
OFF runs; q3 reference-2 reaches 33.922 ms, slightly above two panel periods.
No acceptance criteria or analysis window is relaxed.

The evidence supports keeping asynchronous output and offscreen warmup in the
isolated prototype. Remaining limits are dense CPU preparation (including the
explicit packet-digest diagnostic cost), legacy-to-native audio carry-in and
intermittent scheduling/presentation/reclaim-related stalls whose exact cause
is not established. A future authorised investigation should first correct and
validate the soak observer's child selection, then reproduce the residual stall
with reliable process/stacks and separate syscall attribution. Compiler/texture
work remains P2 and requires separate authorisation. M3 remains performance-blocked;
P1 is not retrospectively passed. **STOPPED; no P2 work begun.**
