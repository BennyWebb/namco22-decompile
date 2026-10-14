# Five matched first-lap A/B pairs

Ticks 1800–5639; production GPU HUD/audio/multicore in both variants. Median of five complete runs per variant. No glFinish/reference draws in this benchmark.

| Metric | Production | Arrays |
|---|---:|---:|
| Rendered FPS | 52.931 | 56.019 |
| 1% low FPS | 25.151 | 21.699 |
| p99 interval ms | 33.003 | 33.566 |
| Worst interval ms (median run worst) | 91.893 | 106.627 |
| Image age median ms | 37.389 | 33.108 |
| Image age p95 ms | 54.535 | 50.715 |
| Simulation Hz | 59.904 | 59.905 |
| Preparation CPU ms/picture | 3.775 | 3.851 |
| GLES owner CPU ms/picture | 16.986 | 15.379 |
| Submission wall ms/picture | 9.617 | 7.374 |
| Texture decode wall ms/picture | 0.449 | 0.435 |
| Texture upload/driver wall ms/picture | 0.472 | 0.865 |
| Draw calls/picture | 860.710 | 438.974 |
| Texture bindings/picture | 863.969 | 443.384 |
| Dropped snapshots | 445.000 | 245.000 |
| Process peak RSS MiB | 288.242 | 332.418 |
| Audio underrun samples (whole process) | 0.000 | 0.000 |
| Resyncs | 0.000 | 0.000 |

## Individual pairs

| Pair | Production FPS | Arrays FPS | Production 1% low | Arrays 1% low | Production worst ms | Arrays worst ms |
|---|---:|---:|---:|---:|---:|---:|
| 1 | 52.68 | 56.04 | 25.15 | 21.67 | 80.72 | 106.95 |
| 2 | 53.09 | 56.02 | 25.27 | 21.88 | 91.89 | 102.83 |
| 3 | 52.93 | 55.99 | 25.15 | 21.70 | 95.19 | 105.35 |
| 4 | 52.92 | 56.05 | 24.36 | 21.64 | 97.62 | 109.03 |
| 5 | 52.96 | 55.83 | 25.25 | 21.84 | 78.73 | 106.63 |

## Approximately five-second replay sections

300 ticks = 5.008 seconds; last section is 240 ticks. Each entry is the median of five runs.

| Ticks | Production FPS | Arrays FPS | Production 1% low | Arrays 1% low | Production worst ms | Arrays worst ms |
|---|---:|---:|---:|---:|---:|---:|
| 1800-2099 | 50.71 | 55.71 | 18.81 | 17.25 | 91.89 | 106.63 |
| 2100-2399 | 56.13 | 58.11 | 30.02 | 26.42 | 34.14 | 45.20 |
| 2400-2699 | 49.85 | 55.12 | 26.00 | 23.07 | 41.21 | 49.07 |
| 2700-2999 | 52.37 | 54.91 | 30.69 | 27.76 | 34.96 | 39.88 |
| 3000-3299 | 56.72 | 57.91 | 32.24 | 24.20 | 32.36 | 52.18 |
| 3300-3599 | 49.70 | 51.92 | 21.51 | 18.78 | 56.00 | 73.88 |
| 3600-3899 | 48.71 | 51.88 | 23.56 | 19.43 | 47.67 | 72.43 |
| 3900-4199 | 59.30 | 59.10 | 36.15 | 32.28 | 31.20 | 35.48 |
| 4200-4499 | 51.32 | 55.71 | 21.82 | 18.21 | 49.40 | 64.17 |
| 4500-4799 | 53.92 | 55.12 | 24.87 | 20.45 | 52.52 | 62.11 |
| 4800-5099 | 53.71 | 55.91 | 27.75 | 29.86 | 37.79 | 36.18 |
| 5100-5399 | 50.11 | 55.71 | 30.61 | 29.77 | 33.56 | 33.60 |
| 5400-5639 | 48.93 | 54.66 | 27.63 | 21.88 | 37.48 | 54.17 |

CPU policy remained schedutil throughout; observed baseline CPU frequency 1512 MHz. Four candidate runs also remain at 1512 MHz; candidate3 has brief normal governor transitions down to 1200 MHz. This is not evidence of thermal throttling. Sensor ranges: baseline 57.57–71.10 °C, candidate 60.41–71.83 °C. Frame-pacing regression repeats in every pair, including all constant-frequency pairs.

All ten simulation endpoints are byte-identical. Audio underruns, overruns, latency resets and simulation resyncs are zero in all ten runs. Physical array peak median113.15 MiB; baseline logical/physical application world allocation is 64 MiB. Median array end allocation 90.18 MiB (70.80% logical-payload occupancy, including charged retired cohorts). No retirement-fence waits. Driver-private memory is not included in the allocation estimate.

Raw original traces and hardware correctness evidence are in raw-five-pairs-and-correctness.tgz. summary.json contains full per-run/per-section data; sections.csv contains all130 section rows.

## Startup and supplementary GPU scope

Warm-filesystem launch to first completed simulation tick: median 0.684 s for
production and 0.714 s for arrays. Atlas resources are allocated lazily.

A separate same-build nonblocking timer pair measures world command span:
median 9.660 ->8.832 ms, p95 12.949 ->11.925 ms; 3395 /3589 valid samples,
zero disjoints, pending results or query-ring drops. Its maximum span rises
15.095 ->18.612 ms. These spans include GPU feeding gaps, exclude unchanged
HUD and are not exclusive shader time. They are not added to the headline
CPU measurements or used to replace the five-pair FPS. See
`gpu-span-summary.json` and `raw-supplementary-gpu-and-frozen.tgz`.

**Decision: do not deploy or enable the atlas by default.** FPS improves 5.8%,
while 1% low declines 13.7%, median worst hitch grows 16.0% and RSS adds 44.2 MiB.
The production executable/launcher remain unchanged and EmulationStation
is restored. No commits were created.
