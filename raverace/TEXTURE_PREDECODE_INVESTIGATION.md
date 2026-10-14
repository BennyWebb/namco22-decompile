# Static texture preparation investigation — 2026-10-14

## Decision

**Preparing complete ROM-derived pen bakes before gameplay improves rare frame
pacing, with essentially unchanged average FPS.** The final five-pair test used
the installed multicore executable as baseline. Every pair improved 1% low and
worst interval. It did not eliminate the first-use hitch: GPU allocation/upload,
eviction and ordinary drawing remain substantial.

The experiment stays independently switchable and **OFF by default**. No
permanent cache learner/loader, deployment, extra Ports launcher or commit was
made. The measured bake cache is a controlled replay-derived worklist, not a
production preload sequence. A production implementation must learn or derive
keys generically and retain ordinary decoding for absent resources.

[Full measurements](benchmarks/texture-predecode-20261014-summary.json) preserve
individual runs, environment observations and supplemental probes.

## 1. Reliable deployed baseline and the 34.48 / 41.26 discrepancy

Five alternating pairs compared the installed multicore executable against the
preserved newer feature-OFF batching baseline. Replay, ticks 1800–5639,
independent timing, geometry skipping, original HUD, audio ON, 64 MiB GPU cache,
frame VBO, thread-profile/replay-timing and one-second `/proc` monitoring matched.
Both use parallel preparation, tile-run decoding, ordered submission, index
order, projection reuse, fast/split memory reads and the C352 inactive fast path.
GLES submission profiling, GPU query/reference diagnostics and batching were OFF.
Both use the H700 toolchain and the same optimisation settings.

| Median of five runs | Installed multicore | Newer unbatched baseline |
|---|---:|---:|
| Racing FPS | 41.090 | 40.953 |
| 1% low FPS | 19.760 | 20.184 |
| p99 interval | 39.730 ms | 39.503 ms |
| Median per-run worst interval | 100.871 ms | 99.704 ms |
| Simulation Hz | 59.903 | 59.888 |
| Presentation CPU/image | 22.646 ms | 23.050 ms |
| Mean swap/image, median across runs | 0.355 ms | 0.361 ms |

The historical 34.48 FPS **does not reproduce as a build regression**. The older
41.26 and later 34.48 runs had nearly equal geometry/HUD preparation costs;
representative mean swap scopes were 0.352 versus 4.041 ms. The historical
batching tests had hundreds of MiB of captures/debug binaries in RAM-backed
`/tmp`; today it began nearly empty. Two alternating pairs adding 256 MiB to
`/tmp` did not reproduce the slowdown: unpressured 41.356/41.387 FPS, pressured
41.387/41.419 FPS, swap 0.337–0.365 ms, zero underruns/resyncs. The temporary
pressure file was removed. Simple RAM occupancy is insufficient to explain it.

**The initiating historical environmental/driver cause remains unresolved.**
Aggregate logs cannot reconstruct GPU scheduling, memory layout or short
frequency changes. The current reproducible deployed racing baseline is about
41 FPS, not 34.48 FPS. No governor, rendering policy or deployed executable was
changed to obtain it. CPU frequency during the final comparison sampled 1512 MHz;
CPU temperatures ranged roughly 53–69.5 °C. GPU frequency was reported as
240 MHz; no active GPU utilisation claim is made.

### Audio anomalies

The old first-pair baseline's 80,229 underrun samples accompanied simulation
pauses of 323.3 ms after tick 1859 and 1467.4 ms after tick 4151. Presentation
gaps occur at the same times **outside measured renderer work**. Missing audio
is about 1.67 seconds at 48 kHz, consistent with exhausting the audio buffer
during these pauses. The initiating kernel/scheduler/I/O cause is unresolved;
the counts do not establish a texture-decoder or batching defect.

Fresh reconciliation also found a 763.8 ms simulation pause after tick 1139 in
the newer baseline's first run, before racing measurement. In the final bake
comparison, baseline run 1 had 45,933 underrun samples and a 1015.9 ms pause
after tick 1679, again before tick 1800. All other nine final runs reported zero
underruns. Counters cover the entire process; FPS/pacing use the racing window.
Anomalous runs remain included in the five-run medians. All endpoint states
match. These coarse traces identify starvation during pauses, not their cause.

## 2. Actual texture dependencies

`eng_load_texture_roms` loads 16 MiB CG pixels and 2.5 MiB CCR map/attributes.
These are uncompressed ROM bytes. The current Rave Racer game table leaves
`tex_fixup` disabled; `eng_texture_tilemap_sys22_fixup` is conditional, not applied
in this configuration. No source-selection policy was changed. The old pipeline
note was corrected to describe this conditional accurately.

Polygon source buffers are immutable after initialisation. Gameplay does not
write these CG/CCR buffers. Geometry supplies UV bounds, bank, cmode and projected
extent. Point RAM/model selection can change those requests; it does not mutate
ROM texture pixels. Actual bake caps, per-axis integer steps and guard dimensions
remain selected by the ordinary renderer. Allocation dimensions determine U/V
scales. Sticky preference for sharper resident GPU tiers remains unchanged.

Palette RGB is runtime planar RAM supplied by the selected immutable snapshot.
Per-group generation changes invalidate GPU bakes. Opacity affects alpha.
Consequently **final RGBA generally cannot be assumed static from ROM identity**.
The original HUD/text/shadow/fade/gamma path also depends on snapshot state and
is excluded from preparation. Its dynamic updates remain intact.

A GPU miss currently converts ROM pens through a live 256-entry RGBA LUT into
the ~4.2 MB scratch buffer, executes original eviction/mutation barriers, and
allocates/uploads. RGBA is discarded after upload. Cache eviction therefore
repeats decoding; palette changes require new colour conversion; a different cap
requires a different sampled raster. The retained tile-run decoder already moves
map/attribute/orientation setup out of the per-texel loop.

The preserved trace found 75 of 77 misses at the tick-1965 burst were first uses.
A cache of *previous* bakes cannot remove that first burst without preparation
beforehand. Unique traced RGBA allocations total 438.57 MiB across the lap;
this is not an instantaneous working-set requirement. The 64 MiB GPU budget
cannot retain the full lap. Final baseline has approximately 7,661 racing misses,
5,007 first-use resource keys and 8,882 evictions. More GPU capacity cannot remove
never-uploaded first uses. CPU preparation does not remove required GPU misses.

## 3. Controlled CPU bake preparation

`RR_TEXTURE_PREDECODE_EXPERIMENT` defaults OFF. With the explicit
`ENG_TEXTURE_PREDECODE_MANIFEST`, startup prepares pen-index rasters after ROM
loading and before emulation starts. Keys comprise UV rectangle, bank, cmode,
cap, decoded dimensions and tile-address mode. They are generated from the ROMs
loaded in this process, not from imported palettes/pixels. Data/entry limits are
128 MiB / 16,384 entries. The worklist is generated by
`tools/make_texture_predecode_manifest.py` from preserved miss diagnostics.

The table contains 11,348 keys, 114,968,100 pen bytes (109.64 MiB), 766,560 bytes
of table/entry metadata plus allocator overhead. Only an **ordinary GPU miss**
consults it, after existing cache/tier selection. Current palette/opacity creates
RGBA on use. Missing keys, unsupported sampler modes or incompatible source mode
use the existing decoder. No GPU pre-insertions, cap overrides, altered scales,
emulated state writes or early palette application occur.

### Exact correctness

Fixed presentation cadence through tick 6006 compares 14 selected HUD-composed
frames plus final capture, including dense geometry, loading at 1965,
transparency, dynamic HUD/palettes and later bursts at 3387/4746. All captured
bytes match. Ordered geometry, simulation/board state, PCM/voice and texture
request/hit/miss/delete/upload digests match:

- 12,809,029 texture events: `2a6578ef363d13f3`.
- Board state at 6006: `f9154a83`.
- PCM: `0c24e8fca4b6400e`; voices: `b76f90445ba87e18`.
- 8,555,269 audio samples; 96,096 emulated intervals.
- Optional live-reference check verified **136,188,011 RGBA texels** with zero
  byte differences, 12,499 prepared-bake uses and 78 ordinary fallbacks through
  tick 6006. This check is OFF in performance runs.

### Correcting the experimental baseline

The initial same-binary five-pair experiment exposed a measurement distortion:
adding the row-reference verification call caused GCC to stop inlining the cold
tile-run decoder. Its first-burst decode increased from about 24 to 31 ms.
That initial comparison is diagnostic evidence, not the headline result.
The experimental flag now forces that existing routine inline; flag-OFF code is
unchanged. Disassembly confirms no out-of-line copy. Corrected captures,
selected-resource miss stream and PCM/voice/state matched the preserved reference
through tick 2001, with live byte verification. The **final five pairs use the
installed executable directly as baseline**, avoiding this distortion.

### Final five alternating first-lap pairs

Original HUD/audio ON, ticks 1800–5639, independent simulation, skipped geometry,
frame VBO, GPU budget 64 MiB, batching/GPU preloading/reference checks OFF.
Both sides have matching thread/replay timing and buffered miss diagnostics.
Every run matches the deterministic reference JSON. Independent runs naturally
select different snapshots/cache histories: that is audited through miss keys,
palette digests, caps and dimensions, not mistaken for a shader discrepancy.
Per-draw data equivalence and controlled equivalent-frame pixels are exact.

| Median of five runs | Installed cold | Prepared pen bakes |
|---|---:|---:|
| Rendered FPS | 41.278 | 41.122 (-0.38%) |
| 1% low FPS | 19.853 | 23.142 (+16.6%) |
| p99 presentation interval | 40.313 ms | 37.287 ms |
| Worst interval, median per-run maximum | 104.110 ms | 71.758 ms (-31.1%) |
| Simulation Hz | 59.904 | 59.904 |
| Decode/colour conversion per image | 0.579 ms | 0.278 ms |
| Allocation/upload scope per image | 0.604 ms | 0.582 ms |
| First-use GPU resource keys | 5,007 | 5,016 |
| GPU misses/uploads | 7,661 | 7,652 |
| GPU evictions | 8,882 | 8,874 |
| Median / p95 software image age | 49.413 / 66.507 ms | 49.834 / 64.641 ms |
| Launch to first emulated tick | 0.648 s | 4.229 s |
| Peak RSS | 294.984 MiB | 404.660 MiB |
| Lowest MemAvailable, median across runs | 571.828 MiB | 460.637 MiB |
| Active texture allocation peak | 64 MiB | 64 MiB |
| Additional shader work / GPU texture storage | — | none |
| Audio underruns, whole-process median | 0 | 0 |

All five pairs improve 1% low and largest interval. Per-run FPS:
40.616/41.449/41.340/41.278/40.669 versus
40.815/41.122/40.934/41.792/41.824. Per-run largest interval:
101.903/99.199/104.174/104.110/108.779 ms versus
72.883/68.416/71.758/69.234/80.800 ms. The exceptional baseline audio count is
45,933 before the measured window; other nine runs are zero.

| Median largest decode burst in window | Cold interval / decode | Prepared interval / decode |
|---|---:|---:|
| 1965–1975 | 104.110 / 25.245 ms | 71.758 / 11.267 ms |
| 3385–3395 | 62.415 / 19.538 ms | 50.952 / 8.219 ms |
| 4743–4753 | 57.056 / 16.590 ms | 51.599 / 8.344 ms |

At the first burst both medians have 77 misses and 1,827,912 decoded samples.
Prepared first-burst upload and eviction scopes still measure about 14.764 and
25.637 ms. These are wall scopes, including driver work/waits and, for eviction,
ordered submission drains; do not add them as exclusive CPU/GPU utilisation.
Changes in upload/eviction time are observations, not proof that CPU decoding
alone caused every millisecond of total improvement.

## 4. Independent GPU preloading probe

A separate bounded diagnostic captures up to 8 MiB / 128 RGBA resources from the
loading burst and uploads them privately at startup. They never enter the active
cache before normal use, and do not influence tier selection. On an ordinary
miss the full live RGBA raster is still decoded and compared **byte for byte**.
Only an identical unused object is adopted. Changed palette/data uses the
ordinary required upload. Existing cache accounting/eviction/order is preserved;
private storage is counted separately, with an explicit additional bound.

80 objects total 7,841,616 bytes (7.48 MiB). Fixed-cadence pixels, selected-resource
miss CSV, geometry and PCM/voice/board state match. Inverting one byte in every
object rejects all 80: zero hits, 85 content-mismatch checks, ordinary upload
fallback, identical output. Unused private objects are deleted on shutdown.

One exploratory first-lap pair, CPU predecode OFF:

| Metric | Installed | GPU preloaded |
|---|---:|---:|
| Racing FPS | 41.293 | 41.293 |
| 1% low | 20.058 | 20.131 |
| p99 | 39.844 ms | 40.580 ms |
| Worst interval | 104.355 ms | 89.366 ms |
| First-burst presentation CPU | 101.010 ms | 88.881 ms |
| First-burst upload scope | 23.548 ms | 1.348 ms |
| First-burst eviction scope | 33.187 ms | 32.569 ms |

All 80 objects were used, no mismatches, zero underruns/resyncs. Startup preload
was 0.040 s with the recently written file in OS cache. Extra CPU reference data
and initial private GPU storage are 7.48 MiB each; active-plus-private texture
peak is approximately 71.48 MiB, excluding existing framebuffer/VBO/driver data.
Live pixel comparison and ordinary decoding remain foreground costs. This single
pair is **not a repeatable performance claim or retained production optimisation**.
No GPU shader code changed. Palette/source/decoder validation could make a future
coherent preload cheaper, but that optimisation is not pursued here.

## 5. Complete-ROM preparation without a worklist

A separate generic representation prepares all referenced oriented raw-pen tiles
and a direct map index. It has no replay/model/palette dependency. The current
actual policy yields 65,747 distinct tile/orientation combinations, including
normal invalid-reference handling. Data is 16,831,232 pen bytes + 4,194,304 index
bytes = **20.05 MiB**, with about 2.06 MiB temporary preparation data. It is bounded
at 131,072 combinations (36 MiB resident maximum). Current preparation takes
0.226 s. A raw-reference check verifies all 16,831,232 pens; live checking verifies
36,506,333 RGBA texels and equivalent frames/resources/PCM/state through tick 2001.

The first compact prototype incorrectly assumed the conditional Sys22 fixup and
16-bit IDs. It exceeded its ID bound and safely fell back; its inactive probe is
excluded. The corrected 32-bit representation preserves the current game policy.

One activated exploratory pair: FPS 41.420→41.481, 1% low 20.384→21.962,
p99 39.985→37.764 ms, worst 100.636→96.210 ms, zero underruns/resyncs.
First-burst decode 24.687→18.601 ms, presentation CPU 99.159→93.367 ms.
It moves orientation/address work out of gameplay, but sampled raster creation
and live palette conversion remain. Its weaker hitch result does not justify
another optimisation round here. It stays a default-OFF diagnostic, without a
five-pair performance/deployment claim.

## 6. Persistent cache feasibility

A diagnostic sample of the complete pen-bake dataset was generated offline,
without importing it into the renderer. Its header includes a SHA-256 identity
of actual CG/CCR source bytes plus tile-address mode, decoder version, entry
count and byte limit. Entries have canonical little-endian rectangle/bank/cmode/
cap/dimension keys and payload CRCs. Palette RGB/generation are excluded because
pens are palette-independent. This is a format/I/O feasibility sample, **not an
implemented production loader or a validated substitute for live rendering**.

115,422,099 bytes including metadata compress to 35,133,032 bytes (33.5 MiB),
using gzip level 1. Three cold-file/warm-file handheld pairs read and inflated
it to `/dev/null`: medians **2.227 / 2.115 s**, versus roughly 3.55 s generating
pens from already loaded ROMs. Cold trials use a per-file `POSIX_FADV_DONTNEED`
hint; no global cache drop. These timings exclude source/payload validation,
allocating/indexing retained records and a complete runtime import. Hence the
roughly 1.3 s potential startup saving is an upper bound. RAM requirements are
unchanged by compression after loading.

A reusable cache is feasible with these requirements:

1. Learn keys from ordinary gameplay or derive immutable model descriptors;
   correctness must not depend on the recorded first lap. Missing entries use
   normal decoding and can be learned for a later launch.
2. Key the namespace by source ROM hashes, current map/fixup/address mode and
   decoder/sampling/guard version. Validate all bounds and payload integrity
   before use; incompatible/corrupt files fall back safely.
3. Keep UV rectangle, bank, cmode, cap and decoded dimensions in each pen key.
   Consult it only after normal GPU resource selection. Recompute scales and
   apply the live palette/opacity. Preserve every required ordered GPU update.
4. Bound CPU RAM at the configured data/entry limit. Retain the 64 MiB active
   GPU policy independently. A private GPU preload needs its own accounted
   budget and exact palette/source validation.
5. Write a bounded consolidated file atomically after gameplay when useful new
   keys were learned, never per frame/miss. Reuse unchanged files; avoid repeated
   large SD writes. Treat new ROM/decoder versions as separate invalidated data.

The controlled experiment establishes the pacing opportunity. It does not yet
justify shipping the large cache or a permanent learner/loader: about 110 MiB
RAM, a 3.6 s extra load, residual ~72 ms hitch and no average-FPS gain are material
tradeoffs. Disk persistence principally helps startup/discovery; once the same
pen data is resident it does not further improve racing. **Stop here with the
prototypes disabled.** A future implementation decision can use these measured
bounds and generic keys; no fixed replay sequence should enter production.

## Artifacts and reproduction

Host `/tmp/rr-predecode/` preserves builds, all initial/final timing data,
correctness captures, exploratory probes and disk samples. Device root
`/userdata/system/rave-racer-predecode-20261014/` contains separate binaries and
runners, with dated result directories. Important final runs:

- Baseline reconciliation: `/userdata/system/rave-racer-multicore/ab-20261014-082742-3923`.
- Exact complete replay: `correctness-20261014-084430-8028`.
- Corrected candidate validation: `final-validation-20261014-092646-29320`.
- Final five pairs: `final-ab-20261014-092916-29756`.
- GPU coherence/corrupt fallback: `gpu-correctness-20261014-091842-28548`.
- GPU probe: `gpu-probe-20261014-100653-11567`.
- Complete-ROM validation/probe: `oriented32-validation-20261014-101211-19367`,
  `oriented32-probe-20261014-101556-19903`.

Build from `raverace` with `playable-knulli`, `RR_PARALLEL_FRAME_PREP=ON`,
`RR_TEXTURE_PREDECODE_EXPERIMENT=ON`, `RR_GLES2_MULTITEXTURE_BATCH=OFF`,
`RR_GLES_SUBMISSION_PROFILE=OFF`. The shipped preset retains its default-OFF
experiment setting. `RaveRacer-Texture-Predecode-Benchmark.sh` reproduces five
pairs; it requires a prepared manifest, separate candidate and verified endpoint.
The existing multicore analyser reports racing metrics; its sysfs parser now
skips a missing numeric frequency sample rather than aborting an entire report.

The installed executable remains SHA-256
`77536f56ad17de9cf8772175c73442d9cb246d87aeb6fbf6c8d7f07c3d6ab53e`;
pre-parallel backup remains
`ed27bc04b41c402e2181a1f36c4b3876becffea6224a9499bd9a181120dbedb1`.
Only `/userdata/roms/ports/Rave Racer.sh` is active. EmulationStation is restored
by every runner and checked again after testing. Existing shader-equivalence
captures/reproduction and previous benchmarks are preserved.
