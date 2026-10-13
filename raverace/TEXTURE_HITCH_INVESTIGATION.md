# Recorded-lap texture hitch investigation — 2026-10-13

The tile-run decoder was retained and deployed after five-pair handheld A/B
validation. The earlier playable binary is preserved separately.

## Workload and protocol

Human replay `/userdata/system/rave-racer-replays/race-20261013-122242-25136.rec`,
primary ticks 1800–5639. Original HUD and SDL audio enabled, overlay off,
independent timing, skipped geometry enabled, frame VBO, 64 MiB texture budget.
WRAM/split reader, C352 inactive voices, ordered submission, index order,
projection reuse and threaded presentation enabled in both variants.

The main thread owns emulation and creates immutable snapshots. The presentation
thread owns palette conversion, ROM texture decoding, GPU cache, GLES context,
HUD composition and swap. Texture tile/pixel ROMs are loaded and fixed up before
threading, then immutable; palette state comes from each immutable snapshot.
There is no worker reading mutable emulation memory in this experiment.

## Initial measured attribution

A full baseline trace and a focused baseline trace reproduce the tick-1965 hitch.
The focused trace reports **107.407 ms presentation thread CPU**, **41.369 ms
RGBA decoding**, **14.247 ms texture allocation/upload**, and **25.556 ms byte-budget
eviction**. There are 77 misses/uploads, 1,874,368 decoded texels, 444 evictions
and 25,446 clock visits. Nearby presentations have one miss and about
0.04–0.71 ms decoding. Other full-lap spikes around 3387 and 4746 decode
1.29 million and 1.18 million texels, costing 29.17 and 25.32 ms respectively.

These are measured wall-clock sub-stages. Overall thread CPU is close to whole
presentation wall time, but that does not independently isolate GPU execution,
active driver CPU, or driver spin waits. The upload scope includes binding,
parameters, `glTexImage2D`, `glTexSubImage2D` and error query. The eviction scope
includes clock traversal, unchanged mutation flush hooks, deletion and accounting.
Nested submission drains must not be added again to the eviction scope.
`lookup_decode_wall` excludes upload and submission, but **includes decode**.

CPU decode staging is the existing fixed approximately 4.2 MB scratch buffer and
1 KB stack palette LUT. There is no application heap allocation per decoded
texture. Driver allocations happen within the measured GL operations. Decoded
RGBA is discarded after upload. The GPU cache key includes UV bounds, bank,
palette group/generation, cmode, opacity and resolution cap. The cache is bounded
by allocated texture dimensions, not just decoded pixels. GLES eviction deletes
objects rather than retaining unaccounted free storage.

At the hitch the READY queue remains bounded at two, snapshot age grows six or
seven ticks, and simulation proceeds. Short swap times do not support vblank
waiting as the primary explanation. The earlier full baseline gave 111.71 ms
CPU, 40.45 ms decode, 13.99 ms upload and 67.14 ms inclusive lookup/decode.
Run variance and differing naturally selected presentations mean those two
single traces are diagnostic examples, not an A/B performance result.

| Tick 1965 baseline operation | Measured ms | Notes |
|---|---:|---|
| RGBA tile/palette decode | 41.369 | 1,874,368 source texels, 77 misses |
| Cache eviction scope | 25.556 | 444 victims, 25,446 clock probes; includes ordered flush hooks |
| GL allocation/upload scope | 14.247 | 77 texture updates, driver time included |
| Scene preparation | 3.433 | Geometry extraction, separate from texture decode |
| GLES draw submission | 9.080 | Ordered draws after texture preparation |
| Swap call | 0.125 | Return time, not physical scanout |
| Total presentation-thread CPU | 107.407 | Overlaps/inclusive scopes must not be summed blindly |

The decode, eviction and upload wall scopes sum to about 81.17 ms, numerically
75.6% of presentation-thread CPU. That ratio is an approximate attribution,
not an exact CPU percentage: wall and CPU clocks differ, and eviction can invoke
ordered draw submission. `draw_wall` (94.874 ms) and `lookup_decode_wall`
(68.334 ms) are inclusive and are intentionally omitted from the sum.

`cpu` is `CLOCK_THREAD_CPUTIME_ID` for the presentation thread; individual
sub-stages use `CLOCK_MONOTONIC`. At tick 1965, presentation wall time is
109.847 ms versus 107.407 ms thread CPU, leaving only 2.440 ms off-CPU.
The thread is overwhelmingly busy rather than sleeping through a swap. This
does not prove all GL-call CPU is useful work: a driver could spin while
waiting for its GPU. The measured 0.125 ms swap scope is not the hitch, and
no reliable Mali GPU execution counter was available in this experiment.

The cache key compares UV bounds, bank, palette group and generation, colour
mode and cap. Hits use existing GPU storage; a miss builds an RGBA LUT and
decodes source tilemap/pixels into the fixed scratch buffer, then evicts enough
clock-policy entries to fit the incoming allocation before the GL upload.
Evictions are hardware-side resource maintenance only; neither the cache nor
the tile-run experiment changes the palette snapshot or geometry order.

## Experiments

1. **Sparse occupancy clock index:** considered and unit-tested, then rejected
   before integration. Only 25,446 visits occur at the hitch; 444 object evictions
   and decoding are the stronger candidates. No eviction-policy change retained.
2. **Immutable RGBA tile-row cache:** a fixed 1,442,816-byte cache with exact LUT
   invalidation and contiguous row copies. Sanitized/native/ARM64 row tests passed,
   but a focused handheld run increased decode to 85.519 ms and presentation CPU
   to 154.582 ms. Removed from production source. Extra row lookup, row population
   and poor reuse did not amortize on this workload; do not extrapolate that result
   to a cache of complete decoded bakes.
3. **Per-miss texture deletion batching:** the same victims, reference-bit hand,
   ordered mutation hooks and 64 MiB budget; collect selected names and call
   `glDeleteTextures` once before allocating/uploading the incoming texture.
   A bounded 65,536-name array costs 256 KiB; no GPU storage is retained and there
   are no gameplay heap allocations. `RR_TEXTURE_DELETE_BATCH` defaults OFF.
   Native frame 1965/3387/4746 pixels, geometry, state, PCM and voice digests match.
   First handheld trial reduces calls to 54 for 450 victims, but remains
   107.745 ms CPU (40.598 decode, 13.629 upload, 23.618 eviction). Five
   alternating full-lap A/B pairs reduced median delete calls from 8,845 to
   2,648 but left median maximum interval at 107.42 versus 107.39 ms,
   median 1% low at 16.48 versus 16.53 FPS, and median FPS at 33.758 versus
   33.774. The change is not retained. One candidate run had a transient
   0.80-second resync and 36,859 audio underrun samples; all others had zero
   underruns. This outlier did not drive the five-run medians.
4. **Tile-bounded RGBA decode:** the existing GLES2 byte-exact tilemap, attribute
   orientation and palette conversion are kept, but the decoder advances across
   each contiguous sampled tile in a short run. This moves tilemap/attribute
   lookup, orientation and source-stride calculation out of the per-texel body.
   It adds no cache or application allocation and does not change the GPU cache,
   uploads, UVs or draw order. It is independently gated by
   `RR_TEXTURE_TILE_RUNS`, default OFF. A 666,144-case sanitized/native test
   covers all 16 orientations, packed modes, tile edges, U wrap, skipped tiles,
   invalid map/tile cases, alpha and mixed adjacent tile attributes. Native
   fixed-cadence frame 1965/3387/4746 pixels, geometry, state, PCM and voice
   digests are identical. In one focused handheld comparison at tick 1965,
   decoded texels remain 1,874,368 and misses/uploads remain 77, while decode
   time falls 41.369 -> 23.275 ms and presentation CPU falls 107.407 ->
   89.240 ms. These isolated traces are not the pacing result; the full-lap
   alternating A/B comparison appears below.

Focused data: [benchmarks/texture-hitch-20261013-focused.csv](benchmarks/texture-hitch-20261013-focused.csv).

## Five-pair full-lap handheld comparison

Five alternating baseline/candidate pairs replayed the same human input
through tick 5639, measuring ticks 1800–5639 with original HUD and audio.
Both sides used the same threaded stage tracing; each finished with the same
deterministic endpoint JSON. The baseline and tile-run build use the same
64 MiB cache, replay, renderer, geometry optimisations and audio settings.
All ten runs had zero resyncs, discarded lag and audio underruns. Values below
are medians of five runs per variant, rather than single illustrative frames.
The individual-run numbers are in
[benchmarks/texture-hitch-20261013-ab.csv](benchmarks/texture-hitch-20261013-ab.csv).
Raw run logs, presentation/tick CSVs, RSS samples and SHA manifest remain on
the handheld at `/userdata/system/rave-racer-texture-hitch/ab-20261013-141408-4257/`.

| Metric | Baseline | Tile runs | Difference |
|---|---:|---:|---:|
| Simulation Hz | 59.904 | 59.904 | unchanged |
| Rendered FPS | 33.822 | 34.195 | +1.1% |
| 1% low FPS | 16.480 | 18.641 | +13.1% |
| 99th-percentile presented interval | 48.317 ms | 44.274 ms | -8.4% |
| Largest presented interval | 106.640 ms | 88.818 ms | -16.7% |
| Intervals above 50 ms | 16 | 12 | -4 |
| Presentation-thread CPU/presentation | 29.145 ms | 28.708 ms | -0.437 ms |
| RGBA decode/presentation | 1.065 ms | 0.618 ms | -42.0% |
| Scene preparation/presentation | 3.673 ms | 3.681 ms | unchanged |
| GLES submission/presentation | 8.539 ms | 8.560 ms | unchanged |
| GL upload/presentation | 0.594 ms | 0.599 ms | unchanged |
| Draw calls/presentation | 859.26 | 859.31 | equivalent workload |
| Texture binds/presentation | 863.78 | 863.79 | equivalent workload |
| Triangles/presentation | 1,813.52 | 1,813.48 | equivalent workload |
| Misses/uploads over window | 7,641 | 7,622 | presentation selection varies |
| Evictions over window | 8,847 | 8,838 | presentation selection varies |
| Dropped snapshots over window | 1,671 | 1,646 | -25 |
| Mean presentation age | 38.10 ms | 37.84 ms | -0.26 ms |
| Peak GPU cache allocation | 64.000 MiB | 64.000 MiB | unchanged |
| Peak process RSS | 277.77 MiB | 278.39 MiB | within run variation |
| Lowest system MemAvailable | 575.46 MiB | 574.05 MiB | within run variation |
| Peak ready queue depth | 2 | 2 | unchanged |

For the tick-1965 burst, the median worst interval was 106.64 -> 88.82 ms.
Similar repeatable bursts around 3386–3387 and 4745–4746 fell from median
77.77 -> 66.13 ms and 72.82 -> 62.13 ms, respectively. Every pair improved
the 1% low by 2.09–2.39 FPS and the largest interval by 16.96–17.82 ms.
The isolated candidate does not increase GPU storage or decoded CPU caches.
Its small RSS difference is below the variation among baseline runs.

The exact same emulated ticks and inputs are replayed, but the two presentation
threads do not necessarily choose the same subset of snapshots when rendering
at different speeds. Small differences in aggregate draw counts or cache misses
are therefore expected. Pixel, texture-event, state and audio equivalence use
separate fixed-cadence tests at the same emulated frame.

## Source changes

`RR_TEXTURE_TILE_RUNS` in `raverace/CMakeLists.txt` is OFF by default. It selects
`engine/texture_tile_runs.inc` only for the GLES2 texture bake, leaving the
desktop renderer and the original decoder as references. The validated
`playable-knulli` preset enables the flag for future handheld
builds; explicit `-DRR_TEXTURE_TILE_RUNS=OFF` restores the baseline.
`engine/tex_bake.c` adds that opt-in decoder and the stage counters; its header,
`engine/quad_gles2.*`,
`raverace/include/rr_gles2.h`, `raverace/src/rr_gles2.c` and
`raverace/src/rr_presentation.c` carry the counters into optional presentation
CSV output. The new `raverace/tools/test_texture_tile_runs.c` is the direct
decoder equivalence test. The benchmark runner and two analysis scripts in
`raverace/tools/` produce the reproducible A/B and miss-key summaries.
The rejected row cache and deletion batching are not retained in production
source. No cache budget, replacement policy, draw submission, simulator, sound
or timing code was changed.

## Added diagnostics

Thread-profile CSV appends inclusive lookup/decode, exclusive decode, GL upload,
decoded texels, eviction scope, clock probes and texture-delete calls. Existing
columns and diagnostic modes remain available. No per-sample/per-polygon output
is added to normal runs. Existing monotonic swap-return timestamps remain the
pacing basis; they do not measure physical scanout.

`ENG_TEXTURE_MISS_TRACE=/absolute/new/path.csv` explicitly enables buffered,
exclusive-created miss diagnostics with texture key, actual palette-LUT digest,
used/allocated/decoded dimensions. It is disabled when unset and must remain
unset for headline comparisons. `analyse_texture_misses.py` distinguishes first
geometry keys, previously decoded identical pixels and prior keys with changed
palettes; it cannot prove the cause of eviction from a miss-only stream.
The `ENG_` prefix is deliberate: deterministic replay startup clears `RR_*`
environment overrides. In a separate trace-enabled handheld replay there were
4,988 first-seen geometry keys (72,584,504 decoded texels), 2,617 repeats with
the same palette pixels (30,894,201 texels), and no previously seen geometry
keys with a changed palette digest in the measurement window. At tick 1965,
**75 of 77 misses were first-seen keys**; at the later bursts 3385–3389 and
4743–4749 the corresponding counts were 52/56 and 29/64. Thus a larger GPU
cache or a cache of previously decoded complete textures cannot remove most of
the first hitch. Repeats later in the lap may merit a separate bounded-cache
experiment, but this trace alone cannot distinguish capacity evictions from
cache-key changes or prove a safe prefetch point.

## Reproduction

Build both variants in separate directories from `raverace` (the preset contains
the existing H700 Buildroot toolchain and all previously validated optimisations):

```sh
cd raverace
cmake --preset playable-knulli -B /tmp/rr-texture-baseline -DRR_TEXTURE_TILE_RUNS=OFF
cmake --build /tmp/rr-texture-baseline -j4
cmake --preset playable-knulli -B /tmp/rr-texture-tile-runs -DRR_TEXTURE_TILE_RUNS=ON
cmake --build /tmp/rr-texture-tile-runs -j4
```

Copy only the experimental binaries and runner to `/userdata/system` (the
normal Ports launcher is untouched):

```sh
ssh root@192.168.1.69 'mkdir -p /userdata/system/rave-racer-texture-hitch'
scp /tmp/rr-texture-baseline/rr_gles2 root@192.168.1.69:/userdata/system/rave-racer-texture-hitch/rr_texture_baseline
scp /tmp/rr-texture-tile-runs/rr_gles2 root@192.168.1.69:/userdata/system/rave-racer-texture-hitch/rr_texture_candidate
scp tools/RaveRacer-Texture-Replay-Benchmark.sh root@192.168.1.69:/userdata/system/rave-racer-texture-hitch/benchmark.sh
ssh root@192.168.1.69 'cp /userdata/system/rave-racer-replay/results-20261013-123359-26138/benchmark-reference.json /userdata/system/rave-racer-texture-hitch/reference-5639.json'
```

Preserve normal `rr_gles2`; experimental binaries live outside Ports under
`/userdata/system/rave-racer-texture-hitch/`. Before the A/B runner, produce or
copy a verified headless endpoint JSON to `reference-5639.json` in that directory.
The runner compares every run against that reference and records binary/replay
SHA-256s. It stops/restores EmulationStation, refuses a normal active game, samples
RSS/available memory and frequencies/thermal sensors every five seconds, and
alternates variant order. Both sides use the same traced measurement mode.

```sh
ssh root@192.168.1.69 'sh /userdata/system/rave-racer-texture-hitch/benchmark.sh /userdata/system/rave-racer-replays/race-20261013-122242-25136.rec 1800 5639 5'
scp -r root@192.168.1.69:/userdata/system/rave-racer-texture-hitch/ab-YYYYMMDD-HHMMSS-PID /tmp/rr-texture-results
python3 tools/summarise_texture_replay.py /tmp/rr-texture-results 1800 5639
```

1% low is reciprocal mean of slowest ceil(1%) swap intervals, with the same
racing bounds and tracing on both sides. Simulation Hz uses completed-tick wall
timestamps, presented FPS uses presentations in that same wall interval. Snapshot
dropping changes which frames populate a performance trace; pixel equivalence
therefore uses a separate fixed-cadence test at identical emulated ticks.

## Validation and deployment

- Desktop OpenGL, native GLES2 and H700 ARM64 GLES2 builds passed. The ARM64 ELF
  uses the H700 Buildroot interpreter and EGL/GLESv2/Mali/SDL2 libraries, with
  no desktop `libGL` dependency. A sanitized native decoder test passed 666,144
  sampled row cases; the same standalone test passed on ARM64.
- Native real-ROM headless and replay regressions passed. Native fixed-cadence
  equivalent-frame captures at ticks 1965, 3387 and 4746 have identical pixels,
  ordered geometry, simulation/board/video digests and PCM/voice state. The
  baseline and tile-run builds produced the same 3,906,040-event ordered
  texture digest (`03f44680eff961c7`) through tick 1965. The native
  threaded/serial/snapshot regression also passed at 600, 1700 and 3600,
  including texture event order. ARM64 completed all 21 recorded checkpoints,
  with tick-1965 pixels and geometry identical to the baseline.
- The five-pair A/B runner checked every final replay JSON against the same
  reference and verified the executable/replay/primary hashes after testing.
  No benchmark run trapped or reported a DSP/sound fault. EmulationStation was
  restored after each isolated test.
- The previous primary executable (SHA-256 `80d36fc2c536c4931da41491aa097be60e5cbcb24efbb036d940cd91bc438e7f`)
  is at `/userdata/system/rave-racer-backups/20261013-texture-hitch/rr_gles2-before-tile-runs`.
  The new primary (SHA-256 `ed27bc04b41c402e2181a1f36c4b3876becffea6224a9499bd9a181120dbedb1`)
  is `/userdata/roms/ports/rave-gles2/rr_gles2`. Its executable `.text` is
  byte-identical to the benchmarked candidate; stripping removed debug info.
  The normal `/userdata/roms/ports/Rave Racer.sh` launcher remains unchanged,
  with `replay-mode` set to `off` and its runtime log at
  `/userdata/roms/ports/rave-gles2/rave-racer.log`. Only that one top-level
  Rave Racer launcher is present.
- A short replay through the installed executable reproduced the candidate's
  tick-1965 image exactly and EmulationStation restarted. The short forced
  fixed-cadence image check ran at 42 simulation ticks/sec and produced audio
  output underruns; it is a correctness harness, not normal gameplay. All ten
  real-time A/B replay runs had zero audio underruns. A separate untraced
  installed-path replay matched the deterministic endpoint, sustained 59.903
  simulation Hz and 34.429 rendered FPS, reported zero audio underruns,
  overflows, resyncs and discarded lag, and restored EmulationStation.

The retained decoder is a measurable improvement in presentation pacing, but
does not eliminate the cold-texture burst. Decode remains about 23 ms within
that frame; eviction and GL allocation/upload still cost about 24 and 13 ms.
Any future prefetch must prove palette/snapshot coherence and bounded work,
and should be evaluated separately. Full arcade-rate rendering is not achieved
by this change. The new binary has passed automated replay on the handheld,
but a fresh human-driven session with this exact binary has not yet been observed;
subjective pacing and audio through speakers/headphones remain for the next
interactive check.
