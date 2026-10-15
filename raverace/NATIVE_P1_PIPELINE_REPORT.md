# Milestone P1 — hybrid three-stage vsync prototype

**Investigation COMPLETE under the user-reduced scope; performance exit gate FAIL. STOPPED.**

The isolated hybrid pipeline preserves the reference replay and sampled visuals,
releases legacy GPU resources, and reaches panel-paced median throughput. It does
**not** achieve zero repeated vblanks or clean audio. Long stalls remain, preparation
has a p99 tail above one panel period, and diagnostic GPU completion p95 exceeds
14 ms. These results do not pass M3 retrospectively or authorise P2.

The user deferred both 30-minute soaks and reduced the original repeated matrix to
one unfenced plus one diagnostic-fence replay per configuration: eight runs total.
Repeatability and sustained thermal stability are **untested**, not passed.
No new hardware runs were performed for this final analysis.

## Implemented ownership and scheduling

Private builder: [native/p1/build.py](native/p1/build.py). It relinks private source
copies under ignored `native/generated/p1/`; existing game/physics/AI/collision,
input, simulation tick, audio and resampler algorithms remain unchanged. Generated
68K/sound sources use the previously validated O2 objects with stack protection;
`--o1` retains the original generated-source option. Affinity remains OS-managed.

- Main owns simulation, audio generation, input, FIFO latch/consume and immutable
  snapshot publication. Palette, mixer, text, polygon/point RAM and direct FIFO
  data are copied into the original bounded presentation slots.
- Existing `rr-prepare` walks/compiles only the slot snapshot and prepares that
  picture's HUD. Native packet storage belongs to its slot. Compiler scratch and
  its current tick remain preparer-owned; they are not consumer handoffs.
- Existing `rr-present` owns the sole GL context, native/legacy submission, HUD
  and swap. Three attribute/instance/dynamic resource copies have reuse fences;
  the consumer waits before overwriting an in-flight copy. No per-frame glFinish
  runs in the normal mode. Startup completion remains; `ENG_P1_FENCE=1` explicitly
  enables diagnostic per-picture completion fences.
- `ENG_P1_QUEUE=2|3` selects bounded strict FIFO. Full queues block main; every
  tick is presented once in order, with zero newest-wins drops, no 6 ms
  coalescing and no wall-clock catch-up/resync. Swap interval is 1. Panel pacing
  at the unchanged measured 59.7104 Hz is the accepted P1 policy (~0.32% slower
  than arcade), not an assertion that every frame meets that cadence.
- Every slot carries a legacy/native tag. Missing native mesh/material/format
  returns a legacy picture rather than aborting or omitting objects. Once native
  rendering starts and no prepared legacy picture remains, the legacy world
  backend/cache is released; fallback reinitialises it on demand. Shared HUD
  resources and CPU ROM metadata remain available. No transition drain is added.

The reference activation remains tick1800. This is not a general race-state
classifier or full native game coverage. City testing below exercised legacy
fallback, not native City rendering. Existing diagnostic strings `queue=latest`
and arcade `target_hz` are inherited labels, not the prototype's FIFO/pacing policy;
ordered per-picture evidence and the private source establish the actual policy.
Clock selection is an explicit runner `stock|both` argument, never a default
production change. The runner traps restoration on exit; both requests CPU
performance and GPU696MHz, with original governor/min/max values saved first.

## Correctness and visual gate

| Check | Result |
|---|---|
| Host full6006, queue2 and3 | All21 CPU/RAM, board/video and PCM/voice checkpoint sets (63 records) identical |
| Native reference packet parity | Exactly4207 packets, 267,308,900 bytes, ticks1800–6006, both host and ARM queue depths |
| Eight measurement replays | Each6006 ticks presented exactly once in order, zero drops, all21 checkpoint sets identical, exit0 |
| Mali sampled visuals | Each queue depth:36 world/composed RGBA readbacks byte-identical to validated prior Mali renderer |
| Host forced missing mesh at2501 | One legacy fallback, recreation then native resume; all21 sets pass, other4206 packets identical,36 sampled images match |
| Manual City course | Racing/all controls work;9806 unsupported pictures fall back, no exit or observed image corruption; audio stutters and several long stalls |

All reference measurement runs report native4207, unsupported0, one legacy
release, zero recreations. The forced fallback separately proves the release /
recreation round trip. Manual City reports native0: its first missing mesh is
code1616 / packet1199478. It establishes fail-soft playability, not broader asset
coverage. The initial fixture using `RR_P1_*` did not exercise its requested
switch because replay startup clears `RR_*`; it is retained as evidence, and the
successful replacement uses `ENG_P1_*`. Build failures and the manual test's
initial ignored SIGINT watchdog are retained; SIGTERM ended it gracefully and
is now the runner cleanup signal.

## Measurement method and limits

Reference recording: `race-20261013-122242-25136.rec`, full6006 ticks; every metric
below uses all4207 pictures at ticks1800–6006 unless labelled whole-run. No warmup,
slow frame, transition or outlier is excluded. p95/p99 are nearest-rank; medians
use the ordinary sample median. Configurations alternated stock-q2, both-q3,
stock-q3, both-q2, with one normal and one separate fence run per configuration.
There were no concurrent evidence transfers or archival during timed games;
archival ran between games. Each run has preflight memory and /tmp records,
checkpoint/tick/thread CSV, complete logs and a persistent sampler stream.

Normal and fence populations are never pooled. Fence runs explicitly disturb
CPU/GPU overlap and cadence; they attribute service costs, not normal smoothness.
Full state checkpoints are retained even in the timing runs; their CPU/I/O cost
is included. These are instrumented runs, not uninstrumented gameplay claims.

Presentation time is the SDL swap return. Repeated vblanks are **inferred** as
`max(0,round(interval / 16.7475 ms)-1)` for every interval, not measured by an
independent physical scanout counter. Short returns and driver jitter make this
an estimate. Snapshot age ends at swap return, a **scanout proxy**, not an exact
panel timestamp. Thus physical zero-repeat/scanout-age acceptance is unproven;
the observed long gaps and audible stutter independently preclude a pass.

The 20Hz single-process sampler reads RSS/HWM, Mali pages, MemAvailable/CmaFree,
clocks/thermal/cooling, VM reclaim/faults, PSI, process I/O, diskstats, thread
state/wchan and D-state stacks. Median interval is ~50ms, but **continuous ≥10Hz
coverage fails**: pressure reads/observer operations cause gaps up to3.803s across
this matrix. All gaps remain; catch-up samples do not recover the missing state.
Per-file sample timestamps are not recorded, so coincident evidence from a slow
sample is only coarse association. No unobserved stall cause is asserted.

## Unfenced presentation and audio

All times in ms, shown median / p95 / p99 / worst. Underruns count missing stereo
output **sample frames**, not callbacks or separate stutter events; native-window
counts subtract the snapshot counter at1799 from6006. Whole-run audio totals are
also retained in every raw log. A counter can be slightly stale at publication.

| Clocks | Queue | Swap interval: med / p95 / p99 / worst | Inferred repeats | Underrun sample frames | Native main gaps >50ms |
|---|---|---|---|---|---|
| stock | 2 | 16.747 / 17.006 / 18.081 / 2530.674 | 286 | 234664 | 3 |
| stock | 3 | 16.747 / 16.875 / 17.644 / 2719.674 | 166 | 136915 | 2 |
| both | 2 | 16.746 / 17.264 / 18.184 / 199.391 | 16 | 17284 | 2 |
| both | 3 | 16.746 / 17.280 / 18.176 / 271.195 | 26 | 26706 | 4 |

Mean native-phase presentation rates (all intervals included):
stock-q2: 55.887 pictures/s; stock-q3: 57.446 pictures/s; both-q2: 59.441 pictures/s; both-q3: 59.342 pictures/s.

Both-clock queue2 is the best **observed single run**, not a repeatability claim.
It still has16 inferred repeats,17,284 missing audio sample frames (~0.360s at
48kHz) and a199.391ms worst swap interval. Stock queue2 has286 inferred repeats
and a2.531s worst interval; stock queue3 has166 and2.720s. The user independently
observed sound stuttering on low frame rates and long stalls without corruption.
No configuration passes the zero-repeat / clean-audio target.

The accepted panel period itself exceeds16.693ms. Counts above the old arcade
period are therefore reported separately in each summary, not interpreted as
missed panel vblanks. The old59.904Hz/M3 targets are not silently substituted for
the user-selected P1 panel cadence.

## Stage service times and queue latency

Normal mode wall service times, ms median / p95 / p99 / worst. Simulation excludes
explicit FIFO blocking but includes scheduler/logging/checkpoint work between
boundaries. Preparation includes walk/compiler and small HUD/ownership work.
GL thread CPU includes the existing draw/swap call path and driver CPU; draw wall
includes resource-reuse waits, not GPU completion. The separate GPU table follows.

| Clocks | Queue | Stage | med / p95 / p99 / worst |
|---|---|---|---|
| stock | 2 | Simulation | 11.791 / 13.549 / 14.617 / 2541.845 |
| stock | 2 | Native walk | 3.563 / 4.677 / 5.199 / 9.492 |
| stock | 2 | Native compiler | 8.317 / 11.268 / 13.120 / 18.786 |
| stock | 2 | Prepare wall | 11.943 / 15.660 / 18.019 / 28.379 |
| stock | 2 | Prepare thread CPU | 11.661 / 15.071 / 17.526 / 28.308 |
| stock | 2 | GL thread CPU | 4.340 / 6.148 / 6.752 / 100.293 |
| stock | 2 | GL draw wall | 3.998 / 6.128 / 7.175 / 102.716 |
| stock | 2 | Swap call wall | 12.202 / 13.343 / 13.602 / 14.151 |
| stock | 2 | Snapshot copy | 0.805 / 1.013 / 1.214 / 2.609 |
| stock | 3 | Simulation | 11.791 / 13.745 / 14.916 / 2720.573 |
| stock | 3 | Native walk | 3.641 / 4.699 / 5.237 / 12.543 |
| stock | 3 | Native compiler | 8.568 / 10.856 / 12.868 / 23.110 |
| stock | 3 | Prepare wall | 12.270 / 15.427 / 17.725 / 35.774 |
| stock | 3 | Prepare thread CPU | 12.000 / 14.926 / 17.343 / 34.833 |
| stock | 3 | GL thread CPU | 4.677 / 6.488 / 7.115 / 96.417 |
| stock | 3 | GL draw wall | 4.322 / 6.396 / 7.413 / 97.729 |
| stock | 3 | Swap call wall | 11.793 / 13.107 / 13.419 / 14.863 |
| stock | 3 | Snapshot copy | 0.966 / 1.277 / 1.671 / 3.869 |
| both | 2 | Simulation | 11.851 / 13.673 / 14.515 / 24.804 |
| both | 2 | Native walk | 3.983 / 4.899 / 5.305 / 9.787 |
| both | 2 | Native compiler | 7.635 / 10.184 / 13.219 / 16.722 |
| both | 2 | Prepare wall | 11.700 / 15.032 / 17.983 / 21.300 |
| both | 2 | Prepare thread CPU | 11.378 / 14.733 / 17.797 / 19.737 |
| both | 2 | GL thread CPU | 5.569 / 6.576 / 6.957 / 98.417 |
| both | 2 | GL draw wall | 5.105 / 6.254 / 7.169 / 100.760 |
| both | 2 | Swap call wall | 10.817 / 12.167 / 12.852 / 14.564 |
| both | 2 | Snapshot copy | 1.321 / 1.821 / 2.110 / 2.953 |
| both | 3 | Simulation | 11.790 / 13.659 / 14.679 / 308.609 |
| both | 3 | Native walk | 4.368 / 5.480 / 6.109 / 7.656 |
| both | 3 | Native compiler | 7.826 / 10.696 / 13.189 / 15.960 |
| both | 3 | Prepare wall | 12.285 / 15.619 / 18.020 / 21.836 |
| both | 3 | Prepare thread CPU | 11.979 / 15.035 / 17.691 / 20.015 |
| both | 3 | GL thread CPU | 5.716 / 6.764 / 7.118 / 95.807 |
| both | 3 | GL draw wall | 5.234 / 6.611 / 8.199 / 96.988 |
| both | 3 | Swap call wall | 10.572 / 11.998 / 12.569 / 14.544 |
| both | 3 | Snapshot copy | 1.612 / 2.562 / 3.248 / 5.041 |

| Clocks | Queue | Snapshot→swap age ms | FIFO wait ms | Total FIFO wait s | Occupied-slot histogram | Native ratio range |
|---|---|---|---|---|---|---|
| stock | 2 | 33.376 / 33.656 / 35.735 / 130.178 | 3.847 / 5.721 / 6.280 / 90.749 | 16.327 | {'2': 4205, '1': 2} | 1.000392361–1.004824653 |
| stock | 3 | 50.109 / 50.246 / 52.539 / 142.669 | 3.661 / 5.550 / 6.154 / 84.604 | 15.522 | {'3': 4201, '1': 1, '2': 5} | 0.999920139–1.004579861 |
| both | 2 | 33.378 / 33.899 / 35.757 / 215.991 | 3.188 / 5.147 / 5.729 / 198.741 | 13.774 | {'2': 4204, '1': 3} | 1.001432292–1.004579861 |
| both | 3 | 50.108 / 50.653 / 53.783 / 304.515 | 2.943 / 5.067 / 5.594 / 82.720 | 12.673 | {'3': 4181, '2': 24, '1': 2} | 0.998246528–1.004579861 |

Queue3 adds about one panel period of snapshot age (50.1ms versus33.4ms) without
eliminating repeats. Simulation blocking is intentional pacing, not hidden
catch-up. A repeat retains the last display picture while no new swap completes;
no simulation ticks are skipped. Snapshot publication occupancy is almost always
full; it is not a continuous time-weighted queue-length histogram.

The existing resampler's native ratios remain roughly0.998–1.005, consistent with
small rate correction, but observed underruns mean drift-free **clean** playback
is not established. Whole-run ratios sometimes reach0.98 during boot/stalls;
all runs report zero overrun samples and zero latency resets. No audio logic was
changed to hide stalls or stretch missing audio.

## Separate diagnostic GPU completion

One-fence world-plus-HUD completion and final glFinish wait, ms. Every4207 sample
is retained; fence timing is not subtracted from normal samples or advertised as
asynchronous steady-state GPU execution time.

| Clocks | Queue | World+HUD completion: med / p95 / p99 / worst | Final wait: med / p95 / p99 / worst | Inferred repeats (diagnostic) | Underrun sample frames |
|---|---|---|---|---|---|
| stock | 2 | 15.739 / 16.287 / 17.823 / 25.295 | 12.119 / 13.795 / 14.638 / 20.786 | 194 | 20234 |
| stock | 3 | 15.898 / 16.317 / 17.686 / 26.337 | 12.565 / 13.789 / 14.669 / 21.940 | 180 | 19972 |
| both | 2 | 9.116 / 16.091 / 16.323 / 22.694 | 5.707 / 13.264 / 13.786 / 18.509 | 1108 | 340929 |
| both | 3 | 9.537 / 16.131 / 16.379 / 21.688 | 6.003 / 13.274 / 13.759 / 18.704 | 1078 | 348914 |

GPU max reduces median fenced completion to9.116–9.537ms from15.739–15.898ms,
but p95 remains16.091–16.131ms. Therefore even this diagnostic evidence does not
meet the historical ≤12ms median / ≤14ms p95 pair. Fence runs have different
CPU contention and pacing; their repeat counts are not normal-mode performance.
Aggregate whole-run reuse-fence waits in unfenced runs are98.736/117.642ms stock
q2/q3 and178.462/220.464ms both q2/q3. There is no per-frame unconditional finish
in normal mode; these waits protect resource reuse.

## Memory, clocks and transition

Whole-process RSS peak and minimum system MemAvailable cover startup through
shutdown, including retained legacy textures and native preload. Transition
medians compare2.0–0.1s before first native publication to2–5s after it. Mali pages
are the driver's global allocation count ×4KiB; **do not add it to RSS** as though
it were an independent process charge. Changes also include driver purge/cache
activity, not just the explicitly deleted legacy objects.

| Clocks | Queue | RSS peak MiB | Min available MiB | Min CMA MiB | Transition RSS MiB | Transition Mali MiB | Peak °C |
|---|---|---|---|---|---|---|---|
| stock | 2 | 731.38 | 136.02 | 0.97 | 728.73→676.76 | 589.16→536.70 | 64.133 |
| stock | 3 | 740.61 | 126.43 | 2.06 | 738.20→681.65 | 592.67→536.15 | 66.644 |
| both | 2 | 732.86 | 134.25 | 2.22 | 730.10→675.67 | 590.49→535.72 | 67.778 |
| both | 3 | 737.72 | 129.12 | 2.20 | 735.05→680.67 | 590.23→535.49 | 66.401 |

Unfenced transition RSS falls51.97–56.56MiB, Mali allocated pages fall52.46–56.52MiB.
However system MemAvailable medians fall2.2–2.9MiB over these same windows because
other allocations/page-cache activity offset release; freed mapped bytes are not
a one-for-one system headroom increase. Native-phase minimum available is126.43–
136.02MiB in stock and129.12–134.25MiB in both-clock runs. Stock-q2 CMA reaches
~0.97MiB: contiguous allocation headroom remains a concern despite general free
memory. No OOM or unexpected exit occurred in these eight runs.

The unchanged native static-resource request is482,760,448 bytes (~460.40MiB),
within512MiB. Three snapshots plus three512KiB native packets reserve about4.89MiB
before small ownership metadata, within the proposed8MiB incremental CPU-buffer
budget; queue2 uses fewer active slots. The extra two GPU upload-resource copies
request458,752 bytes (~0.44MiB). Compiler scratch remains bounded. Original logs'
`extra_hud_bytes` is an unconditional accounting formula, **not** proof CPU RGBA
planes are allocated; GPU-HUD mode uses per-picture prepared data. Startup peaks
and legacy fallback still require both architectures' resources. City exercised
legacy retention; the forced fixture proved lazy recreation, not arbitrary-course
peak-memory coverage.

All stock GPU samples are240MHz; both-clock GPU samples are696MHz. All CPU cores
are sampled in raw evidence; CPU0 frequency counts and cooling-state counts are
included in summaries. Short-run maximum temperature is67.778°C. CPU cooling
state stays0; stock queue3 has some GPU cooling-state1 samples despite current
frequency remaining240MHz. Neither this nor short both-clock state0 samples
establish sustained no-throttle behaviour. Both30-minute soaks were explicitly
deferred. No affinity or display-timing changes were made.

All eight preflight /tmp values are5364KiB. Logs, CSVs, sampler output and captures
use `/userdata/system/rave-racer-native-p1-20261015`, mounted on persistent
FUSE/exFAT storage. Private working directories contain relative outputs. No
capture/log/diagnostic file is directed to RAM-backed /tmp during timing runs.
Driver/SDL internal allocations and system services are outside that file-path
claim. The sampler itself adds CPU/I/O work; its gaps preclude a low-overhead
observer guarantee.

## Stalls and causal evidence

Every >50ms main publication gap, including boot/menu gaps, is retained in
[all-main-progress-stalls.csv](benchmarks/native-p1-20261015/all-main-progress-stalls.csv)
and per-run `stalls.json.gz`, with VM/PSI/I/O/thread/stack observations. Normal
native gaps are:

| Clocks | Queue | Tick: publication gap ms |
|---|---|---|
| stock | 2 | 1802: 104.912, 2761: 2201.160, 4441: 2543.145 |
| stock | 3 | 1803: 98.484, 2521: 2722.347 |
| both | 2 | 1802: 101.443, 3236: 212.749 |
| both | 3 | 1803: 97.737, 2874: 87.366, 4676: 310.509, 5401: 82.265 |

- **Transition:** tick1802/1803 has~98–105ms main gaps, mostly FIFO wait; the GL
  consumer's draw/driver tail is~97–103ms. Legacy release and the first native
  submission coincide. This is not normal simulation CPU cost; individual
  delete/upload/driver mechanisms are not separated by this instrumentation.
- **Captured D-state/page faults:** main is observed in`__lock_page_killable` /
  `filemap_fault` at boot/code-loading scenes around80 and1346/1347, with major
  faults/read I/O and sometimes background reclaim. These stacks support a
  file-backed page-fault cause for those observed waits. They do not prove the
  cause of a different multi-second race stall.
- **Long native stalls:** stock q2 ticks2761/4441 and q3 tick2521 contain2.2–2.72s
  simulation-boundary gaps without corresponding major-fault/direct-reclaim/
  allocstall increments in the retained brackets. No main D-state is captured
  inside those long gaps. Their60n+1 boundaries implicate the preceding periodic
  main-thread logging region, but no syscall trace proves a blocked stderr write.
  Periodic logging, pressure accounting and persistent-storage waits remain
  plausible, not established, causes. FIFO wait identifies backpressure, not
  the root cause of the consumer delay.
- **Observer stalls:** slow pressure-file operations report one voluntary context
  switch with multi-second elapsed time, and occasional involuntary switches;
  write flushes also stall. This is evidence of observer blocking/descheduling,
  not proof that the renderer was busy for the same duration. A single sample's
  fields straddle such waits. Timing samples are retained unchanged; missing
  thread observations cannot be manufactured retrospectively.

The historical2.74s D-state stall is **not root-caused** by this experiment.
Comparable multi-second delays recur, but their missing kernel-state coverage
prevents equivalence. No speculative memory, logging, compiler, shader or
texture-format fix was applied during final analysis.

## Bottleneck and exit decision

The pipeline exposes real overlap: normal median simulation is~11.8ms and prepare
~11.7–12.3ms while GL submission/swap proceeds on its own worker. Median swap
cadence is close to the panel period instead of summing all serial stages.
Nevertheless native preparation p99 is17.7–18.0ms, above16.7475ms, before additional
snapshot/main contention. Compiler p99 is~12.9–13.2ms plus walk~5.2–6.1ms; these
quantiles cannot simply be added as a matched-frame p99, but full prepare p99
confirms the deficit. Dense-scene GPU completion p95 is also~16.1ms even with
GPU max in the diagnostic population. Thus native preparation is the main
normal CPU-stage tail limitation; GPU/driver service and transition/long host
stalls remain co-limiters. More queue depth adds age rather than fixing service
cost. These data do not isolate compiler algorithms as the cause of the long
multi-second gaps.

| Exit item | Decision |
|---|---|
| Bounded three-stage hybrid prototype | Implemented and isolated |
| Full replay / all21 state/audio sets /4207 packets | PASS |
| Sampled Mali visual fidelity | PASS at36 readbacks per queue depth; not exhaustive imagery |
| No tick drops / graceful unsupported fallback | PASS in reference, host fixture and manual City scopes |
| Legacy GPU release / recreation | PASS; measured RSS/Mali reduction, remaining headroom limitations above |
| Zero repeated native vblanks at best clocks | FAIL in instrumented single-run evidence; exact physical counter unavailable |
| Clean audio / no underruns | FAIL; manual audible stutter and measured missing output samples |
| Continuous ≥10Hz stall coverage / old D-state root cause | NOT MET; observer gaps and incomplete causal evidence |
| Repeatability / thermal soaks | DEFERRED by user; no pass claimed |
| Production integrity / clocks / EmulationStation | PASS at shutdown checkpoint |

The most defensible next investigation would first isolate persistent logging /
pressure-sampler stalls with bounded nonblocking diagnostic output and timestamped
syscall evidence, then evaluate preparation and GPU tails using a reliable
observer. Compiler/texture-format P2 work, renewed repeats/soaks, architecture
changes and production deployment require separate authorisation. This is a
recommendation only; none was begun.

## Integrity, evidence and stop

Production executable SHA256 remains
`6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`.
All47 protected files pass the final retained manifest check (executable, assets,
launcher/configuration, saves/replays where present). Every run has matching
before/after clock and production hashes and a restored EmulationStation record.
After the reduced matrix, GPU governor/min/max are`simple_ondemand/0/0`, all four
CPU governors`schedutil`, EmulationStation PID7711; no game/sampler remains.
The scheduler was stopped before round2 and persistent evidence was synced.
Immediately before retrieval the same ES/no-game state was confirmed. A redundant
later SSH recheck lost its connection and was cancelled; final analysis relies on
the already retrieved restoration records, not a claimed new device check.

Prototype device binary SHA256:
`7b7ac2d7e8fdf36d491182d9ccbe35cea831c1c70b9fc4ffb1260d70b02aacd5`.
Starting HEAD remains`c98d2b20f5e946a0e45e98125332215332fb0c12` on
`feature/knulli-h700`; no commit/push or production deployment. Source changes
are private P1 helpers plus this report/evidence and HANDOVER. Original simulation,
physics, audio, saves, replays and controller configuration are untouched.

Reproduction, raw telemetry and validation receipts:
[benchmarks/native-p1-20261015/README.md](benchmarks/native-p1-20261015/README.md).
ROM-derived packet bodies and pixel readbacks remain in ignored local diagnostic
storage, with hashes/comparison results in the evidence directory. Every timing
sample and failed fixture/build is retained. **STOPPED at P1. No P2 or further
optimisation is authorised or started.**
