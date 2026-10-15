# Stage 1b — Environment and build-flag headroom audit

**COMPLETE / STOPPED — bounded measurement audit; no Stage 2 or Milestone 4B implementation.**

Branch `feature/knulli-h700`, starting HEAD `cd4dc046dfd4ff187a04f0fb751c35c8fca76215`. Investigation only: no native compiler/shader/architecture/gameplay/audio-logic changes, production deployment, commit or push. All reported B runs complete 6,006 ticks and pass 21 authoritative checkpoint sets. M3 remains performance-blocked.


**Recommendation:** use generated-source `-O2` in a separately authorised prototype, retain stack protection, leave affinity off, and expose CPU-performance/GPU-max settings as explicit experimental switches. Both clocks plus O2 save 1.46 ms (13.9%) of median simulation time versus the matched stock O1 run and 2.94 ms (28.7%) of median world-plus-HUD completion. This does **not** solve the p99 preparation tail: the unchanged three-stage model still has 70 late pictures at 10% contention and 298 at 20%. The measured panel is 59.7104 Hz, below arcade 59.9040599 Hz. No settings measured here establish consistent 59.904 FPS or a no-drop, vsynchronised presentation path at the current display timing.

The prior Stage 1 recommendation remains a **conditional GO for a limited throughput experiment**, not production or performance acceptance. Resolve the presentation-policy question explicitly before claiming a path to the ultimate objective. No parallel prototype, compiler algorithm optimisation, display-timing change or production change is implemented here. M3 remains performance-blocked.

## Clock audit and governor sensitivity

One persistent 20 Hz sampler records GPU current frequency/governor/raw transition tables and numeric deltas, per-core CPU frequencies, thermal zones and task/memory observations. Worst native-phase spacing among the 24 B runs is 62.472 ms (about 16 Hz even at the largest gap), meeting the requested 10 Hz minimum. Every sample is retained.

Across the three stock runs, native GPU reports 240 MHz for all 9218 native-phase samples. Production-threaded GPU is 240 MHz for 79.4306% of 4215 samples, with 16.0142% at 336 MHz and occasional 420–696 MHz readings. Thus **serial native stays at 240 MHz; production-threaded does not**. CPU maximum is 1512 MHz and cores 0–3 share one frequency policy. In stock native runs, CPU0 reports 1512 MHz for 91.2779% of samples,1416 MHz for 5.7279%, and 480 MHz for 1.6815%; production reports 1512 MHz for 93.7367% and 1416 MHz for 4.0807%. All per-core histograms remain in the per-run JSON; [stock CPU distribution](benchmarks/native-stage1b-20261015/A-stock-cpu-distribution.json) records counts. Raw GPU transition tables and deltas are retained in driver units; they are not inferred GPU utilisation.

The GPU exposes only `simple_ondemand`; GPU-performance diagnostics therefore use the authorised min=max 696 MHz clamp, restoring original min=max 0 and governor after every run. CPU-performance uses the existing performance governor; no overclock or thermal-policy change.

Summary below: median of three per-run medians/p95/p99; worst is maximum over all runs. Each per-run percentile uses nearest rank; no outlier removal. Complete per-run distributions are in [all-run-metrics.csv](benchmarks/native-stage1b-20261015/all-run-metrics.csv).

| Metric / governor | Median ms | p95 ms | p99 ms | Worst ms |
|---|---:|---:|---:|---:|
| Simulation wall / stock | 10.539 | 13.783 | 15.509 | 23.924 |
| Simulation wall / gpu | 10.463 | 13.614 | 15.409 | 20.148 |
| Simulation wall / cpu | 9.949 | 11.870 | 12.435 | 19.079 |
| Simulation wall / both | 9.910 | 11.847 | 12.405 | 19.012 |
| Native walk wall / stock | 3.354 | 4.326 | 4.906 | 8.260 |
| Native walk wall / gpu | 3.330 | 4.329 | 5.127 | 8.240 |
| Native walk wall / cpu | 3.327 | 4.265 | 4.703 | 5.159 |
| Native walk wall / both | 3.325 | 4.240 | 4.709 | 5.236 |
| Compiler wall / stock | 7.343 | 9.753 | 11.086 | 13.759 |
| Compiler wall / gpu | 7.284 | 9.869 | 11.212 | 15.475 |
| Compiler wall / cpu | 7.253 | 9.522 | 10.822 | 12.044 |
| Compiler wall / both | 7.249 | 9.534 | 10.906 | 12.607 |
| World + HUD completion / stock | 10.263 | 12.127 | 13.530 | 19.378 |
| World + HUD completion / gpu | 7.307 | 8.989 | 9.995 | 14.721 |
| World + HUD completion / cpu | 10.238 | 12.071 | 13.404 | 20.802 |
| World + HUD completion / both | 7.276 | 8.942 | 9.772 | 11.445 |
| Final fence wait / stock | 7.586 | 8.819 | 9.311 | 15.528 |
| Final fence wait / gpu | 4.611 | 5.666 | 5.940 | 7.193 |
| Final fence wait / cpu | 7.558 | 8.792 | 9.293 | 16.826 |
| Final fence wait / both | 4.596 | 5.644 | 5.926 | 7.180 |
| Presentation interval / stock | 36.244 | 44.048 | 48.390 | 57.378 |
| Presentation interval / gpu | 33.176 | 40.332 | 44.366 | 52.056 |
| Presentation interval / cpu | 35.694 | 42.708 | 46.934 | 63.381 |
| Presentation interval / both | 32.660 | 39.685 | 43.416 | 50.973 |

The serial diagnostic includes the unchanged Stage 1 sequential original-GLES shadow comparator. Its extra work is excluded only from the native-product service model, as in Stage 1; actual presentation/audio results retain its cost. The original model code is unchanged.

| Governor | Modeled late pictures,0% contention (three runs) |10% |20% |
|---|---|---|---|
| stock | 0/0/7 | 153/143/200 | 387/404/406 |
| gpu | 0/0/0 | 160/138/153 | 392/386/377 |
| cpu | 0/0/0 | 96/75/94 | 341/339/348 |
| both | 0/0/0 | 95/74/78 | 346/335/331 |

Combined clocks provide about 0.9 ms modeled p99 service headroom, but still 74–95 late pictures at 10% contention. The original 12 ms median/14 ms p95 renderer targets and 16.693 ms deadline remain unchanged. These are software-target model results, not proof of real display pacing.

Peak recorded temperature across all game runs is 68.588 °C. CPU and GPU coolers bind to the 70 °C target, but the active `power_allocator` uses the 60 °C first passive trip as its switch-on point (sustainable power 1000 mW). Therefore below 70 °C does not establish absence of thermal control. Later stock-GPU production runs observe GPU cooling states 1/2, meaning thermal OPP limiting is active; CPU cooling remains 0. Later fixed-clock native runs observe both cooling states 0, CPU1512 MHz and GPU696 MHz throughout the measured windows. B did not record cooling states, so no stronger thermal-causality claim is made for B. Raw thermal bindings/policy and later cooling-state evidence are retained.

One GPU-only production run contains a 2.740-second stall at tick 804, before the 1800–6006 analysis window. Main is in uninterruptible D state for 54/55 samples with essentially zero CPU progress; exact wait cause is unproven. This event remains in full-replay rates/audio counters and raw data. It is not removed as an outlier.

The unchanged B production profiler does not export raw simulation records or p99; later separately labelled opt-in shutdown-logging copies supply this missing distribution. Original GLES draw/submission wall is not a separately fenced GPU completion measurement; no extra fences were added. Full-replay audio/resync totals are explicitly distinguished from native-phase per-picture timings.


### Unchanged production-threaded governor comparison

Original preparation, draw/submission and presentation intervals below use the same three-run summary convention. Draw/submission is **not independently fenced GPU completion**; native walk/compiler and final GPU fence are unavailable/inapplicable in this unchanged executable.

| Governor / metric | Median ms | p95 ms | p99 ms | Worst ms |
|---|---:|---:|---:|---:|
| stock / preparation | 3.826 | 5.066 | 6.012 | 9.495 |
| stock / draw/submission | 14.190 | 21.413 | 27.790 | 81.302 |
| stock / presentation interval | 16.792 | 26.579 | 31.491 | 83.417 |
| gpu / preparation | 3.909 | 5.597 | 6.705 | 17.670 |
| gpu / draw/submission | 14.533 | 21.780 | 27.670 | 79.455 |
| gpu / presentation interval | 16.834 | 23.629 | 29.629 | 81.184 |
| cpu / preparation | 3.786 | 5.094 | 5.838 | 14.092 |
| cpu / draw/submission | 14.125 | 21.401 | 27.319 | 78.543 |
| cpu / presentation interval | 16.786 | 26.822 | 31.413 | 80.084 |
| both / preparation | 3.869 | 5.568 | 6.705 | 9.573 |
| both / draw/submission | 14.449 | 21.951 | 27.878 | 77.689 |
| both / presentation interval | 16.820 | 23.740 | 29.882 | 79.322 |

Native GPU clock gains do not translate into equivalent original GLES submission gains: median production draw/submission remains 14.1–14.5 ms. Production retains independent presentation/coalescing; its interval population is actual presented pictures, not every simulation tick. Full-replay drops, resyncs and audio underrun counts are in all-run-counters.csv.


### Production simulation distributions and the logging limitation

| Unchanged B governor | Median ms | p95 ms | p99 | Worst ms |
|---|---:|---:|---|---:|
| stock | 11.920 | 14.073 | Not exported | 17.481 |
| gpu | 12.099 | 14.510 | Not exported | 19.313 |
| cpu | 11.832 | 13.831 | Not exported | 17.045 |
| both | 12.063 | 14.434 | Not exported | 17.182 |

The following separately labelled default-OFF shutdown-CSV copies supply complete distributions with unchanged simulation hot-path opcodes. They are not retroactively substituted into the unchanged B runs.

| Logging run | Median ms | p95 ms | p99 ms | Worst ms |
|---|---:|---:|---:|---:|
| D-1-stock-production-off | 11.770 | 13.950 | 14.950 | 16.950 |
| D-2-stock-production-off | 11.911 | 14.020 | 15.018 | 18.863 |
| B-supplement-gpu-production | 12.142 | 14.459 | 15.523 | 18.859 |
| B-supplement-cpu-production | 11.809 | 13.947 | 15.146 | 16.485 |
| D-1-both-production-off | 12.202 | 14.433 | 15.268 | 17.723 |
| D-2-both-production-off | 12.185 | 14.460 | 15.376 | 16.780 |

## Panel measurement — completed, physical constraint identified

Same 640×480 fullscreen Mali SDL/EGL surface, RGB8/depth 16 requested(actual 24), swap interval 1 reports success; all 3001 consecutive swap returns and 3000 intervals retained. CLOCK_MONOTONIC_RAW timestamps avoid NTP slew.

- Return interval mean **16.7425 ms /59.7281 Hz**, median **16.7480 ms /59.7085 Hz**, standard deviation **0.3633 ms**. Startup queue-fill returns remain in these statistics.
- Swap-call blocking median **16.6701 ms**; interval 1 is honoured once buffers fill. No interval exceeds 1.5× the median. Timestamps do not prove absence of physical tearing.
- A linear fit using **all 3001 returns** gives **16.7475 ms /59.7104 Hz**. This matches the nominal 24 MHz timing: Htotal 770, Vtotal 522,59.7104045 Hz. fb0 reports 640×480 virtual 640×960, bpp 32, mode `U:640x480p-59`, pixclock 41666ps (rounded). SDL nominal 60 and the coarse driver fps counter are not the measured rate.
- FBIO_WAITFORVSYNC reports success but returns in ~8microseconds. This is a no-op, **not** an independent 121kHz refresh measurement; raw calls are retained and rejected as refresh evidence.

**The panel is slower than the arcade 59.9040599 Hz.** Vsync pacing would require about one omitted presentation every 5.16seconds while preserving every authoritative simulation tick. A bounded queue cannot absorb that permanent drift indefinitely. Unsynchronised presentation is another policy option with tearing risk; it does not increase physical scanout rate. Display-timing investigation would require separate authorisation. No option is selected or implemented, and the all-picture 59.904 objective is not relaxed.

The unchanged Stage 1 pipeline model assumes 16.693359375 ms presentation targets; it remains useful for comparing CPU/GPU service costs, but cannot certify no-drop scanout on this measured panel mode. This limitation must remain explicit in any future prototype decision.

A second, physically observed probe retains another 3001 returns: interval mean 16.7415 ms/59.7318 Hz, median 16.7444 ms/59.7215 Hz, standard deviation 0.3262 ms; fitted all-return cadence 59.7106 Hz. The observer reports “a slightly flickering purple screen with intermittant red and blue flashes.” Rapid alternation can blend perceptually, but this is not a tearing-free visual pass. Both probes exit 0, restore EmulationStation and verify unchanged clocks/production hashes.

## Affinity — completed

Sixteen full replays form two counterbalanced off/pinned pairs for each mode and governor (stock/both), followed by two separate production logging supplements for GPU-only and CPU-only governors. All 18 runs exit 0, reach 6006 and pass all 21 authoritative checkpoint sets; every run verifies original clocks, production hashes and EmulationStation restoration.

The default-OFF preload pins only explicitly requested main/rr-prepare/rr-present roles. New children first recover their creator's original unrestricted mask, so audio and Mali helpers do not inherit a pin. Standalone syscall tracing proves unset produces **zero sched_setaffinity calls**. Enabled thread masks and per-thread sampled cores/context switches are retained in [D-thread-residency-and-context-switches.csv](benchmarks/native-stage1b-20261015/D-thread-residency-and-context-switches.csv). Core observations are sampled last-CPU counts, not exact time residency or a scheduler trace.

| Governor / mode / pin | Simulation median, two runs (ms) | Simulation p99, two runs (ms) |
|---|---|---|
| stock / native / off |10.498 /10.594|14.955 /15.576|
| stock / native / pinned |10.377 /10.379|13.318 /13.099|
| stock / production / off |11.770 /11.911|14.950 /15.018|
| stock / production / pinned |12.136 /12.059|15.140 /15.164|
| both / native / off |9.958 /9.954|12.383 /12.415|
| both / native / pinned |10.068 /10.072|12.573 /12.616|
| both / production / off |12.202 /12.185|15.268 /15.376|
| both / production / pinned |12.364 /12.417|15.966 /15.724|

All requested stage percentiles, worst cases, presentation distributions and counters remain in the per-run JSON and all-run CSV rather than being pooled across executables. With both performance clocks, native modeled late counts at 0/10/20% contention are **0/103/346 and 0/83/341 unpinned**, versus **0/149/369 and 0/124/366 pinned**. Stock native tail improvement is real in these pairs, but it does not transfer to the best governor configuration or production threading. **Recommend affinity OFF by default**, retaining the switch for diagnostics. Best-combination flag tests therefore use both performance clocks and affinity off.

The persistent observer consumes about 0.08 of one CPU core in the first native comparison pair. That overhead is included in all results, never subtracted; these are instrumented measurements, not zero-overhead estimates.

## Generated-source flags — completed under revised one-run scope

Only `gen/rr_lifted.c` and `gen/snd_driver.c` are rebuilt. All other linked objects are reused and hashed. The rebuilt O1 .text sections match the original generated objects exactly. Host O1/O2/O2-no-stack-protector each completes 6,006, all 21 checkpoint sets and **4207 exact packets /267,308,900 bytes** identical to the established native reference.

The ARM wrapper enables `__SSP_STRONG__` for O1/O2, and the no-stack-protector variant correctly removes it. Neither generated object references stack-check helpers under the default flags, so O2 and O2-no-stack-protector stripped executables are byte-identical. There is no stack-protector cost to remove from these generated functions.

ARM current/O1/O2/O2-no-stack-protector each completes 6,006 with exit 0 and all 21 authoritative sets identical to the recording. All 4207 packet lengths andSHA256 receipts are identical to the host reference and each other, including all 267,308,900 bytes. Validation-only hash-enabled timings are inadmissible performance evidence. The user subsequently reduced timing to **one full replay per configuration**, explicitly skipping repeat rounds 2/3. The four-run timing matrix uses hashing OFF: stock O1, best-clock O1, best-clock O2 and best-clock O2-no-stack-protector. Flag repeatability is not established; all earlier B/D repeats remain retained. No three-stage prototype or architectural implementation has begun.

Optional PGO is omitted from this bounded audit: it requires a separately validated training replay different from the measured reference. Optional perf cross-build from the matching 4.9.170 source stops at missing flex; target libelf headers are also absent, and Buildroot perf is not enabled. The failure log is retained; no system packages/configuration or device profiler installation changed. No attribution timings or flame graphs are claimed.


### Build size and build time

Only two generated sources compile, with compiler cache disabled; other objects are reused. Wall times are actual local builds with concurrent host/ARM compilation and validation, not isolated build-speed benchmarks. Full commands and linked-object hashes are in E-build-*.json.

| Variant | ARM rebuild s | ARM stripped bytes | ARM unstripped bytes | Host rebuild s | Host bytes |
|---|---:|---:|---:|---:|---:|
| O1 | 90.201 | 7,323,824 | 53,092,728 | 65.743 | 52,091,520 |
| O2 | 174.304 | 7,086,256 | 49,401,472 | 106.826 | 58,901,280 |
| O2-nosp | 177.394 | 7,086,256 | 49,401,664 | 110.370 | 58,901,512 |

### Matched generated-flag results

One full replay per configuration, as amended by the user. Each distribution has 4207 native-phase samples, nearest-rank p95/p99, with all samples retained. Hashing is OFF. These are single-run comparisons; the identical O2/no-stack-protector binaries provide an additional cross-check but do not replace a repeatability study.

| Configuration / metric | Median ms | p95 ms | p99 ms | Worst ms | >16.693 ms |
|---|---:|---:|---:|---:|---:|
| stock O1 / simulation wall | 10.538 | 13.667 | 15.254 | 18.911 | 16 |
| stock O1 / simulation CPU | 10.206 | 12.404 | 14.965 | 18.908 | 12 |
| stock O1 / native walk | 3.340 | 4.328 | 4.927 | 7.861 | 0 |
| stock O1 / compiler | 7.266 | 9.879 | 11.139 | 14.515 | 0 |
| stock O1 / world+HUD completion | 10.247 | 12.144 | 13.550 | 16.986 | 1 |
| stock O1 / final fence wait | 7.601 | 8.807 | 9.355 | 11.498 | 0 |
| stock O1 / presentation interval | 36.108 | 43.794 | 48.214 | 53.019 | 4207 |
| both O1 / simulation wall | 9.939 | 11.841 | 12.345 | 18.948 | 2 |
| both O1 / simulation CPU | 9.836 | 11.731 | 12.269 | 13.527 | 0 |
| both O1 / native walk | 3.310 | 4.257 | 4.697 | 5.044 | 0 |
| both O1 / compiler | 7.249 | 9.505 | 10.852 | 11.867 | 0 |
| both O1 / world+HUD completion | 7.307 | 8.938 | 9.885 | 10.967 | 0 |
| both O1 / final fence wait | 4.609 | 5.672 | 5.961 | 6.363 | 0 |
| both O1 / presentation interval | 32.649 | 39.682 | 43.452 | 49.919 | 4207 |
| both O2 / simulation wall | 9.075 | 11.014 | 11.602 | 17.264 | 1 |
| both O2 / simulation CPU | 8.986 | 10.923 | 11.466 | 12.622 | 0 |
| both O2 / native walk | 3.315 | 4.208 | 4.685 | 5.240 | 0 |
| both O2 / compiler | 7.220 | 9.487 | 10.847 | 11.748 | 0 |
| both O2 / world+HUD completion | 7.308 | 8.924 | 9.824 | 10.692 | 0 |
| both O2 / final fence wait | 4.624 | 5.689 | 5.964 | 6.555 | 0 |
| both O2 / presentation interval | 31.884 | 38.821 | 42.654 | 48.995 | 4207 |
| both O2-nosp / simulation wall | 9.079 | 11.029 | 11.642 | 17.103 | 1 |
| both O2-nosp / simulation CPU | 9.006 | 10.956 | 11.495 | 12.680 | 0 |
| both O2-nosp / native walk | 3.295 | 4.230 | 4.674 | 5.198 | 0 |
| both O2-nosp / compiler | 7.268 | 9.557 | 10.835 | 11.777 | 0 |
| both O2-nosp / world+HUD completion | 7.304 | 8.965 | 9.849 | 10.632 | 0 |
| both O2-nosp / final fence wait | 4.616 | 5.678 | 5.941 | 6.515 | 0 |
| both O2-nosp / presentation interval | 31.950 | 38.704 | 42.805 | 47.225 | 4207 |

| Configuration / simulation component (inclusive wall) | Median ms | p95 ms | p99 ms | Worst ms |
|---|---:|---:|---:|---:|
| stock O1 / 68K | 4.268 | 5.355 | 7.285 | 8.112 |
| stock O1 / sound driver | 1.716 | 2.755 | 3.884 | 9.467 |
| stock O1 / C352 | 1.056 | 1.750 | 2.245 | 4.861 |
| stock O1 / DSP | 2.841 | 4.491 | 6.217 | 9.171 |
| both O1 / 68K | 4.214 | 4.441 | 4.524 | 4.808 |
| both O1 / sound driver | 1.622 | 2.569 | 2.914 | 9.611 |
| both O1 / C352 | 0.971 | 1.666 | 1.772 | 1.950 |
| both O1 / DSP | 2.789 | 3.539 | 3.739 | 4.050 |
| both O2 / 68K | 3.275 | 3.487 | 3.548 | 10.013 |
| both O2 / sound driver | 1.758 | 2.783 | 3.152 | 4.041 |
| both O2 / C352 | 0.956 | 1.658 | 1.758 | 2.019 |
| both O2 / DSP | 2.755 | 3.505 | 3.666 | 3.922 |
| both O2-nosp / 68K | 3.271 | 3.485 | 3.557 | 9.758 |
| both O2-nosp / sound driver | 1.761 | 2.784 | 3.140 | 4.322 |
| both O2-nosp / C352 | 0.961 | 1.662 | 1.773 | 2.385 |
| both O2-nosp / DSP | 2.780 | 3.526 | 3.698 | 4.003 |

O2 versus matched both-clock O1 saves 0.864 ms (8.7%) median simulation wall and 0.850 ms median simulation CPU. Lifted 68K median falls 4.214→3.275 ms (22.3%); sound-driver median rises 1.622→1.758 ms (8.4%). C352 is effectively unchanged. No mixed-source flag experiment or source optimisation is undertaken. Compiler/walk and renderer costs change little, as expected from reusing those objects.

### Model and counters for the best combination

| Configuration | Late pictures at 0% |10% |20% contention | p99 service headroom at 0% (ms) |
|---|---:|---:|---:|---:|
| stock O1 | 1 | 192 | 399 | 0.201 |
| both O1 | 0 | 70 | 332 | 1.003 |
| both O2 | 0 | 70 | 298 | 1.036 |
| both O2-nosp | 0 | 75 | 304 | 0.978 |

| Configuration | Full-replay audio underrun samples | Clock resyncs | Peak temperature °C |
|---|---:|---:|---:|
| stock O1 | 4,545,307 | 0 | 57.248 |
| both O1 | 3,787,435 | 0 | 57.653 |
| both O2 | 3,604,775 | 0 | 58.220 |
| both O2-nosp | 3,607,267 | 0 | 58.544 |

The serial diagnostic still executes the original-GLES shadow comparison. Its actual presentation medians 31.9–36.1 ms and millions of audio underrun samples are retained; it is not playable 59.904 FPS. Native-phase underrun deltas, all B/D/E counters, production resyncs/drops and every missed deadline are in the linked per-run JSON/CSVs. No diagnostic work is removed from actual presentation intervals. Only the pre-existing Stage 1 service model excludes sequential shadow work when modelling a hypothetical native product.

O2 reduces simulation demand, but preparation remains 10.5–10.7 ms median with a heavy tail. CPU-only model p99 headroom improves mainly through fixed CPU clocks: about 0.20 ms stock versus 1.00 ms both-clock O1 and 1.04 ms both-clock O2 in this matched set. It does not survive 10% contention without misses. GPU clock headroom is substantial but cannot remove CPU preparation or physical-panel constraints. The model's assumed 0.1 ms handoffs and slowdown penalties remain assumptions, not measured concurrency.

## Recommended settings and remaining limitations

| Setting | Recommendation for a separately authorised prototype | Evidence / limit |
|---|---|---|
| Generated-source O2 | Adopt for the two generated sources; retain baseline switch | Full host/ARM determinism and packet parity; ~0.86 ms median simulation saving in one matched run. Broader interactive coverage and repeatability remain unproven. |
| Stack protection | Retain current protection | Removing it changes no stripped executable bytes and buys no code saving. |
| GPU maximum clock | Explicit diagnostic/prototype switch, with restoration; evaluate sustained power before a default | ~2.96 ms median GPU-completion saving in repeated B comparisons; thermal control is active in some stock production workloads. No thermal policy changed. |
| CPU performance governor | Explicit diagnostic/prototype switch, with restoration | ~0.59 ms median simulation saving and improved tails in repeated B comparisons; power/thermal tradeoff remains. |
| Affinity | OFF by default; retain opt-in mapping | Stock serial tail improvement does not carry to both-clock native or production threading. Audio/driver helpers remain unrestricted. |
| Logging | Buffered shutdown records for diagnostics; no validation hash/dump in performance/product runs | Existing hot-path machine code preserved; observer cost ~0.08 core is included, not subtracted. |
| Presentation | Decision required, no implementation | Measured panel slower than arcade. Preserving ticks with explicit presentation omission, accepting unsynchronised tearing risk, or a separately authorised display-timing audit are policy options. None satisfies all current requirements automatically. |

No setting is deployed as a production default. No claim is made that one replay establishes all game/interactive states, long-duration thermal stability, real parallel contention, input latency or audible quality. P99/worst tails, all misses, startup events, scheduler stalls and raw samples are retained. The 12 ms median/14 ms p95 renderer and 16.693 ms frame targets are unchanged; these diagnostic measurements do not retrospectively pass M3.

## Exit evidence and stop

- 50 isolated H700 game replays: all 6,006 ticks, all 21 authoritative CPU/RAM, board/video and PCM/voice checkpoint sets identical on every run; all exits 0. This comprises 24 governor,16 affinity,2 logging-supplement,4 hash-validation and 4 flag-performance runs. The user cancelled E repeat rounds 2/3; none were run or fabricated.
- Host O1/O2/O2-nosp and ARM current/O1/O2/O2-nosp: all 4,207 exact command packets /267,308,900 bytes identical to the established reference, preserving the original 3840-picture M3 window. The first host harness attempt's shutdown failure is retained separately and corrected by a full rerun; it is not counted as a pass.
- All 50 game runs verify per-run governor restoration, unchanged production hashes and EmulationStation readiness. Both panel probes verify unchanged clocks/hash and return to EmulationStation. Final 47 protected file hashes pass, including executable, launcher, controller settings, replay/EEPROM/sidecars and original ROM inputs.
- Final CPU governors: schedutil on 0–3,480–1512 MHz bounds. GPU: simple_ondemand, min/max 0/0, current 240 MHz. EmulationStation is running; no game, sampler or panel probe remains. Original control and production-copy hashes also match.
- Production executable SHA256: `6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`. Branch/HEAD unchanged. Existing game/compiler/shader/threading sources unchanged; only new diagnostic tools/evidence/report and HANDOVER documentation are added. No commit, push or production deployment.
- Raw clock logs are compressed losslessly after all hardware runs; decompressed hashes and byte counts are recorded in clock-raw-sha 256.json. Every sample and failed/development attempt is retained. All 51 summary entries distinguish the 50 game runs from standalone helper diagnostics; validation timings are labelled inadmissible for performance.

See [per-run validation](benchmarks/native-stage1b-20261015/all-run-validation.json), [all distributions](benchmarks/native-stage1b-20261015/all-run-metrics.csv), [audio/rate counters](benchmarks/native-stage1b-20261015/all-run-counters.csv), [unchanged-model comparison](benchmarks/native-stage1b-20261015/pipeline-model-comparison.csv), and [final device integrity](benchmarks/native-stage1b-20261015/device-meta/final-integrity.txt).

**STOPPED. Stage 2, Milestone 4B, architectural implementation and any production change require explicit user authorisation.**


[Retained evidence and methods](benchmarks/native-stage1b-20261015/README.md).
