# Human-driven first-lap benchmark — 2026-10-13

## Dataset and scope

The player recorded a lap on the physical RG40XX H and confirmed that original
HUD, audio and controls remained correct. This is a **human-driven first lap**,
not a complete multi-lap race or a generated driving sequence.

- Recording: `/userdata/system/rave-racer-replays/race-20261013-122242-25136.rec`.
- 6006 completed ticks, 100.26 emulated seconds, 524 input changes, 12,660 input bytes.
- Marker 659 was in the menu **before coin insertion**, not a race-start marker.
- Marker 5639 followed the first lap, as confirmed by the player.
- Primary moving-race window: **1800–5639**, 3840 ticks, approximately 64.10 seconds.
- Native captured frame 1800 shows 110 km/h; frame 2100 shows 202 km/h and advancing
  scenery. The conservative start excludes boot, track selection and countdown,
  rather than treating the first marker as racing or inventing a precise green-light tick.

The historical dense opening 1400–1700 and extended drive/wall tail are preserved.
Neither is the same workload as this recording. Do not interpret differences in
FPS as a renderer regression or label this lap a distinct renderer/game mode.

## Determinism and functionality

All **21 original-recording checkpoints through tick 6006** matched on native
Linux and ARM64 headless playback: translated CPU registers/RAM, board/video RAM,
raw four-channel PCM, C352 voice history and sample counts. Original final state,
including the last tick on controller exit, was reproduced. Every benchmark run
also matched its headless reference at tick 5639. No traps, unmapped accesses,
ROM writes, DSP faults or sound faults appeared.

Equivalent-frame checks using the actual recorded inputs passed:

- Native frame 4800: serial vs threaded pixels, ordered geometry, board/state and
  PCM/voice digests identical.
- Mali/ARM64 frame 1800: serial vs threaded pixels, ordered geometry, board/state,
  checkpoint files and PCM/voice digests identical.

These checks establish more than a visually similar route. Vehicle position,
speed and heading were not separately decoded into diagnostic fields; their
stored WRAM/register state is covered by the deterministic checkpoints.

Verification results:

```text
/userdata/system/rave-racer-replay/human-verify-20261013-123024-25848
/userdata/system/rave-racer-replay/human-pixels-20261013-124912-27080
```

## Measurement protocol

Three alternating untraced/traced pairs, all validated optimisations enabled,
independent timing, skipped geometry on discarded presentations, frame VBO,
64 MiB texture cache, original HUD and host audio enabled, performance overlay
**off**. Input and initial EEPROM/ROMs are identical in every run.

Raw PCM verification runs are separate from headline runs (`--replay-checks off`
for performance). Untraced runs quantify throughput without CSV/tick tracing;
traced runs produce complete presentation/tick streams. No forced presentation
cadence or screenshot readbacks are used in these performance runs.

Simulation Hz and displayed/presented throughput are separate. Presentation
intervals use monotonic **swap-return** timestamps, not emulated tick counts;
physical scanout and GPU execution time are not independently measured.
Each short section uses the same completed-tick wall-clock interval for simulation
Hz and presentation counts. 1% low = reciprocal mean of the slowest ceil(1%)
intervals. Whole-lap tail statistics have over 2100 samples; five-second sections
have approximately 150–200 and only a few 1% tail samples.

The optional drop counter is sampled after swap. Exact whole-window counts come
from the runtime log; section deltas subtract the startup prefix using its
`total_dropped` and range `dropped` values. A few drops near section/end boundaries
can be attributed to the neighbouring sample, so use unpresented tick counts and
whole-window drop totals alongside this sampled distribution.

## Overall results

Three-run medians; frame-interval statistics are medians of each traced run's statistic:

| Metric | Untraced | Traced |
|---|---:|---:|
| Simulation Hz | 59.903819 | 59.903682 |
| Rendered FPS | 33.914 | 33.696 |
| Simulation CPU ms/tick | 11.446 | 11.473 |
| Presentation thread CPU ms | 29.124 | 29.134 |
| Scene preparation wall ms | 3.666 | 3.695 |
| Snapshot publication-to-swap age ms | 38.084 | 38.132 |
| Mean presentation interval ms | N/A | 29.661 |
| Median presentation interval ms | N/A | 28.992 |
| p95 interval ms | N/A | 38.569 |
| p99 interval ms | N/A | 48.989 |
| 1% low FPS | N/A | 16.374 |
| Maximum interval ms | N/A | 108.333 |
| READY snapshot drops | 1664 | 1678 |

Untraced FPS range: 33.805–34.009; traced range: 33.618–34.039. Median throughput
difference is -0.645%; pair differences were +0.692%, -1.149% and -0.645%.
Trace overhead is small in this experiment, but three pairs cannot separate it
precisely from normal variation. Use untraced throughput for headline numbers,
traced data for pacing distributions and section comparisons.

All six runs: **zero timing resyncs, zero discarded lag, zero audio underruns,
overruns or latency resets**, queue peak two. Simulation sustains the arcade
frequency while presentation does not. Approximately 43–44% of simulation ticks
have no presented frame; obsolete READY snapshots are replaced rather than
stalling simulation. Average READY queue occupancy is about 1.72/2, and average
snapshot age is about 38 ms; neither is a measured input-to-photon delay.

Typical traced presentation: 859 draws, 864 texture bindings, 1814 triangles;
3D inclusive draw wall time 14.855 ms, submission wall time 8.538 ms, swap wall
0.221 ms. The latter stages are **nested**, not additional costs to sum.
Total presentation thread CPU includes scene, 3D, original HUD/fade/gamma and
host submission. Submission wall time includes application work and driver
calls; it is not isolated GPU time or exclusive CPU-only GLES time.

Median racing-window texture activity: 7651 misses/uploads and 8841 evictions.
Logical texture storage remains bounded at 64 MiB. Actual driver allocations,
GPU utilisation and process memory were not newly sampled in these runs.

## Short racing sections

About 300 ticks per section (5.008 emulated seconds); the final section has 240.
Values below are medians across the three traced runs. All section simulation
rates are within 59.893–59.914 Hz.

| Emulated ticks | Presented FPS | Mean ms | Median ms | 1% low FPS | p99 ms |
|---|---:|---:|---:|---:|---:|
| 1800–2099 | 37.33 | 26.51 | 27.09 | 11.62 | 63.82 |
| 2100–2399 | 36.75 | 27.16 | 27.84 | 21.94 | 45.40 |
| 2400–2699 | 30.75 | 32.39 | 31.01 | 16.40 | 58.51 |
| 2700–2999 | 32.34 | 31.13 | 29.95 | 18.28 | 46.30 |
| 3000–3299 | 35.55 | 28.13 | 27.96 | 21.87 | 43.56 |
| 3300–3599 | 30.95 | 32.14 | 29.73 | 13.58 | 66.98 |
| 3600–3899 | 30.96 | 32.29 | 31.75 | 16.23 | 57.76 |
| 3900–4199 | 39.93 | 25.02 | 25.21 | 23.21 | 35.74 |
| 4200–4499 | 33.34 | 29.97 | 29.33 | 15.31 | 63.16 |
| 4500–4799 | 34.34 | 29.14 | 27.04 | 15.80 | 53.52 |
| 4800–5099 | 32.95 | 30.34 | 28.91 | 19.94 | 49.27 |
| 5100–5399 | 32.15 | 31.17 | 30.77 | 21.23 | 45.70 |
| 5400–5639 | 31.95 | 31.39 | 31.05 | 19.45 | 50.62 |

The lowest sustained sections are 2400–2699 (30.75 FPS), 3300–3599 (30.95),
3600–3899 (30.96), and 5400–5639 (31.95). The relatively light 3900–4199 section
reaches 39.93 FPS. These are different views along the same recorded lap, not
new tracks or rendering configurations.

## Repeatable stutters

All three traces reproduce the same scene-related hitch locations:

| Tick(s) | Interval | Presentation CPU | Misses/uploads | Swap wall |
|---|---:|---:|---:|---:|
| 1965 | 106.48–108.64 ms | 106.40–108.21 ms | 77 | 0.11–0.26 ms |
| 3386–3387 | 76.70–81.72 ms | 76.64–81.65 ms | 56–60 | about 0.12 ms |
| 4745–4746 | 72.63–74.70 ms | 71.95–73.61 ms | 53–55 | about 0.11–0.13 ms |

The first hitch has 960 draws, about 3.4–3.7 ms scene preparation,
91.6–93.5 ms inclusive 3D draw work, and 8.9–10.5 ms submission. The spike is
mostly outside the measured submission subset, alongside 77 texture misses and
uploads. Presentation CPU is close to the entire interval and swap is short,
so these traces do **not** support a swap/vblank wait as the main cause.
This is evidence of a presentation-side texture/preparation burst, not proof
that decoding alone causes it: decode, upload staging and driver CPU work still
need targeted separation at these ticks.

There is a median 18 intervals over 50 ms and one over 100 ms in each traced
racing window. Their scene-aligned repetition is useful for future profiling;
there is no evidence here of a simple wall-clock-periodic timer causing them.
Steady low-FPS sections also have higher geometry/submission costs: for example,
2400–2699 averages 946 draws and 31.86 ms presentation CPU, whereas 3900–4199
averages 562 draws and 24.93 ms. Geometry count is an association, not an isolated
measurement of cost per polygon or a reason to reorder transparent geometry.

**Recommended next investigation:** profile texture lookup/decode, uploads and
remaining original HUD composition separately around 1960–1970, 3380–3395 and
4740–4755, using this same replay. Do not increase the cache or implement a new
renderer based only on these correlations. No rendering optimisation was made
in this task.

## Reproduction

Recording/format/build instructions are in
[INPUT_RECORDING_REPLAY.md](INPUT_RECORDING_REPLAY.md).
The one-shot setting has already returned to `off`; only one normal Rave Racer
launcher remains. The known-good primary binary is unchanged and EmulationStation
was restored after tests. No commits were created.

Run the same controlled first-lap benchmark:

```sh
ssh root@192.168.1.69 \
 'sh /userdata/system/rave-racer-replay/RaveRacer-Replay-Benchmark-v3.sh \
 /userdata/system/rave-racer-replays/race-20261013-122242-25136.rec 1800 5639 3'
```

Retrieve results and generate complete per-section/aggregate reports:

```sh
scp -r root@192.168.1.69:/userdata/system/rave-racer-replay/results-20261013-123359-26138 ./lap-results
python3 raverace/tools/summarise_race_replay.py ./lap-results 1800 5639
python3 raverace/tools/analyse_race_replay.py \
 lap-results/run-1.presentations.csv lap-results/run-1.ticks.csv 1800 5639 lap-results/run-1.log
```

Small checked results are preserved in
[benchmarks/human-lap-20261013-summary.json](benchmarks/human-lap-20261013-summary.json)
and [benchmarks/human-lap-20261013-sections.csv](benchmarks/human-lap-20261013-sections.csv).
Original replay, EEPROM sidecar, checkpoint files and complete raw traces remain
on the handheld. Input files and user data were never replaced or deleted.

For broader coverage, record a full multi-lap race or another track using the
same one-shot launcher. Mark after countdown and at the actual finish to define
racing bounds; these markers remain manual annotations, not automatic game-state
classification. The present measurements describe this one representative lap.
