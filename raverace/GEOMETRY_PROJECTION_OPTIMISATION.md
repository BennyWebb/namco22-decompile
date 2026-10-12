# Reuse unchanged System 22 vertex projections

## Scope and dependency analysis

This experiment changes only the host geometry decoder. `RR_GEOMETRY_REUSE_PROJECTION`
(default **OFF**) reuses screen coordinates within one polygon; it introduces no
model cache, new storage, changed arithmetic, reordered geometry or GPU changes.
The benchmark enables both validated `RR_ORDERED_SUBMISSION` and
`RR_GEOMETRY_INDEX_ORDER`, plus the WRAM, split-reader and C352 optimisations.

The current path is:

1. The scheduler/DSP updates polygon RAM and consumes direct-polygon commands.
   Those hardware operations still run on every emulated tick. Skipped-frame
   geometry suppression is unchanged.
2. On presentations, `slave_list.c` traverses current polygon RAM. Object/view
   matrices are combined using Q15 arithmetic; translations, lighting, zoom,
   priority and clipping state come from the current display list.
3. `geo_hw_object` traverses model/chunk/packet pointers. Point ROM is immutable
   and already decoded once into native sign-extended 32-bit words. Point RAM is
   dynamic and uses the existing callback. Neither is cached by this change.
4. `quad_fixed` loads four XYZ triplets, transforms them with 64-bit sums,
   rejects/culls, reads UVs, computes priority/fog/brightness, projects the raw
   vertices, near-clips attributes, projects the clipped vertices and optionally
   guard-clips. Brightness consumes a stateful lighting index exactly once.
5. The emitted 1044-byte `geo_quad` is copied into the existing retained buffer;
   sorted 12-byte keys provide ordered indices. GLES2 then clips to the screen,
   converts attributes, expands fans, resolves textures and emits ordered VBO
   draw commands. The validated contiguous frame upload remains unchanged.

Immutable model XYZ/UV/packet fields are reread. Their transformed positions,
lighting, projection, clipping, priority, palette/fog and texture-cache state
remain dynamic. Reusing transformed records across frames would require invalidation
for all of these dependencies. No large cross-frame cache is justified here.
No per-polygon heap allocation exists in the decoder: its temporary records and
clipping arrays are on the stack. Retained geometry capacity and sort-key allocation
behaviour are unchanged. Full record initialisation/emission copies still exist;
this experiment does not assume they dominate merely because they are large.

## The targeted change

The baseline projects an ordinary visible vertex twice using the same
`proj_sat` arguments. If `q.behind == 0`, all four integer depths are positive,
therefore at least one. The near clip emits exactly the original four vertices,
in their original order, without intersections. Consequently `rv[i].sx16/sy16`
can be copied from `v[i].sx16/sy16` bit for bit.

Only those two fields are reused. The original near-clip traversal, fixed-point
UV/brightness interpolation, depth, fractional attributes, pre-projection guard
coordinates, guard clipping, texture bounds and metadata remain. Polygons with
any vertex at zero/negative depth retain the original projector. Direct polygons
are untouched. No new per-frame allocations, CPU cache or texture storage exist.

The ARM64 Release decoder shrinks from 9352 to 8936 bytes (-416 bytes). Its
stack reservation decreases by 48 bytes (the 144-byte saved-register area plus
3328 versus 3280 bytes of local storage). GCC still uses 64-bit scalar multiply/
multiply-add instructions for transforms and `__divti3` for wide projection.
Unchanged polygons bypass their second set of division calls; clipped polygons
retain them. This is source-level redundant-work removal, not ARM-specific maths.
NEON has no integer division operation that replaces the required exact wide
arithmetic. No evidence justifies intrinsics in this experiment.

## Profiling methodology and limitations

`RR_GEOMETRY_PROJECTION_PROFILE` is a separate default-OFF diagnostic option.
`GEO_PROFILE_FROM=1400 GEO_PROFILE_TO=1700` enables point-ROM/RAM access counts,
reusable-polygon counts and CPU timers around transform, raw projection and
near clipping/projection. It logs once at exit. Timed regions are disjoint but
not a complete scene decomposition: traversal, culling, lighting, guard clipping,
emission, sorting and backend work remain outside them.

Fine per-polygon clocks materially perturb short loops: one early fixed-frame
run increased scene CPU time from 3.997 to 18.466 ms/presentation. The reported
transform/raw/near times are **instrumented diagnostic costs**, not normal stage
costs, and must not be summed with inclusive scene time or used as headline
performance. A calibration reports clock-interval cost separately; counters and
changed code layout are additional overhead. Headline builds compile out both
this diagnostic and `RR_PIPELINE_AUDIT`.

To isolate arithmetic without hot-loop clocks, `GEO_PROJECTION_TRACE=FILE`
captures the actual projector arguments at emulated frame 1500. A bounded
32768-entry diagnostic-only array uses 1,310,720 bytes; overflow/write status is
reported explicitly. `bench_geometry_projection.c` replays the captured arguments
500 times with clocks outside the loop. The trace contains 18260 calls and no
overflow. On the H700, replay costs 0.983337 ms per captured frame (53.852 ns/call).
This is a warm-data microbenchmark, not an end-to-end FPS measurement or a precise
prediction of savings. The full decoder and handheld A/B comparison decide retention.

The dense fixed-presentation workload contains 122185 emitted model polygons,
114713 reusable (93.88%), 947828 raw and 977692 near projector calls before
optimisation. The candidate avoids 917704 repeated calls (47.66% of those two
projection passes). All 5,978,070 measured point accesses in this particular
window hit immutable ROM; that does not justify bypassing dynamic callbacks in
other scenes. The next implementation must preserve those callbacks.

## Files changed

| File | Purpose |
|---|---|
| `engine/geo_hw.c` | Small conditional projection reuse; optional diagnostic hooks |
| `engine/geo_projection_profile.inc` | Isolated default-OFF counters, timing, bounded trace capture |
| `raverace/CMakeLists.txt` | Two independent default-OFF options, scoped to GLES2 geometry |
| `raverace/tools/test_geometry_projection.c` | Full real-decoder record/state comparison |
| `raverace/tools/bench_geometry_projection.c` | Actual-input projector replay timing |
| `raverace/tools/RaveRacer-Projection-Benchmark.sh` | Alternating headline/coarse/detail experiment |
| `raverace/tools/RaveRacer-Projection-Correctness.sh` | Same-GPU captures, headless and audio/state comparisons |
| `raverace/tools/analyse_projection_benchmark.py` | State/cohort checks and timing/throughput summaries |
| This report | Evidence, limitations and reproducible commands |

## Reproduction

Use the existing matching H700 SDK, not Ubuntu's AArch64 compiler. In separate
build directories, build both flag values with identical settings:

```sh
for reuse in OFF ON; do
  cmake -S raverace -B /tmp/rr-projection-$reuse \
    -DCMAKE_TOOLCHAIN_FILE="$PWD/raverace/cmake/knulli-aarch64.cmake" \
    -DKNULLI_HOST="$HOME/dev/knulli-scarab/output/h700/host" \
    -DCMAKE_BUILD_TYPE=Release -DRR_GLES2=ON \
    -DRR_FAST_MEMORY_READS=ON -DRR_SPLIT_MEMORY_READER=ON \
    -DRR_C352_INACTIVE_FASTPATH=ON \
    -DRR_MEMORY_READ_COUNTERS=OFF -DRR_C352_VOICE_COUNTERS=OFF \
    -DRR_ORDERED_SUBMISSION=ON -DRR_GEOMETRY_INDEX_ORDER=ON \
    -DRR_GEOMETRY_REUSE_PROJECTION=$reuse \
    -DRR_GEOMETRY_PROJECTION_PROFILE=OFF -DRR_PIPELINE_AUDIT=OFF
  cmake --build /tmp/rr-projection-$reuse -j4
  cp /tmp/rr-projection-$reuse/rr_gles2 /tmp/rr-projection-$reuse-arm64
  # Coarse frame scopes, no per-polygon instrumentation.
  cmake -S raverace -B /tmp/rr-projection-$reuse -DRR_PIPELINE_AUDIT=ON
  cmake --build /tmp/rr-projection-$reuse -j4
  cp /tmp/rr-projection-$reuse/rr_gles2 /tmp/rr-projection-audit-$reuse-arm64
  # Detailed counts/timing; not for throughput comparisons.
  cmake -S raverace -B /tmp/rr-projection-$reuse -DRR_GEOMETRY_PROJECTION_PROFILE=ON
  cmake --build /tmp/rr-projection-$reuse -j4
  cp /tmp/rr-projection-$reuse/rr_gles2 /tmp/rr-projection-profile-$reuse-arm64
 done
scp /tmp/rr-projection-*-arm64 raverace/tools/RaveRacer-Projection-Benchmark.sh \
  raverace/tools/RaveRacer-Projection-Correctness.sh \
  root@192.168.1.69:/userdata/roms/ports/rave-gles2/
ssh root@192.168.1.69 \
  'cd /userdata/roms/ports/rave-gles2 && sh RaveRacer-Projection-Benchmark.sh'
```

The runner performs five alternating unprofiled A/B pairs, two alternating
coarse fixed-presentation pairs and separately labelled detailed runs. Every run
uses the same coin/accelerator/steering script, 64 MiB textures, frame VBO uploads,
independent timing and skipped-frame geometry ON. Full simulated startup is
included in execution but throughput measurement covers inclusive frames
1400–1700 (301 ticks). Fixed comparisons use every third tick: 100 presentations,
1401–1698. Their forced FPS is not interactive throughput. Draw/cache/VBO equality
is checked on identical presented ticks; timed independent runs may choose different
ticks and have different cache traffic. The longer 3600-frame correctness run is
not representative racing FPS because its later wall-facing period is unchanged.

Download the result directory printed by the runner, then run:

```sh
python3 raverace/tools/analyse_projection_benchmark.py /tmp/RESULT_DIRECTORY
ssh root@192.168.1.69 \
  'cd /userdata/roms/ports/rave-gles2 && sh RaveRacer-Projection-Correctness.sh'
```

The correctness runner requires the existing `rr_headless-c352-fast` reference.
Both runners use separately named experimental executables, verify the working
binary checksum and restore EmulationStation. Native builds omit the toolchain;
native capture checks use `SDL_VIDEODRIVER=offscreen MESA_GLES_VERSION_OVERRIDE=2.0`
and `tools/test_ordered_submission.py` with the two builds and headless reference.
The direct test includes the real decoder and compares its complete emitted
records, lighting index, callback count and reject statistics; compile it twice
with/without `-DRR_GEOMETRY_REUSE_PROJECTION`, then compare binary stdout.

## Results and decision

Physical RG40XX H, 2026-10-12; default `schedutil` CPU and `simple_ondemand`
GPU governors retained. Temperature observed during headline runs was 48.5°C;
frequencies were not pinned and thermal/frequency counters were not continuously
sampled. Five alternating pairs help account for that variation.

| Headline metric, dense 1400–1700 | Baseline (both prior flags ON) | Projection reuse ON |
|---|---:|---:|
| Median simulation Hz | 58.6670 | 58.5639 |
| Median rendered FPS | 25.9357 | 27.0651 |
| Rendered FPS range | 25.5328–26.5436 | 26.6343–27.2390 |
| Median scene wall ms/presentation | 3.698 | 3.213 |
| Median submission wall ms/presentation | 6.596 | 6.531 |
| Median draws/presentation | 792.9 | 792.5 |
| Median triangles/presentation | 1911.4 | 1911.3 |
| Median texture binds/presentation | 795.7 | 795.4 |
| Median resyncs | 2 | 2 |
| Median discarded lag ms | 97.181 | 101.254 |
| Median RSS KiB | 199556 | 203028 |

The ratio of FPS medians improves **4.35%**; the median paired improvement is
**4.04%**. Individual paired improvements are +4.93%, +6.00%, +0.34%, +4.04%,
+3.56%: all five are positive, with clear run-to-run variability. More frames
are presented (baseline 131–136 versus reuse 137–140), rather than simulation
becoming faster. Simulation Hz changes by -0.18%; no simulation acceleration,
reduced resyncs or full arcade speed is claimed. Scheduler policy is unchanged.
Different presentation choices explain small differences in headline geometry,
cache/driver memory and GPU traffic; they are not changed scene semantics.

Two additional alternating **coarse-only** fixed-presentation pairs disable the
geometry counters/timers entirely, retaining only `RR_PIPELINE_AUDIT`. Both sides
present the same 100 ticks. The table averages those two runs per side:

| CPU stage, ms/presentation | Baseline | Reuse | Difference |
|---|---:|---:|---:|
| Scene preparation, inclusive | 3.7087 | 3.2024 | -0.5063 (-13.65%) |
| Traversal/decode/transforms/emission, scene subset | 2.9900 | 2.4842 | -0.5057 (-16.91%) |
| Sorting, scene subset | 0.4578 | 0.4592 | +0.0014 |
| GLES backend, inclusive | 12.4972 | 12.4518 | -0.0453 |
| Submission including VBO/draw, backend subset | 6.6000 | 6.4996 | -0.1003 |
| Vertex upload, submission subset | 0.1878 | 0.1938 | +0.0060 |
| Swap | 0.4738 | 0.4789 | +0.0051 |
| Total presentation (scene + backend + swap) | 16.6797 | 16.1332 | -0.5465 (-3.28%) |

Scene savings are 0.5011 and 0.5114 ms in the two pairs; the traversal-stage
savings account for essentially all of them. Backend/submission differences
are incidental variation, not an implemented optimisation. Inclusive rows and
their subsets must **not** be added together. The microbenchmark's approximately
0.49 ms prediction is consistent with the observed geometry reduction.

Fixed frames have **identical** 800.1 draws, 1926.8 triangles and 803.4 binds per
presentation; 234 texture uploads, 122845 hits, 234 misses and 791 evictions in
the measured cohort; exactly equal per-tick VBO uploads/bytes and sort allocations.
Reported average VBO traffic is 231219.6 bytes/presentation and 1.01 uploads
(the existing texture-mutation barrier can add a transfer). No new geometry
allocation occurs. The retained 8192-polygon buffer remains 8,552,448 bytes;
texture storage remains bounded by the same 64 MiB budget. Coarse fixed-run RSS
is 200652/200740 KiB baseline versus 200584/200712 KiB reuse. The +3472 KiB
headline median difference is not a new application buffer: presentation/cache/
driver allocation patterns vary, and fixed-run RSS does not reproduce an increase.

The original historical no-ordered/no-index baseline was 23.6524 FPS. The new
combined result is approximately **14.43% higher**, but that is a historical,
non-contemporaneous comparison. Only the five current A/B pairs isolate this
new change. No independent handheld performance factorial was run with prior
flags OFF; native correctness checks cover that configuration separately.

### Correctness and provenance

- Native desktop renderer builds; native headless regression checks pass.
- Native same-GPU captures at 600, 1700 and 3600 match byte for byte, alongside
  complete headless JSON, PCM/voice, board/video and geometry digests.
- Native coupled-mode frame1700 capture/state/PCM/voice checks match.
- The direct decoder test compares 10000 cases including all near-plane masks,
  zero/negative depths, random Q15 transforms, signed zooms/shifts, culling,
  guard clipping/saturation, dynamic callbacks, UVs, priority and lighting-index
  consumption. Full emitted records and final callback/reject statistics match.
  ASan/UBSan also pass (LeakSanitizer disabled because the sandbox uses ptrace;
  the decoder test has no heap allocations).
- ARM64 ELF uses the matching SDK and Linux 4.9/glibc interpreter; dependencies
  are SDL2, GLESv2, EGL, Mali, libm/libc/loader, with no desktop libGL.
- All 16 original benchmark exits, deterministic state/board digests and same-tick
  geometry/cache/VBO comparisons pass; four subsequent coarse-only runs pass
  the same checks. Physical handheld captures at 600/1700/3600 also match byte for byte with
  matching PCM/voice/board/video/geometry digests and headless state. Final ARM64
  decoder outputs match each other and native outputs. Native captures also
  pass with both prior geometry/submission flags OFF, establishing independence.
- Final production ARM64 `.text` matches the actually benchmarked candidate,
  despite subsequently isolating diagnostic code into an include file.
- The working handheld SHA-256 remains
  `79f7abe165562b0dbf6e653a059fb0e3300cc936965ed59b45a9d6ae87ab242f`.
  EmulationStation is restored (verified process PID22206). The final test SSH
  command returned 1 only because Knulli's cleanup `start` reported that the
  frontend was already running; every test/capture comparison completed.
  The checked-in runners now check existing frontend state before restarting
  and preserve the test status, avoiding that misleading cleanup failure.

Raw data is preserved locally under `/tmp/rr-projection-results/` and on the
device under `/userdata/roms/ports/rave-gles2/`:

- `projection-benchmark-20261012-213147-21637`: ten headline, four fixed and two
  detail runs. Its initial fixed builds still contained disabled-by-environment
  geometry instrumentation; their extra conditional checks added about 0.29 ms
  to scene time. Those rows are superseded for normal CPU attribution by:
- `projection-coarse-20261012-213958-22020`: four coarse-only fixed runs, with
  `RR_GEOMETRY_PROJECTION_PROFILE=OFF`, used in the CPU table above.
- `projection-results`: initial counts, frame1500 trace and decoder comparisons.
- `projection-correctness-20261012-214201-22124`: hardware PPMs, full headless and
  GLES2 JSON, PCM/voice/board/video/geometry digests for all three frames.

`analyse_projection_benchmark.py MAIN_DIRECTORY COARSE_DIRECTORY` validates and
summarises both directories. The checked-in benchmark runner now uses separate
`-audit-` binaries for its fixed runs so the extra collection is unnecessary on
future repetitions. Fine timers remain deliberately labelled and excluded from
headline and coarse results.

### Decision and next opportunity

Retain `RR_GEOMETRY_REUSE_PROJECTION` **disabled by default** for review: repeatable
FPS and scene-CPU gains support this small change. It removes redundant work,
requires no numerical approximation and adds no new production storage.

Remaining list/decode/transform work is approximately 2.48 ms/presentation, but
submission is approximately 6.50 ms and texture lookup/decode plus upload about
3.5 ms. Submission is now the largest measured host rendering stage. Investigate
its driver/application split and state-change cost before another geometry
rewrite or cache. ROM packet decoding and Q15 transforms are possible later
candidates, but their isolated normal costs have not been established by the
perturbing per-polygon timers. Do not turn those numbers into a NEON promise.

Even presentation alone costs about 16.13 ms of thread CPU, before roughly
9.5 ms of simulation per tick on the same thread. This change improves rendering
throughput at almost-full simulation speed; it does not provide a demonstrated
route to sustained 59.904 rendered FPS. No further optimisation is implemented.

