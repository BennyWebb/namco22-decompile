| Metric: median of five accepted racing windows | Production | Original arrays | Retained arrays |
|---|---:|---:|---:|
| Rendered FPS | 52.900 | 55.957 | 55.801 |
| 1% low FPS | 24.899 | 22.151 | 21.486 |
| p99 presentation interval ms | 32.685 | 33.807 | 35.648 |
| Worst presentation interval ms | 79.034 | 101.651 | 101.133 |
| Image age median ms | 37.634 | 33.286 | 32.648 |
| Image age p95 ms | 55.031 | 50.500 | 51.131 |
| GLES-owner CPU ms/picture | 16.986 | 15.413 | 15.370 |
| Submission wall ms/picture | 9.608 | 7.385 | 7.406 |
| Allocation/upload/retirement scope ms/picture | 0.476 | 0.865 | 0.831 |
| Decode wall ms/picture | 0.452 | 0.436 | 0.438 |
| World draws/picture | 859.991 | 439.419 | 438.685 |
| World/cache binds/picture | 863.258 | 443.834 | 442.845 |
| Peak process RSS MiB | 286.961 | 334.145 | 393.242 |
| Simulation Hz | 59.904 | 59.904 | 59.904 |

Normal benchmarks retain the established combined texture scope. Per-API
allocation/upload separation is reported only in the controlled diagnostics;
those independent timings are not distributed into this table.

| Lifetime counter / allocation | Production | Original arrays | Retained arrays |
|---|---:|---:|---:|
| Physical array peak MiB | 64.00 | 114.08 | 128.00 |
| Physical array end MiB | 64.00 | 89.73 | 127.87 |
| End logical-payload occupancy % | — | 71.15 | 49.94 |
| Cohorts allocated, whole process | — | 7494.00 | 6320.00 |
| Existing-cohort matches, whole process | — | 6714.00 | 7886.00 |

The existing-cohort counter includes filling a virgin second layer, reused
holes, and (for retention) completed empty cohorts; it is not a count solely
of overwritten layers. Peak/end array figures include charged retired/idle
storage. Driver-private allocations are not included.

| Round | Production FPS / 1% low / worst ms | Original FPS / 1% low / worst ms | Retained FPS / 1% low / worst ms |
|---|---:|---:|---:|
| 1 | 52.73 / 24.90 / 78.84 | 55.86 / 21.80 / 109.03 | 55.47 / 21.00 / 93.96 |
| 3 | 52.90 / 24.77 / 80.66 | 55.85 / 22.15 / 100.09 | 55.80 / 21.62 / 104.04 |
| 4 | 52.68 / 24.52 / 80.35 | 55.96 / 22.21 / 103.97 | 55.85 / 21.49 / 97.42 |
| 5 | 52.91 / 25.41 / 79.03 | 55.99 / 22.07 / 94.37 | 56.00 / 21.58 / 104.68 |
| 6 | 53.10 / 25.75 / 78.85 | 56.08 / 22.40 / 101.65 | 55.64 / 21.00 / 101.13 |

| Approximately five-second section | Production FPS / 1% low / worst ms | Original FPS / 1% low / worst ms | Retained FPS / 1% low / worst ms |
|---|---:|---:|---:|
| 1800-2099 | 50.13 / 19.64 / 79.03 | 55.30 / 17.69 / 101.65 | 55.91 / 17.67 / 101.13 |
| 2100-2399 | 55.90 / 29.98 / 33.57 | 58.30 / 26.85 / 40.69 | 58.30 / 24.14 / 44.03 |
| 2400-2699 | 50.67 / 25.67 / 45.28 | 54.92 / 24.01 / 48.33 | 54.31 / 21.47 / 57.70 |
| 2700-2999 | 52.12 / 31.27 / 33.20 | 54.92 / 27.29 / 40.52 | 54.71 / 25.50 / 42.68 |
| 3000-3299 | 56.91 / 32.30 / 32.00 | 57.92 / 24.89 / 47.67 | 58.12 / 25.20 / 48.03 |
| 3300-3599 | 49.72 / 21.32 / 57.74 | 51.43 / 18.89 / 66.39 | 51.72 / 18.95 / 66.09 |
| 3600-3899 | 48.52 / 23.56 / 48.39 | 51.31 / 20.73 / 58.44 | 51.92 / 20.17 / 61.52 |
| 3900-4199 | 59.11 / 35.90 / 30.93 | 59.30 / 34.01 / 32.60 | 59.09 / 29.97 / 38.71 |
| 4200-4499 | 52.11 / 21.77 / 50.08 | 54.92 / 17.90 / 58.32 | 55.32 / 17.72 / 58.77 |
| 4500-4799 | 54.12 / 24.50 / 53.91 | 55.87 / 22.38 / 59.90 | 55.90 / 21.93 / 60.87 |
| 4800-5099 | 53.90 / 30.14 / 33.56 | 56.33 / 26.40 / 44.19 | 55.90 / 28.29 / 38.20 |
| 5100-5399 | 49.51 / 30.11 / 33.55 | 56.11 / 29.68 / 34.12 | 55.50 / 28.16 / 37.37 |
| 5400-5639 | 49.17 / 27.09 / 37.46 | 54.15 / 23.04 / 47.02 | 53.66 / 24.09 / 44.18 |

| Tick-1965 burst: median of five | Production | Original arrays | Retained arrays |
|---|---:|---:|---:|
| Misses | 77.000 | 77.000 | 77.000 |
| Upload/lifecycle scope ms | 14.947 | 48.731 | 49.772 |
| Eviction scope ms | 25.180 | 11.078 | 10.401 |
| Decode ms | 23.625 | 25.034 | 24.996 |
| Submission ms | 9.130 | 10.225 | 9.633 |
| Owner CPU ms | 78.680 | 98.725 | 100.051 |
| Presentation interval ms | 79.034 | 101.651 | 101.133 |
