# P2 — Fixed CPU clock, play-build preparation and compiler hotspots

**Investigation complete; performance exit gate: FAIL. STOPPED.**

The selected isolated build is **play split + palette revision/material cache (a) + selective palette gathering (b)**, `v2-b-play`, SHA256 `7f313d01fd5c76c8a0af9babc4e60a794968a85f01fca086b829bf242fccf5c3`. It reduces typical dense preparation cost and gives zero settled-native underruns in all three headline races, but dense preparation p99 is **14.310 / 14.739 / 14.811 ms**, exceeding the required **14 ms** in every headline run. The soak also retains one >50 ms main gap and 2,131 settled-native underrun frames. No failed sample is excluded and no criterion is waived. The following complete populations determine the remaining conclusions.

## Scope, methods and identities

Initial clean `feature/knulli-h700` HEAD: `49d4b30db039245329bd1ccc987cdf71bc936b01`. All work is isolated under [native/p2](native/p2/README.md); original production source files are untouched. Generated sources remain O2 with stack protection. P1b's three-stage FIFO, queue 2, valve OFF, asynchronous output, off-screen warmup, gradual legacy release, affinity OFF and original simulation/audio/fallback remain unchanged. GPU policy stays `simple_ondemand`; no texture formats, shaders, display timings or asset coverage changes.

The unchanged P1b baseline binary SHA256 is `cf9721afb5e7c3152ff9954ca634a98e2228c7669252714be8ff0d3215394458`. The reference is `/userdata/system/rave-racer-replays/race-20261013-122242-25136.rec`, all 6,006 ticks; native analysis is 1800–6006 (4,207 pictures), dense analysis 3550–3590 (41 pictures). Nearest-rank p95/p99 and every sample are retained. With 41 observations, each run's dense p99 equals its maximum. Validation, attribution and timing runs are separate populations.

Every independent measurement starts below 50°C after EmulationStation is stopped for cooling. CPU uses `userspace` at 1512000 kHz on all four cores, with original governor/min/max saved and restored by an EXIT trap. No thermal trip or power-allocation changes. A low-priority, buffered, persistent sampler records per-core clocks, GPU frequency, temperatures and both cooling states at nominal 10 Hz in every run. Headline observer OFF disables process/memory observation, not mandatory clock telemetry. Actual sampling interval distributions, rather than a perfect-rate claim, accompany every run. Full observer runs follow the explicit game PID; the soak switches an explicit PID file for each lap. A two-child fixture verifies main-thread identification.

No game logging, capture or telemetry writes to RAM-backed `/tmp`. Per-run preflight retains `/tmp` usage, MemAvailable and filesystem evidence. No transfers or archiving overlap timed games. Original CPU/RAM, board/video and PCM/voice checkpoint code remains enabled in both build modes; checkpoint algorithms, physics, input and simulation timing are unchanged.

Underruns are missing audio **sample frames**, not events. Full native is counter(6006) − counter(1799). Native-settled is counter(6006) − counter(actual legacy-release completion); the first included main counter sample is release+1. All headline releases complete at tick 2112. This separation exposes handover carry-in instead of hiding it.

An inferred repeat is `max(0, round(interval/panel_period) − 1)` per swap-return interval, using the unchanged P1b method and panel period `1000/59.7104 = 16.7474996 ms`. These are **inferred**, not scanout observations: delays below 1.5 periods can accumulate without incrementing the counter. Interval means and complete distributions are retained alongside counts. Snapshot→swap is not independently measured physical scanout latency. Service times do not add directly across concurrently executing stages.

## Validation and play-build split

Validation builds retain SHA packet receipts, sampled Mali readbacks and renderer output-consistency scans. Play builds omit packet/image digesting, GL error polling and redundant compiled-output validation scans from preparation/presentation, while retaining format/capacity handling and unsupported-picture fallback. Exact-key cache hashing remains functional lookup work. Main authoritative checkpoints remain enabled.

All six cumulative corrected validation builds (play, a, b, c, d, e) finish 6,006 ticks with **all 21 authoritative sets / 63 domain records, all 4,207 packets and all 36 Mali image hashes exactly matching** the reference. Packet coverage is 267,308,900 bytes per validation replay. Images cover world and composed output at 18 ticks, including dense/near-clipped and later scenes; they are not 36 separate HUD-only captures. Host validation also passes, including the unselected no-metadata/no-Z sorting combination. The latter was **never transferred, measured or adopted on H700**.

See [validation summary](benchmarks/native-p2-20261016/validation-summary.json), [image method](benchmarks/native-p2-20261016/validation-method.json), [exact-output reasoning](native/p2/SAFETY.md) and [build identities](benchmarks/native-p2-20261016/build/v2-binaries.json). Timing/play replays independently retain all authoritative checkpoint records; packet/image work is deliberately absent there, with equivalence established by validation builds from the same source.

## Compiler experiments and selection

Experiments are cumulative and run in the requested order. Each has its own validation replay, timing replay and separate profile; no attribution timings enter comparisons. Positive paired savings below mean before-minus-after for the same source tick, including negative differences. One intermediate pair establishes a measured observation, not a repeatability guarantee.

| Step | Change | Dense prep p99 ms | Paired dense prep saving mean / median ms | Decision |
|---|---|---:|---:|---|
| Play | Complete validation/play split | 18.810 | See repeated baseline/play matrix | Keep split |
| a | Exact palette revision / recipe cache | 14.865 | 3.172 / 3.345 vs play | Keep |
| b | Gather only referenced, changed groups | 14.310 | 0.111 / 0.093 vs a | Keep; modest single-pair benefit |
| c | ROM-derived immutable UV/recipe metadata | 15.140 | −0.409 / −0.397 vs b | Reject runtime adoption |
| d | Guarded reuse of original walk Z | 15.053 | 0.054 / 0.040 vs c | Reject: no compiler/overall benefit |
| e | Sort index permutations | 15.853 | −0.280 / −0.314 vs d | Reject |

Step a compares the three exact 256-byte planes once per palette group, advances a revision only on change and caches recipe→material by all seven signed 32-bit recipe fields plus group revision. Hash collisions cannot hit without exact key equality. Revision wrap clears cache/gather state; a full table falls back to the original resolver. It removes repeated per-quad 768-byte comparisons for unchanged groups. Step b gathers every referenced group whose current revision has not been gathered, including changes while a group was unused. HUD/LUT continue using original slot data. ARM symbol sizes verify 1,673,344 bytes (1.596 MiB) of additional static CPU cache state; see [cache memory evidence](benchmarks/native-p2-20261016/build/cache-memory.txt). GPU allocations are unchanged.

Step c exports a 510,984-byte CPU-only sidecar after checking all 15,968 resident meshes' positions and UVs against original ROM/provenance and GPU-pack bytes. It preserves output but regresses measured compiler/preparation time; no cache-miss cause is claimed without counters. The selected b build never loads it and GPU static assets remain 482,760,448 bytes, below the existing 512 MiB ceiling.

Step d only reuses original walk integer Z where a strict overflow proof holds for valid, non-near, unprotected quads. It keeps the original float32 conservative error enclosure and X/Y computation. Dense compiler paired median regresses 0.091 ms; native compiler 0.078 ms and preparation 0.068 ms. The small dense preparation median gain is insufficient evidence of an optimisation.

Step e preserves comparators, unique painter ties, protected batches, instance order and complete packet bytes. However, the matched glibc 2.40 `qsort` **already uses indirect sorting for elements larger than 32 bytes**, including these 128-byte commands. Thus the Stage 1 interpretation that sorting repeatedly moves full commands needs correction: libc already sorts pointers and then permutes records. The extra explicit permutation adds no demonstrated benefit; compiler paired medians regress 0.350 ms dense / 0.177 ms native. See [matched libc evidence](benchmarks/native-p2-20261016/sorting-libc-evidence.txt). This identifies a mistaken cost assumption without asserting an unmeasured microarchitectural cause.

The first selected b timing population is reused exactly once as `final-1`, identified by the same binary SHA and protocol. Its source directory remains `v2-timing-b`; a text manifest records the alias on the exFAT device. It is not a new/fourth trial. Two further alternating rounds complete three runs per family. All five experiments remain reproducible, including rejected ones; final adoption contains only a+b.

## Fixed-clock headline results

Three runs of each family are complete. Round 1 spans the separate compiler investigations; rounds 2 and 3 run baseline → play → final consecutively, with independent below-50°C cooldowns. The unchanged first b timing replay supplies final-1. This is an ordered comparison, not a randomised experiment; all chronology is retained in [run-order.csv](benchmarks/native-p2-20261016/run-order.csv). No repetition was silently substituted.

| Run | Native prep median/p95/p99/worst ms | Dense prep median/p95/p99/worst ms |
|---|---|---|
| baseline-1 | 11.855/15.019/18.010/22.306 | 17.725/18.482/18.851/18.851 |
| v2-play-1 | 11.758/14.762/17.050/19.274 | 17.466/18.046/18.810/18.810 |
| final-1 | 9.544/12.102/13.925/15.576 | 13.824/14.151/14.310/14.310 |
| baseline-2 | 11.801/15.032/17.620/21.687 | 17.706/18.339/19.190/19.190 |
| v2-play-2 | 11.756/14.747/16.849/20.154 | 16.772/18.025/20.154/20.154 |
| final-2 | 9.547/12.190/13.917/19.874 | 13.795/14.672/14.739/14.739 |
| baseline-3 | 11.845/14.969/17.194/19.184 | 17.265/18.106/19.122/19.122 |
| v2-play-3 | 11.780/14.802/17.090/19.731 | 17.099/18.232/18.371/18.371 |
| final-3 | 9.656/12.310/14.167/15.791 | 14.079/14.505/14.811/14.811 |

| Run | Full native / settled underrun frames | Inferred repeats | Main gaps >50ms | Release tick | Peak °C | CPU flag |
|---|---:|---:|---:|---:|---:|---|
| baseline-1 | 4311 / 3555 | 0 | 0 | 2112 | 59.19 | False |
| v2-play-1 | 1567 / 668 | 0 | 0 | 2112 | 65.43 | False |
| final-1 | 776 / 0 | 0 | 0 | 2112 | 64.21 | False |
| baseline-2 | 3149 / 2254 | 0 | 0 | 2112 | 63.40 | False |
| v2-play-2 | 875 / 0 | 0 | 0 | 2112 | 64.30 | False |
| final-2 | 898 / 0 | 0 | 0 | 2112 | 63.57 | False |
| baseline-3 | 2722 / 1227 | 1 | 0 | 2112 | 64.94 | False |
| v2-play-3 | 1976 / 1123 | 0 | 0 | 2112 | 64.94 | False |
| final-3 | 924 / 0 | 0 | 0 | 2112 | 63.97 | False |

The play split alone gives modest tick-matched preparation savings, with variable tails; it does not remove dense overload. Cache + selective gather provides the substantial gain. Paired median **before-minus-after**, in ms:

| Comparison | Round | Native prep | Dense prep | Dense compiler | Dense walk |
|---|---:|---:|---:|---:|---:|
| baseline-to-play | 1 | 0.047 | 0.644 | 0.359 | 0.049 |
| baseline-to-play | 2 | 0.076 | 0.633 | 0.218 | 0.167 |
| baseline-to-play | 3 | 0.048 | 0.132 | -0.088 | 0.027 |
| play-to-final | 1 | 2.277 | 3.565 | 3.826 | -0.364 |
| play-to-final | 2 | 2.236 | 2.962 | 3.579 | -0.482 |
| play-to-final | 3 | 2.156 | 3.250 | 3.766 | -0.507 |

Final compiler gains coexist with a measured dense walk regression. The walk algorithm is unchanged; these comparisons do not prove a specific cache/contention cause. Do not add differences of independent medians to manufacture an expected total. All signed per-tick differences and their distributions are retained.

| Run | Window | Compiler median/p95/p99/worst ms | Walk median/p95/p99/worst ms |
|---|---|---|---|
| baseline-1 | native | 8.204/10.335/12.916/16.944 | 3.444/4.496/5.072/6.167 |
| baseline-1 | dense | 12.824/13.293/13.513/13.513 | 4.781/5.113/5.223/5.223 |
| v2-play-1 | native | 8.273/10.218/12.226/13.767 | 3.470/4.540/5.006/7.176 |
| v2-play-1 | dense | 12.442/13.142/13.667/13.667 | 4.767/5.246/5.516/5.516 |
| final-1 | native | 6.021/7.594/8.740/10.426 | 3.493/4.652/5.267/7.596 |
| final-1 | dense | 8.670/8.964/9.054/9.054 | 5.119/5.327/5.684/5.684 |
| baseline-2 | native | 8.162/10.257/12.647/15.070 | 3.455/4.538/5.012/6.402 |
| baseline-2 | dense | 12.583/13.133/13.429/13.429 | 4.763/5.093/5.517/5.517 |
| v2-play-2 | native | 8.249/10.209/11.991/15.088 | 3.466/4.561/5.047/7.198 |
| v2-play-2 | dense | 12.104/13.092/15.088/15.088 | 4.589/5.017/5.821/5.821 |
| final-2 | native | 5.997/7.636/8.722/14.938 | 3.513/4.587/5.180/7.455 |
| final-2 | dense | 8.643/9.045/9.092/9.092 | 5.100/5.761/6.129/6.129 |
| baseline-3 | native | 8.167/10.218/12.272/13.747 | 3.471/4.495/4.966/6.573 |
| baseline-3 | dense | 12.331/13.065/13.747/13.747 | 4.663/4.954/5.121/5.121 |
| v2-play-3 | native | 8.249/10.246/12.288/14.040 | 3.476/4.562/5.048/5.814 |
| v2-play-3 | dense | 12.441/13.296/13.380/13.380 | 4.627/5.090/5.142/5.142 |
| final-3 | native | 6.068/7.707/8.896/11.060 | 3.531/4.676/5.269/6.666 |
| final-3 | dense | 8.751/9.166/9.321/9.321 | 5.153/5.488/5.699/5.699 |

### Presentation and stage timing

| Run | Swap intervals mean/median/p95/p99/worst ms | Snapshot→swap median/p95/p99/worst ms |
|---|---|---|
| baseline-1 | 16.764 / 16.748 / 16.892 / 18.054 / 20.952 | 33.376 / 33.510 / 35.529 / 43.604 |
| v2-play-1 | 16.750 / 16.744 / 16.846 / 17.219 / 18.860 | 33.373 / 33.479 / 33.860 / 45.119 |
| final-1 | 16.747 / 16.746 / 16.829 / 17.010 / 18.560 | 33.354 / 33.459 / 33.607 / 43.684 |
| baseline-2 | 16.757 / 16.744 / 16.859 / 17.785 / 20.328 | 33.366 / 33.483 / 34.931 / 42.808 |
| v2-play-2 | 16.745 / 16.744 / 16.846 / 17.210 / 19.128 | 33.367 / 33.476 / 33.845 / 44.133 |
| final-2 | 16.759 / 16.758 / 16.852 / 17.169 / 18.636 | 33.390 / 33.488 / 33.799 / 44.079 |
| baseline-3 | 16.755 / 16.747 / 16.857 / 17.498 / 33.719 | 33.377 / 33.491 / 34.456 / 50.511 |
| v2-play-3 | 16.751 / 16.746 / 16.851 / 17.285 / 20.988 | 33.374 / 33.488 / 34.025 / 43.845 |
| final-3 | 16.747 / 16.746 / 16.844 / 17.121 / 18.879 | 33.365 / 33.472 / 33.728 / 44.608 |

| Run | Native interval mean ms | Dense interval mean ms |
|---|---:|---:|
| baseline-1 | 16.763845 | 17.776231 |
| v2-play-1 | 16.749839 | 17.159913 |
| final-1 | 16.746735 | 16.746081 |
| baseline-2 | 16.756554 | 17.538640 |
| v2-play-2 | 16.744629 | 16.929745 |
| final-2 | 16.758552 | 16.759147 |
| baseline-3 | 16.755328 | 17.436036 |
| v2-play-3 | 16.750777 | 17.321436 |
| final-3 | 16.747033 | 16.747385 |

All nine headline runs have zero >50 ms main/prepare/present inter-stage gaps. Baseline-3 has one isolated inferred repeat; final runs have none. Exact ticks/stage times for every repeat remain in each run’s event file. Final dense interval means recover the panel period, while baseline dense means remain 17.44–17.78 ms despite low rounded repeat counts. The full service table includes simulation, preparation wall/thread CPU, compiler, walk, presentation-thread CPU, GL draw, swap and snapshot age for both windows at median/p95/p99/worst. Normal timing does not add a per-frame diagnostic GPU fence; draw and swap wall time must not be relabelled exclusive GPU execution.

All native headline clock samples are 1512000 kHz on all four cores; GPU is 240 MHz, CPU/GPU cooling states 0. Native clock sampling averages approximately 100 ms, with measured scheduling jitter retained. Start temperatures and complete sampled histories remain per run.

## Sampling profiles and remaining CPU work

`perf` was successfully cross-built from the matching 4.9.170 kernel tools. Private downloaded host flex/bison/m4/libelf headers avoided an unavailable sudo install; ARM64 recording uses NO_LIBELF. The recorder exists only in the diagnostic directory. Failed and successful build logs are retained. See [recorder provenance and limitations](native/p2/PERF_BUILD.md).

Profile executables add symbols, frame pointers and no-inlining for attribution only. `perf record -B -F 999 -g` targets rr-prepare. Full-native and dense windows are selected from that run’s preparation timestamps. Actual ELF SHA and every executable library section byte/address are authenticated before resolving raw IPs with cross-nm. Unknown samples remain counted; tail calls, driver/kernel boundaries and no-inline perturbation limit attribution. These runs never contribute performance timings.

| Build | Native samples / unknown self | Dense samples / unknown self | Flame graphs and tables |
|---|---:|---:|---|
| play | 54199 / 550 | 723 / 8 | [native](benchmarks/native-p2-20261016/runs/v2-profile-play-run/flame-native.svg), [dense](benchmarks/native-p2-20261016/runs/v2-profile-play-run/flame-dense.svg), [top functions](benchmarks/native-p2-20261016/runs/v2-profile-play-run/top-functions.csv), [summary](benchmarks/native-p2-20261016/runs/v2-profile-play-run/profile-summary.json) |
| a | 45109 / 573 | 594 / 13 | [native](benchmarks/native-p2-20261016/runs/v2-profile-a-run/flame-native.svg), [dense](benchmarks/native-p2-20261016/runs/v2-profile-a-run/flame-dense.svg), [top functions](benchmarks/native-p2-20261016/runs/v2-profile-a-run/top-functions.csv), [summary](benchmarks/native-p2-20261016/runs/v2-profile-a-run/profile-summary.json) |
| b | 45282 / 563 | 596 / 8 | [native](benchmarks/native-p2-20261016/runs/v2-profile-b-run/flame-native.svg), [dense](benchmarks/native-p2-20261016/runs/v2-profile-b-run/flame-dense.svg), [top functions](benchmarks/native-p2-20261016/runs/v2-profile-b-run/top-functions.csv), [summary](benchmarks/native-p2-20261016/runs/v2-profile-b-run/profile-summary.json) |
| c | 45998 / 570 | 603 / 6 | [native](benchmarks/native-p2-20261016/runs/v2-profile-c-run/flame-native.svg), [dense](benchmarks/native-p2-20261016/runs/v2-profile-c-run/flame-dense.svg), [top functions](benchmarks/native-p2-20261016/runs/v2-profile-c-run/top-functions.csv), [summary](benchmarks/native-p2-20261016/runs/v2-profile-c-run/profile-summary.json) |
| d | 46314 / 590 | 608 / 7 | [native](benchmarks/native-p2-20261016/runs/v2-profile-d-run/flame-native.svg), [dense](benchmarks/native-p2-20261016/runs/v2-profile-d-run/flame-dense.svg), [top functions](benchmarks/native-p2-20261016/runs/v2-profile-d-run/top-functions.csv), [summary](benchmarks/native-p2-20261016/runs/v2-profile-d-run/profile-summary.json) |
| e | 47619 / 540 | 627 / 6 | [native](benchmarks/native-p2-20261016/runs/v2-profile-e-run/flame-native.svg), [dense](benchmarks/native-p2-20261016/runs/v2-profile-e-run/flame-dense.svg), [top functions](benchmarks/native-p2-20261016/runs/v2-profile-e-run/top-functions.csv), [summary](benchmarks/native-p2-20261016/runs/v2-profile-e-run/profile-summary.json) |

Top ten self-time functions for **selected b**, denominator all retained samples in each window:

| Function | Native self share | Dense self share |
|---|---:|---:|
| `live_cull` | 18.41% | 17.95% |
| `quad_fixed` | 14.02% | 12.92% |
| `live_compile` | 9.69% | 8.05% |
| `eng_point_read` | 5.91% | 5.54% |
| `proj_sat` | 5.87% | 6.71% |
| `__divti3` | 5.50% | 5.54% |
| `__memcpy_generic` | 4.47% | 6.21% |
| `memcmp` | 4.23% | Outside dense top ten |
| `emit` | 3.58% | 4.87% |
| `live_mesh` | 3.55% | Outside dense top ten |

The unchanged-play profile attributes 19.1% native self samples to `live_compile`, 7.7% to `memcmp` and 6.4% to recipe comparison. Selected b shifts the largest share to conservative culling (`live_cull`, 18.4%) and original geometry walking (`quad_fixed`, 14.0%; `eng_point_read`, `proj_sat`, `__divti3`). Dense culling remains about 18%, quad conversion about 13%. This supports the original palette/material cost attribution and the measured cache gain, while identifying culling and walking as residual CPU work. Shares are not clock-normalised savings and must not be subtracted as timing deltas. Per-stage top-ten tables, including compiler, walk and other categories, are retained for every build. Sorting’s matched-libc evidence refines the earlier bucket interpretation. No further optimisation is implemented.

## Continuous replay-loop soak

The final b build completed **1,852.85 seconds / 14 complete laps**, without inter-lap cooling or clock resets. Native exposure totals **986.39 seconds (16.44 minutes)**; the rest includes load/boot/legacy and process turnover. Every lap passes all 21 authoritative sets and completes all 6,006 ticks. Native-settled begins after release tick 2112 each lap. The full soak has **3 inferred repeats, 14,406 full-native underrun frames, 2,131 settled-native underrun frames and one >50 ms main gap**. It therefore fails the soak stability gate.

| 5-minute bucket (s) | Native exposure s | Repeats | Underrun frames native/settled | Main gaps >50ms | Peak °C |
|---|---:|---:|---:|---:|---:|
| 0–300 | 140.9 | 0 | 2013 / 0 | 0 | 65.51 |
| 300–600 | 146.3 | 0 | 2649 / 0 | 0 | 66.24 |
| 600–900 | 178.7 | 0 | 1706 / 0 | 0 | 66.56 |
| 900–1200 | 168.2 | 0 | 1679 / 0 | 0 | 67.29 |
| 1200–1500 | 140.9 | 2 | 3410 / 1779 | 1 | 66.97 |
| 1500–1800 | 159.3 | 1 | 2949 / 352 | 0 | 67.78 |
| 1800–1853 | 52.1 | 0 | 0 / 0 | 0 | 67.37 |

Rates are available with native-exposure denominators in [soak-summary.json](benchmarks/native-p2-20261016/runs/soak-1512000/soak-summary.json): repeats/minute by bucket are 0/0/0/0/0.852/0.377/0. Settled underruns occur in laps 11 and 13, rather than as a monotonic rise. That does **not** prove no degradation: the late failures and rising sampled temperature prevent a clean stability claim. The partial final bucket is retained. The user observed settled racing and reported **“Looks and sounds good.”** This supports perceived playability during observation, without overriding the measured failures.

### Clocks, thermal activity and observer

CPU remains 1512 MHz in every sampled native window and all 18,427 full-session clock samples; CPU cooling state remains 0. The device qualifier records 9,868 native samples and zero flagged values, so the authorisation’s conditional 1416 MHz CPU repeat is **not triggered**. GPU remains under the original simple_ondemand policy, but its cooling device becomes active: 707 full-session samples at state 1 and 45 at state 2. Native windows also contain GPU cooling activity. Across the whole session, 18,386 GPU samples are 240 MHz and 41 are higher stock frequencies. One native boundary window has samples at 696/336 MHz. No GPU settings were changed. **No CPU throttling is observed; an unconditional “no throttling” claim is not supported because GPU cooling is active.** Peak sampled temperature is 67.778°C.

The fixed observer follows all 14 distinct game PIDs; 18,372 samples contain game-thread records. It consumes 79.92 CPU seconds across the session (about 4.31% of one core), at SCHED_IDLE/nice 19, with zero dropped records/errors. Actual sample interval median/p95/p99/worst is 100.011/105.719/124.025/3527.726 ms. The long observer gaps have **zero overlap with measured native game gaps**; the slowest individual kernel read is 27.810 ms. Thus low-impact does not mean perfect 10 Hz coverage or zero overhead. Headline process observation stays OFF. Raw read bounds and coverage gaps are retained.

### Residual stalls and memory/reload evidence

- **Lap 11, tick 3966:** main publication gap 64.068 ms; preparation-start gap 63.509 ms; presentation-start gap 55.344 ms. Recorded simulation service is 62.793 ms with zero queue-blocked time; preparation is 7.495 ms and draw 4.445 ms. This underfeeds presentation, producing a 43.511 ms swap interval (2 inferred repeats) and 1,779 missing audio frames. The actual main PID 2644 is captured, with main runnable and workers waiting on futex in the preceding sample. However, the ~100 ms bracketing samples fall outside the 64 ms gap. Main is R in both, major-fault count unchanged, sampled global reclaim counters unchanged, process block reads increase only 8 KiB and block writes 0. The main consumes four CPU-accounting jiffies over that ~99.5 ms bracket. **Exact CPU/scheduling cause is unknown**; these stacks do not prove D-state reclaim or a particular syscall. No speculative fix is made.
- **Lap 13, tick 3515:** interval 33.453 ms, one inferred repeat and 352 settled underrun frames. Simulation 13.908 ms, preparation 13.861 ms, draw 15.704 ms and swap 17.190 ms identify a presentation-side late event. No >50 ms main gap occurs; precise GPU/driver/scheduling attribution is not isolated.

See [process-targeted findings](benchmarks/native-p2-20261016/runs/soak-1512000/stall-findings.json), [captured gap observations](benchmarks/native-p2-20261016/runs/soak-1512000/gap-attribution.json) and per-lap repeat event files. This does not reproduce P1’s multi-second D-state native stall. The separate interrupted startup D snapshot remains an unknown pre-native event.

Observed per-lap peak RSS is 772,480–775,468 KiB; global minimum MemAvailable is 112,136 KiB and CmaFree 1,724 KiB. Peak Mali accounting is 152,874 pages (~597.16 MiB), including driver/transient legacy resources; it is not the static native asset total. RSS and GPU page accounting overlap and must not be summed. All laps reload about 509–531 MB of process block-read data, with per-lap global reclaim/allocstall increments. Peak RSS and minimum available memory remain broadly stable; repeated pack reloads create substantial reclaim exposure. See [per-lap counters](benchmarks/native-p2-20261016/tables/soak-1512000-per-lap.csv). Those global increments are **not coincident causal evidence** for the captured native main gap, whose bracketing reclaim counters do not change. A safe in-process replay reset remains unavailable; no reset rewrite or texture-format work is started.

## Retained failures and measurement limitations

Initial partial-play-split runs removed packet SHA and GL error polling but still contained renderer consistency scans. They remain labelled preliminary (`play-1`, initial `timing-a/b` and profiles) and do not substitute for corrected headline populations. All corrected v2 source identities are retained.

`preflight-hot` was stopped while cooling, before any replay or clock change; later orchestration stops EmulationStation before cooling. `interrupted-startup-play` ended at tick 386 after the user reported a retained race image and unresponsive START+SELECT. A first process snapshot showed main in D, but it had resumed to R before its kernel stack was captured. The exact transient wait is **unknown**. Its operator-annotated wrapper status 143 is not a natural successful replay exit; no native pictures were measured. The contemporaneous Wi-Fi/kernel trace lies outside completed native timing windows and does not prove causality. Raw kernel memory/register dumps are retained privately with hashes, rather than published.

The first headline supervisor attempt hit exFAT's unsupported symlink operation before launching a game or changing clocks. It was replaced by a text alias manifest. All failed/preliminary logs remain evidence; no native timing outlier is removed. The host-only sorting combination without c/d passed parity but was not transferred or adopted after e regressed, avoiding an unsupported extra optimisation search.

The soak uses complete replay processes, not a fabricated in-process reset: the original lifted reset entry does not return, replay completion exits, and no safe original-state reset lifecycle exists. Reload, boot/legacy activity and reclaim are consequently included in the continuous wall-time exposure and reported per lap. Native exposure is reported separately. No simulation reset rewrite was introduced.

The observer is SCHED_IDLE/nice 19, has no pressure-file reads, buffers output and timestamps each kernel read. Process-targeted stacks, I/O and global reclaim counters are retained. Global `pgscan*`/`pgsteal*`/`allocstall*` deltas alone cannot establish a game's stall cause. Sampling can miss a short state change; absent captured causality remains unknown. No speculative reclaim fix is included.

## Evidence and reproducibility

[Evidence README](benchmarks/native-p2-20261016/README.md) indexes all methods and retained populations. [Per-run comparisons](benchmarks/native-p2-20261016/run-comparison.csv), [all stage service distributions](benchmarks/native-p2-20261016/service-timings.csv), [headline gate evaluation](benchmarks/native-p2-20261016/headline-summary.json), [tick-matched differences](benchmarks/native-p2-20261016/paired/) and losslessly compressed raw CSV/JSON/kernel telemetry accompany this report. Every inferred repeat has a tick and coincident stage times in its run's `repeat-events.json`; complete intervals remain in `thread.csv.gz`. Raw perf data/script and unsanitised kernel dumps stay in ignored `native/generated/p2/hardware`, indexed by [private evidence hashes](benchmarks/native-p2-20261016/private-profile-manifest.json). Public SVGs contain sampled function names/shares rather than ROM asset bytes. No commits are created.

## Exit gate and final device/repository state

| Requirement | Result |
|---|---|
| Dense preparation p99 ≤14 ms in all final headline runs | **FAIL:** 14.310 / 14.739 / 14.811 ms |
| Three final runs: settled-native underruns 0 | PASS: 0 / 0 / 0 |
| Three final runs: main gaps >50 ms =0 | PASS: 0 / 0 / 0 |
| Three final runs: at most one isolated single inferred repeat | PASS: 0 / 0 / 0 |
| Soak: no main gaps >50 ms | **FAIL:** one 64.068 ms gap |
| Soak: no throttling, no degradation | Not demonstrated: CPU unflagged, GPU cooling active; late repeat/audio failures |
| Validation: all 21 sets, 4,207 packets and 36 images | PASS for every corrected cumulative validation build |

The investigation and required measurements are complete; **P2 performance gate FAIL**. Preparation margin remains limited by conservative culling and original geometry walking, with rare main-service and presentation outliers still visible in the longer soak. The cache gain makes settled reference-race play materially better, but does not prove sustained failure-free pacing. Retain a+b as the isolated candidate; reject c/d/e. Any further work should first obtain a targeted attribution of the residual main-service event and evaluate remaining culling/walk costs under separate authorisation. Memory/texture-format investigations may be proposed for the substantial reload/reclaim footprint, but this run does not prove that reclaim caused the native gap. No further optimisation, broader coverage or productisation has begun.

All **46 completed full replays**, including preliminary, attribution and 14 soak laps, independently pass all 21 authoritative sets; the incomplete startup attempt remains separately retained. [Full replay audit](benchmarks/native-p2-20261016/full-replay-audit.json) records packet/image receipt coverage explicitly rather than implying play builds hash packets. All 35 independent device sessions have equal saved/restored clock policies and production hashes, with EmulationStation receipts; the final-1 alias is counted once. Final all-47 protected-file verification passes. Production SHA256 remains `6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`. All four CPUs are restored to schedutil, min 480000 / max 1512000 kHz; GPU simple_ondemand with original min/max 0. EmulationStation PID 3285 is running; no diagnostic game, sampler or perf process remains. [Restoration summary](benchmarks/native-p2-20261016/restoration-summary.json) and [final device evidence](benchmarks/native-p2-20261016/runs/final-check/device-final.txt) retain the checks. Production executable/launcher, physics/audio/gameplay, saves, replays and controller settings are unchanged. No commit, push or production deployment. Historical P1/P1b/M3 gates are unchanged.

**STOPPED. Texture format, broader coverage, productisation and further optimisation require separate user authorisation.**
