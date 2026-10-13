# Three-stage presentation experiment on the RG40XX H

## Executive assessment

The existing playable build already overlaps emulation and rendering. During the
recorded first lap, the `rr-present` Linux thread consumes approximately one
entire Cortex-A53 core while the simulation thread consumes approximately 0.78
of another. The application threads migrate across all four CPUs. One physical
core reaching 100% in the overlay does not identify a fixed simulation core.

The remaining opportunity is to overlap **CPU-only geometry and original HUD
preparation** with GLES submission. A separate, default-OFF
`RR_PARALLEL_FRAME_PREP` prototype performs that split. The snapshot format and
emulated clocks are unchanged. Texture cache ownership, texture decoding,
uploads, GLES draw calls, HUD uploads/composition and EGL presentation remain on
the GLES thread. Final A/B performance and deployment decisions appear below.

## Existing pipeline and measured constraint

```text
Simulation thread: 68K / DSP / sound / C352 / PDP / video boundary
        │  immutable snapshot (palette generations include dropped ticks)
        ▼
Presentation thread: point/list geometry → sorting → texture lookup/decode
        → clipping/fan/vertices → GLES uploads/draws → HUD decode/upload/compose
        → SDL_GL_SwapWindow
```

`rr_presentation_video()` copies only selected complete video states. It always
runs palette generation tracking, PDP/list boundary processing and direct FIFO
consumption on every emulated tick. A three-slot latest-frame mailbox can
discard obsolete pictures without deferring hardware-visible operations.
`rr_gl_presentation()` walks point/polygon memory and builds ordered `geo_quad`
records. `eng_gles2_draw()` uses the ordered records, shader/texture cache and
one frame-streamed VBO. The original text layer is decoded in
`rr_gl_video_compose()`, then uploaded and composed by `rr_video_layers`.
`SDL_GL_SwapWindow()` is called only by the presentation thread. C352/SDL audio
has its own output thread, but emulated sound execution remains on simulation.

A physical baseline replayed the real first lap through tick 5639, measuring
ticks 1800–5639 with the active playable executable, HUD and audio enabled,
64 MiB texture cache, frame VBO streaming and skip-geometry enabled. Stage
instrumentation (`--thread-profile`, `--profile-video`) is diagnostic and is
kept separate from the matched headline comparisons. Means below are per
presented frame; inclusive scopes must not be added to their parents.

| Stage or observation | Baseline physical measurement |
|---|---:|
| Simulation CPU, per tick | 11.721 ms |
| Presentation thread CPU | 29.417 ms |
| CPU scene geometry preparation | 3.755 ms |
| CPU original HUD/text decode | 7.049 ms |
| GLES ordered submission (includes draw calls) | 8.770 ms wall |
| Texture lookup/decode | 2.047 ms wall |
| Texture uploads | 0.656 ms wall |
| HUD texture uploads | 3.133 ms wall |
| HUD final GLES composition | 0.715 ms wall |
| Swap | 0.227 ms wall |

`rr-present` used 0.986 of a CPU core during this measurement window; the
simulation main thread used 0.778. The Mali framebuffer worker used about
0.057, SDL audio about 0.011 and other driver/audio workers less. All main
threads migrated among CPUs 0–3. Mean per-core utilisation ranged 41–69%; a
core sampled at 100% reflects scheduling/migration rather than sustained
whole-device CPU saturation. The `rr-present` thread was sampled runnable in
all 49 one-second samples; simulation was runnable in 36 and sleeping in 13,
consistent with fixed-timestep waits. The monitor's coarse sampling cannot
resolve sub-millisecond kernel/GPU waits. During the measured window, CPU
frequencies sampled 1.2–1.512 GHz (median 1.512 GHz); CPU thermal nodes were
53.0–60.8 °C. No sustained frequency fall was observed. GPU frequency was
reported as 240 MHz, but no reliable GPU utilisation counter was available.
The `perf` executable is absent on this Knulli installation.
The CPU thermal zone exposes passive trips at 60 and 70 °C, and `schedutil`
controls CPU frequency. Frequency samples are observations rather than proof
of thermal throttling; the A/B monitor records both frequency and temperature
so anomalous runs can be inspected without conflating temperature, driver
work and game workload.

The CPU-only scene + HUD scopes total approximately 10.8 ms of work on the
original presentation thread. If this entire work overlaps perfectly, the
remaining approximately 18.6 ms presentation-thread CPU gives a **theoretical
upper bound near 54 FPS** before handoff, cache and memory-bandwidth costs. This
is not a prediction of measured FPS. It also shows this split alone cannot
sustain 59.904 FPS: the remaining presentation stage exceeds the 16.693 ms
arcade period by about 1.9 ms, even before difficult texture bursts.

## Experimental ownership model

```text
Simulation             CPU preparation                 GLES presentation
──────────             ───────────────                 ─────────────────
PDP + all hardware     newest READY snapshot          newest PREPARED snapshot
copy video snapshot ──► geometry/list/projection       apply frame palette
                      sort indices                    texture cache + GL draws
                      decode text/shadow ───────────► upload HUD + compose
                                                      swap
```

The same three-slot mailbox has explicit `FREE`, `WRITING`, `READY`,
`PREPARING`, `PREPARED` and `READING` ownership. No snapshot or prepared arena is
mutated once published to the next stage. Only unowned `READY` snapshots can
be replaced by the producer or preparer; the consumer is the sole owner of a
`PREPARED` snapshot until it becomes `READING`. Capture/fixed-cadence
tests wait for their selected frame to finish. Shutdown drains bounded pending
work; the preparer signals completion before the GLES owner exits. Mutexes
protect only metadata/state transitions, not geometry, cache work or GL calls.

The first unpaced prototype decoded nearly every emulated tick although GLES
displayed only about 39 pictures/s. Its preparer now stops when one frame is
`PREPARED`; the producer retains the newest `READY` snapshot, replacing only
older unprepared snapshots while GLES is busy. When GLES takes the prepared
frame, the preparer wakes and processes the newest ready state. This bounds
speculative preparation and preserves the GPU consumer's completed frame.
The current source gives a newer simulation snapshot up to 6 ms to replace an
old `READY` one after GLES claims the previous picture. The 6 ms cap is below
the measured ~8.5 ms mean overlap slack between GLES-thread execution and CPU
preparation. The worker never holds the metadata mutex while waiting. A strict
late-latch experiment that waited for the next captured tick limited added age
to 3.1 ms in one physical smoke pair, but lowered rendered FPS to 37.38 and
worsened its 1% low; it was rejected. Fixed-cadence captures retain their
wait-for-completion behaviour.

Each slot owns its reusable geometry arena, ordered indices and two 640×480
RGBA HUD buffers. The initial geometry arena is 2048 × 1044 bytes per used
slot and grows only if a frame needs more; no full `geo_quad` copy is made at
handoff. The three HUD buffer pairs reserve 7,372,800 bytes in total. Their
content is copied from the existing CPU decoder into slot-owned storage, never
read from emulated memory by the GLES consumer. The preparer owns the list
walker, sorting keys and HUD decoder cache; the GLES thread owns palette
conversion, texture cache, VBO, EGL context and all GLES objects. A thread-local
snapshot selector prevents the two host stages from sharing the emulated-memory
view. Ordered indices are copied as compact 32-bit values, so the GLES thread
never reads sorting globals while preparation continues.

HUD source changes have a monotonically increasing preparation generation.
The GLES thread compares that generation with the last uploaded one, preserving
uploads even if an intermediate prepared picture is discarded. Likewise, the
full palette and per-group generation are applied when the chosen frame reaches
the GLES thread. GPU texture storage is not modified or depended upon by the
preparer. Original polygon ordering, scissor state, palette and priority rules
remain with the existing GLES renderer.

CPU texture decoding is deliberately still on the GLES thread: the current
cache miss path may synchronously evict a live texture, drain ordered commands,
create/upload a GLES texture and update palette-keyed cache entries. Moving it
would require a separately versioned staging protocol and cannot safely reuse
the present cache API from another thread. This is the main unoverlapped burst
work around tick 1965.

The feature requires `RR_THREADED_PRESENTATION=ON` and
`RR_GEOMETRY_INDEX_ORDER=ON`. Its CMake default is OFF. Existing serial,
snapshot-reference and ordinary two-thread paths remain available. A profile
CSV adds preparation CPU, HUD CPU, snapshot-to-prep and prep-to-consumer age
columns; `[PREP-RATE]` reports preparation throughput and discarded prepared
pictures. These are diagnostic measures; presented FPS always uses swap-return
timestamps and simulation Hz always uses completed emulated ticks.

## Build and reproduction

Use the matched H700 Buildroot toolchain via the existing preset. These builds
remain separate from the active Ports executable:

```sh
cd raverace
cmake --preset playable-knulli -B /tmp/rr-multicore-arm64 \
  -DRR_PARALLEL_FRAME_PREP=ON
cmake --build /tmp/rr-multicore-arm64 -j4
file /tmp/rr-multicore-arm64/rr_gles2
readelf -l /tmp/rr-multicore-arm64/rr_gles2 | grep interpreter
readelf -d /tmp/rr-multicore-arm64/rr_gles2 | grep NEEDED

cmake --preset playable-native -B /tmp/rr-multicore-native \
  -DRR_PARALLEL_FRAME_PREP=ON -DRR_TEXTURE_TILE_RUNS=ON
cmake --build /tmp/rr-multicore-native -j4
```

The final ARM binary is AArch64 for Linux 4.9 with interpreter
`/lib/ld-linux-aarch64.so.1`. Its GLES dependencies are `libGLESv2.so.2`,
`libEGL.so.1`, `libmali.so.0` and `libSDL2-2.0.so.0`; it does not require
desktop `libGL`. Its highest referenced glibc symbol version is 2.38, below
Knulli's glibc 2.40.

For the physical A/B comparison, copy the current playable binary as
`/userdata/system/rave-racer-multicore/rr_baseline`, the candidate as
`rr_parallel_prep`, and the known deterministic tick-5639 endpoint as
`reference-5639.json`. Use `tools/RaveRacer-Multicore-Replay-Benchmark.sh`,
which refuses an active game, stops/restores EmulationStation in a trap,
hashes the executables/replay, runs five alternating A/B pairs and keeps logs,
per-presentation/tick CSVs and per-thread `/proc` samples in a dated directory.

```sh
scp /tmp/rr-multicore-arm64/rr_gles2 \
  root@192.168.1.69:/userdata/system/rave-racer-multicore/rr_parallel_prep
scp tools/RaveRacer-Multicore-Replay-Benchmark.sh \
  root@192.168.1.69:/userdata/system/rave-racer-multicore/benchmark.sh
ssh root@192.168.1.69 \
  'sh /userdata/system/rave-racer-multicore/benchmark.sh \
   /userdata/system/rave-racer-replays/race-20261013-122242-25136.rec 1800 5639 5'
scp -r root@192.168.1.69:/userdata/system/rave-racer-multicore/ab-DATE-TIME-PID \
  /tmp/rr-multicore-results
python3 tools/analyse_multicore_replay.py /tmp/rr-multicore-results 1800 5639
```

The runner uses audio and original HUD, disables the performance overlay,
retains the 64 MiB GPU texture cache and identical replay/ROM/EEPROM state. It
uses the same traced mode on both sides. A separate `--profile-video` run is
needed for CPU HUD decode versus GLES upload/composition attribution; its
clock overhead must not be mixed into headline FPS. The five-second section
report is `sections.csv`; complete run statistics are `summary.json`.
For the diagnostic-only stage build, add `-DRR_PARALLEL_PREP_PROFILE=ON` to
the CMake preset in a separate build directory and run the same replay with
`--profile-from 1800 --thread-profile presentations.csv --profile-video
--thread-sync-profile`. Use the source test script
`tools/RaveRacer-Multicore-Replay-Benchmark.sh` for new A/B headline data;
never compare FPS from a stage-instrumented executable to an uninstrumented
one.
Each 300-tick section represents about five seconds of *emulated* time; its
actual elapsed wall time and simulation Hz are derived from the tick CSV.
Only presented swap-return timestamps enter rendered FPS and frame-interval
percentiles. The monitor samples Linux task CPU ticks, last-used CPU,
wait-channel and core frequency/thermal state once per second; it cannot
resolve brief driver waits or report GPU execution time. The game has no
reliable Mali utilisation counter on this firmware.

The initial unpaced experiment is retained as diagnostic evidence, not as the
deployment candidate. Five alternating physical A/B pairs produced these
medians over ticks 1800–5639:

| Metric | Existing | Unpaced preparer |
|---|---:|---:|
| Rendered FPS | 33.384 | 39.079 |
| 1% low FPS | 18.126 | 17.559 |
| p99 interval | 46.513 ms | 45.537 ms |
| Largest interval | 90.575 ms | 102.254 ms |
| Simulation rate | 59.904 Hz | 59.904 Hz |
| Presentation thread CPU/picture | 29.414 ms | 24.500 ms |
| Preparation CPU/prepared picture | — | 14.340 ms |
| Snapshot age | 38.570 ms | 48.628 ms |
| Process peak RSS (`VmHWM`) | 275.977 MiB | 294.871 MiB |

The unpaced worker consumed approximately 0.865 core, compared with the GLES
thread's 0.960 core. It prepared roughly 1,500 pictures per run that were
discarded before display. Two of five runs resynchronised the simulation clock
(one or two resyncs); the baseline had none. The gain in median FPS therefore
does not justify deploying that policy. A single paced smoke pair gave 33.275
versus 41.889 FPS and 18.432 versus 20.426 FPS at the 1% low, but raised
snapshot age from 38.539 to 55.667 ms. These single-pair figures are a design
signal, not a repeatable performance claim.

Five alternating pairs for the paced policy confirmed 33.618 versus 42.012
rendered FPS and 18.116 versus 20.426 FPS at the 1% low. Its median p99
interval fell from 45.903 to 39.722 ms, but snapshot age rose from 38.345 to
55.238 ms, and the median largest hitch rose from 93.068 to 100.322 ms. One
candidate run and one baseline run had notable resynchronisation/audio
underrun disturbances. The candidate's CPU thermal maximum reached 68.4–70.0
°C versus 63.7–65.6 °C for baseline, near the device's 70 °C passive trip.
The median simulation rate was 59.904 Hz in both variants, but this did not
hold in every individual run. The paced variant is therefore not deployed.

The final bounded policy improved FPS in every five-second emulated section;
the lowest-throughput section remained ticks 3600–3899. Figures below are
medians of five runs, with the same traced mode on both sides:

| Replay ticks | Existing FPS | Bounded FPS | Existing 1% low | Bounded 1% low |
|---|---:|---:|---:|---:|
| 1800–2099 | 36.74 | 42.14 | 13.62 | 17.19 |
| 2100–2399 | 36.35 | 43.32 | 23.40 | 26.84 |
| 2400–2699 | 30.35 | 38.34 | 17.83 | 21.93 |
| 2700–2999 | 31.54 | 39.92 | 21.96 | 26.25 |
| 3000–3299 | 35.15 | 43.74 | 23.55 | 26.78 |
| 3300–3599 | 30.15 | 37.94 | 14.94 | 17.33 |
| 3600–3899 | 30.15 | 37.02 | 17.86 | 18.21 |
| 3900–4199 | 38.73 | 46.93 | 24.00 | 30.01 |
| 4200–4499 | 32.16 | 40.74 | 16.45 | 19.65 |
| 4500–4799 | 33.94 | 41.93 | 17.88 | 20.89 |
| 4800–5099 | 32.55 | 40.14 | 22.06 | 24.59 |
| 5100–5399 | 31.54 | 38.54 | 21.85 | 25.14 |
| 5400–5639 | 31.20 | 38.19 | 20.82 | 25.62 |

## Correctness and measured outcome

The native fixed-presentation captures at frames 600, 1700 and 3600 match the
previous playable renderer byte for byte, with identical ordered geometry,
board state and PCM/voice digests. The native snapshot/serial/thread/drop
regression checks fixed-cadence texture events and deterministic state. The
same-GPU H700 baseline/candidate fixed-frame captures at 600, 1700 and 3600
also match pixels, geometry and state/audio digests. A native ThreadSanitizer
frame-1700 run reports no application race and matches the reference image and
state; the known Mesa `libgallium` internal report was suppressed, and this
cannot detect races in uninstrumented Mali code. Actual first-lap replay
captures at ticks 1965 and 3600 match pixels, ordered geometry, PCM/voice,
board and simulation state on the same Mali GPU. All 63 saved state records
(21 checkpoint sets) through tick 6006 match the original recording byte for
byte. This continuous run had zero audio underruns, clock resyncs, traps,
DSP faults and sound faults. Build-only success is not considered validation.

| First-lap metric, ticks 1800–5639 | Existing presentation | Bounded three-stage prototype |
|---|---:|---:|
| Median rendered FPS (five runs) | 33.431 | 41.279 (+23.5%) |
| Median 1% low FPS | 17.890 | 20.624 (+15.3%) |
| Median p99 presentation interval | 46.521 ms | 39.159 ms |
| Median largest hitch | 92.670 ms | 101.590 ms |
| Simulation Hz | 59.904 | 59.904 |
| Presentation-thread CPU/picture | 29.364 ms | 22.532 ms |
| Preparation CPU/picture | — | 14.767 ms |
| Snapshot age to swap-return | 38.505 ms | 49.847 ms |
| Process peak RSS (`VmHWM`) | 277.926 MiB | 293.172 MiB |
| Audio underruns | 0/5 runs | 0/5 runs |

The five candidate FPS results were 41.329, 40.889, 41.527, 40.716 and
41.279; baseline was 33.509, 33.431, 33.274, 33.556 and 33.322. Four
candidate runs sustained the target with zero resyncs; one resynchronised once
and discarded 16.9 ms of lag. All five baseline runs had zero resyncs. The
candidate raised mean simulation CPU from approximately 11.7 to 13.2 ms/tick,
consistent with increased shared-core/cache/memory pressure; the simulation
still met its 59.904 Hz target in the median run. The preparer used a median
0.620 core, GLES 0.941 core and the simulation process main thread 0.868
core. Both rendering threads migrated among all four CPUs. The candidate's
CPU thermal maximum was 68.7–69.2 °C across runs versus 64.2–65.8 °C in
baseline. Sampled CPU frequency stayed at 1512 MHz in these five candidate
runs; one-second sampling can miss short changes.

Draw counts were similar (median 859 baseline, 863 candidate per picture), as
were triangle and texture-bind counts. They need not be numerically identical
in real-time runs because the two schedulers present different subsets of the
same emulated ticks; fixed-frame comparisons are the visual oracle. The
prepared queue never exceeded two ready pictures and the bounded preparer did
not discard completed preparations. Around tick 1965, both variants had
roughly 77 texture misses/uploads. The candidate's larger 101.6 ms median
worst hitch includes more texture-upload wall time than baseline. This split
improves normal throughput and p99 intervals but does not solve rare texture
bursts. Snapshot age is a software image-age estimate, not measured physical
controller-to-photon latency; its 11.342 ms increase was assessed in an
interactive handheld test. The player reported responsive controls and chose
to retain the prototype.

## Diagnostic attribution and limits

A separate, diagnostic-only physical first-lap run enabled
`RR_PARALLEL_PREP_PROFILE`, `--profile-video` and `--thread-sync-profile`.
Its extra clocks are excluded from the A/B FPS figures. It reported 14.590 ms
preparation-thread CPU per prepared picture: 10.879 ms for the original HUD
CPU path including immutable-buffer copies, 2.978 ms polygon list/decode/
transformation, 0.674 ms sorting and 0.015 ms direct polygons. The GLES
thread used 21.924 ms CPU/picture. Its draw scope was 16.017 ms wall,
including 10.441 ms ordered GLES submission, 1.832 ms texture lookup/decode
scope (0.507 ms pure decode) and 0.535 ms texture upload. HUD GPU uploads
cost 4.790 ms CPU/picture and final HUD composition 0.872 ms; swap averaged
0.397 ms wall. Draw, submission, lookup and upload scopes nest, so they must
not be summed as independent costs. The baseline's earlier diagnostic render
thread used 29.417 ms CPU/picture, with 3.755 ms scene and 7.049 ms original
HUD CPU decode. Memory and driver contention change stage costs when they
execute in parallel.

Across the diagnostic run, metadata-lock acquisition totalled 13.2 ms on the
simulation producer, 11.3 ms on the preparer and 4.3 ms on GLES. The preparer
spent 22.83 s in condition waits over the roughly 64 s measured replay; the
GLES consumer spent 5.86 s waiting for prepared frames. The one-second
`/proc` samples and these counters support real execution overlap without a
global render lock. They do not reveal short waits inside the Mali driver.
No reliable GPU execution/utilisation counter or `perf` sampling facility was
available on this firmware.

The measured GLES-owner cost alone implies a CPU-side ceiling around
1/0.0225 ≈ 44 presentations/s in the traced comparison. The 16.693 ms arcade
period would require at least 5.8 ms less CPU per GLES presentation (about
26%) and more simulation margin under load. Splitting preparation across
cores does not by itself provide a credible route to sustained 59.904 rendered
FPS. Another geometry thread would not remove this serial GLES-context
bottleneck. The tick-1965 texture burst is the largest rare hitch; moving its
CPU decode to a worker would need a separate immutable staging/cache protocol
because current misses can evict and upload GLES objects synchronously. That
is a separate experiment, not part of this prototype.

## Handheld deployment and recovery

The bounded prototype was promoted after matched A/B and full replay
correctness. The normal binary is
`/userdata/roms/ports/rave-gles2/rr_gles2` (SHA-256
`77536f56ad17de9cf8772175c73442d9cb246d87aeb6fbf6c8d7f07c3d6ab53e`).
The previous working executable and launcher are preserved at
`/userdata/system/rave-racer-backups/20261013-pre-parallel-prep/` (old binary
SHA-256 `ed27bc04b41c402e2181a1f36c4b3876becffea6224a9499bd9a181120dbedb1`).
`/userdata/roms/ports/Rave Racer.sh` is the sole active launcher and was not
edited. EmulationStation was restored after deployment; the installed binary
completed a 600-tick replay/GLES launch smoke. The full pre-deployment replay
had no audio underruns. The short startup smoke reported an ALSA underrun,
which did not recur in the interactive session. The player confirmed that
controls felt responsive and original HUD and audio remained correct. That
session ran 23,604 emulated ticks and exited normally; the final log reported
zero audio underruns, traps, unmapped accesses, ROM writes, DSP faults and
sound faults. It averaged 59.9049 simulation Hz and 47.9843 rendered FPS
across a player-driven route; this is not directly comparable with the
first-lap replay window. One scheduler resync discarded 18.1 ms of lag. To
restore the old binary, stop the game and EmulationStation, copy the backed-up
`rr_gles2` over the active one, then restart EmulationStation.

The device keeps the validated baseline/candidate pair and dated A/B,
diagnostic and correctness logs under
`/userdata/system/rave-racer-multicore/`. Obsolete unpaced, strict-latch and
diagnostic binaries were removed from that test directory. No extra Ports
launchers were created and no repository commit was made.

The specific handheld result directories are `ab-20261013-153821-18542`
(unpaced), `ab-20261013-160343-30698` (paced),
`ab-20261013-162808-17838` (final bounded five-pair comparison),
`replay-correctness-20261013-164429-22215` and
`diagnostic-20261013-165148-22678`. The paired manifest hashes, complete
logs, timestamp CSVs and one-second `/proc` samples are retained there. The
native flag-OFF renderer, desktop renderer and headless deterministic checks
also passed; the final bounded native snapshot/serial/thread/drop suite
matched 600/1700/3600 pixels, ordered geometry, fixed-cadence texture events,
PCM/voice and board/simulation state. Native ThreadSanitizer at frame 1700
reported no application race with the Mesa-internal suppression noted above.
