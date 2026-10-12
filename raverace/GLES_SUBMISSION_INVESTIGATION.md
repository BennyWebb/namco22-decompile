# GLES2 draw-submission investigation

## Decision

The dense-opening bottleneck is principally draw API/driver execution, not
per-draw vertex attribute configuration or repeated VBO overwrites. A static
sequential index-buffer experiment made submission **slower** and is removed
from active source. Optional profiling, correctness traces and reproduction tools
remain; no new production optimisation is retained and no defaults change.

All measurements enable the three validated flags (`RR_ORDERED_SUBMISSION`,
`RR_GEOMETRY_INDEX_ORDER`, `RR_GEOMETRY_REUSE_PROJECTION`) and the validated WRAM,
stackless-reader and C352 optimisations. Independent timing, skipped geometry,
64 MiB texture budget and frame VBO streaming are unchanged.

## Architecture and ordering constraints

`eng_gles2_draw` consumes the existing ordered scene indices, screen-clips,
converts colour/UV/inverse-depth attributes and expands triangle fans into the
retained 60000-vertex arena. Each vertex is 40 bytes. Ordered commands hold a
texture, contiguous vertex range and scissor rectangle. Existing ordered
submission merges only adjacent compatible ranges. Texture mutation/deletion
hooks drain the ordered prefix before changing texels used by queued commands.
That barrier must remain, even if a different batch is still under construction.

A chunk uses one `glBufferData` to re-specify the VBO store. Submission walks its
commands in original order, updates scissor when necessary, binds the texture
and issues a draw. Buffer offsets select already uploaded vertices. Texture
barriers may create extra chunks; frame-based streaming is already effective.
There is no CPU index/vertex rebuild at each draw.

- Attributes: three `glVertexAttribPointer` and three enables **once per
  presentation**, not per draw. Shader, matrix, depth/blend, viewport and clear
  state are also established per presentation. Retaining them across frames
  saves little and requires ownership guarantees against SDL/texture work.
- Texture and scissor changes: about 800 texture changes per presentation, but
  few scissor changes. Distinct textures prevent further ordinary adjacent
  coalescing. Prior no-rebind tests were ineffective; do not repeat that as a
  proposed major optimisation.
- Queries: `glGetError` at chunk upload/submission/end and after texture uploads.
  Shader, capability and uniform queries happen at initialisation. No hot-loop
  `glGet*` configuration query exists. Existing checks are retained.
- Synchronisation: no normal-path `glFinish`, forced GPU result read or framebuffer
  readback. Screenshot `glReadPixels` is test-only. SDL swap occurs once per
  presentation, outside submission. Independent mode requests swap interval zero.
- Rendering semantics: priority is encoded in depth; alpha blending and original
  painter order still constrain primitives. Different materials cannot be globally
  texture-sorted. Removing a texture-mutation barrier can change queued draws.
  Changing depth or arbitrary grouping is not an acceptable optimisation.

## Low-overhead attribution

`RR_GLES_SUBMISSION_PROFILE` defaults OFF. It compiles sparse operation timers
and optional ordered texture-event hashing; headline builds compile out both.
The rejected optimisation is archived only in the inactive patch; applying it
requires an explicit source change, and no indexed option remains active.
`GLES_PROFILE_FROM=1400 GLES_PROFILE_TO=1700` selects the emulated range. Setup,
attributes, VBO upload and error queries are infrequent and timed directly;
state/draw calls are sampled at probability 1/32 using a deterministic PRNG.
Samples are bounded at 8192/category/clock (786432 bytes of diagnostic sample
arrays). CPU and wall use separate randomly chosen cohorts, with an empty interval
paired immediately before each operation. This avoids putting a CPU-clock syscall
inside a measured wall interval and calibrates at the current frequency. Raw and
adjusted means, median/p95, counts and live calibration are reported once at exit.
Frame-command construction aggregates the existing `flush` wall timers, adding
no clock reads; it is outside the inclusive CPU command-submission scope.

Fine CPU timing has significant fixed overhead relative to a 1–7 microsecond
call. Empty CPU intervals were approximately 2.6–2.8 us, while a monotonic call
cost about 115 ns. A calibration taken only after process cleanup varied up to 4.4 us as CPU load/
frequency changed; those provisional adjusted figures are superseded by the live
paired method. Calibration-adjusted estimates remain approximate, not exact
exclusive CPU accounting. Very cheap query estimates fell below calibration
resolution; negative adjusted values mean **unresolved**, not negative/free work.
The initial fixed-stride pilot may have sampling bias; the final diagnostic uses
pseudo-random selection and separately calibrated clock cohorts. Coarse CPU scopes and unprofiled
A/B pairs are the decisive measurements. No sample timing is enabled in them.

The ordered `GLES_TEXTURE_TRACE=1` digest is correctness-only. It hashes canonical
32-bit fields for texture requests (full key and palette generation), hit/miss
results, requested/actual bindings, budget deletions and image/subimage uploads.
It includes emulated frame/order and GPU texture IDs; compare on the **same GPU**.
It hashes metadata rather than ROM/decoded pixel payload. Pixel captures and
unchanged decoding separately establish image equivalence. No per-event logging,
shadow buffer or per-frame allocation is introduced.

## Physical benchmark results

RG40XX H, 2026-10-12. Five alternating unprofiled A/B pairs, plus two alternating
fixed-presentation CPU pairs. Headline measurements cover inclusive ticks
1400–1700 (301). Fixed comparisons present the same 100 ticks, 1401–1698.
Whole startup is simulated identically but excluded from headline rates.

| Metric | Existing glDrawArrays | Sequential glDrawElements experiment |
|---|---:|---:|
| Median rendered FPS | 26.7005 | 24.4140 |
| FPS range | 26.5116–26.8553 | 24.1306–24.8366 |
| Median simulation Hz | 58.5757 | 58.7890 |
| Median submission wall ms/presentation | 6.576 | 8.002 |
| Median draws/presentation | 793.3 | 792.7 |
| Median triangles/presentation | 1912.5 | 1911.1 |
| Median texture bindings/presentation | 796.0 | 795.6 |
| Median resyncs | 2 | 2 |
| Median discarded lag ms | 99.318 | 102.418 |
| Median RSS KiB | 203108 | 204724 |

All five FPS pairs lose: -8.98%, -7.86%, -9.03%, -8.94%, -7.52%; median paired
change **-8.94%**. Fewer renders give simulation slightly more budget, not improved
simulation execution. Geometry/cache differences in these rate runs reflect
presentation selection, not changed emulated inputs or rendering semantics.

Coarse CPU results below average two fixed-frame runs per side. Inclusive stages
and their subsets must not be added together:

| CPU stage, ms/presentation | Existing | Indexed experiment |
|---|---:|---:|
| Scene | 3.2248 | 3.2419 |
| Backend inclusive | 12.4883 | 14.0453 |
| Submission inclusive, backend subset | 6.5501 | 8.1113 |
| VBO upload, submission subset | 0.1878 | 0.1861 |
| Swap | 0.4700 | 0.4426 |
| Total presentation (scene + backend + swap) | 16.1831 | 17.7297 |

The existing all-call monotonic draw timing averages 5.234 ms versus 6.769 ms
indexed. Those fields include small call-site profiling overhead. Sparse initial
CPU estimates attribute roughly 5.2 versus 7.1 ms to draw APIs and 0.7 ms to
state changes. Driver internals are opaque; this does **not** prove that the
indexed regression is a particular bounds scan or GPU-cache algorithm.

Fixed cohorts have identical 800.1 draws, 1926.8 triangles and 803.4 binds per
presentation, 231219.6 VBO bytes and 1.01 transfers (one existing mutation barrier).
Texture totals match: 122845 hits, 234 misses/uploads, 791 evictions; GPU texture
storage/budget is unchanged. Indexing did not deduplicate vertices or save VBO
traffic. It added only a once-uploaded 120000-byte GPU index buffer and a temporary
120000-byte CPU allocation freed at startup, but fixed RSS also rose ~2.3 MiB,
indicating driver-side allocation effects beyond nominal buffer accounting.

The current production baseline remains approximately 12.89% above historical
23.6524 FPS. That is **not a fresh controlled comparison**: the last task measured
27.0651 FPS and this task 26.7005 under normal frequency/load variation. Production
ARM64 machine-code comparison, rather than that small rate difference, verifies
that the rejected experiment has not changed the baseline.

## Submission cost interpretation

The 6.55 ms is actual main-thread CPU for the submission scope, including client
Mali code, application loop work and instrumentation, excluding GPU worker time.
It is not just application preparation and is not GPU elapsed time.

| Existing-path operation | Approximate ms/presentation | Basis |
|---|---:|---|
| Draw calls | 5.1–5.3 | Existing all-call wall timers, sparse adjusted CPU estimate |
| Texture/scissor handling | ~0.7–1.1 | Sparse estimates vary with instrumentation; includes ownership checks |
| VBO upload | 0.188 | Coarse CPU scope |
| Application loop, timing, counters, residual | Less than roughly 0.6 | Submission residual; not independently exact |
| Attribute configuration | ~0.004 | One setup group per presentation, outside submission |
| General GL setup/clear | ~0.18 | One setup group per presentation, outside submission |
| Error query calls | Below fine CPU resolution | Very cheap; preserve correctness checks |
| Swap/EGL presentation | 0.470 | Separate coarse CPU scope |

These estimates are not an exact additive decomposition. The existing two
monotonic clocks per draw cost approximately 0.18 ms CPU in aggregate. Removing
that instrumentation cannot remove the ~5.2 ms API cost; it is a small ceiling,
not a major architectural optimisation. VBO renaming/streaming is already working;
indexing cannot fix a VBO stall that is no longer the main cost.

Submission wall and CPU costs are close. The sampled pilot measured 7.048 ms
wall versus 7.003 ms CPU, and indexed 8.629 versus 8.586. Thus large sleeping waits
are not evident. CPU-consuming driver spin waits remain possible and cannot be
excluded without reliable driver/kernel traces. No unconditional synchronisation
was introduced to answer that question. The earlier architecture audit's low
resolution sensitivity and nonblocking GPU spans support a CPU/driver bottleneck,
but do not substitute for reliable GPU utilisation counters.

## Final paired diagnostic and overhead

The final `paired-sparse` baseline run uses live paired calibration and independent
CPU/wall cohorts, superseding the provisional initial sampler for operation
estimates. It presents the same 100 ticks and matches the headline state:

| Operation | Estimated CPU ms/presentation | Estimated wall ms/presentation |
|---|---:|---:|
| GL setup/clear (outside submit) | 0.1377 | 0.1358 |
| Attributes (outside submit) | 0.0038 | 0.0041 |
| Texture/scissor state | 0.8494 | 1.0946 |
| Draw API | 5.8952 | 5.4799 |
| VBO upload | 0.1899 | 0.1811 |
| Error queries | 0.00094 | 0.00164 |

These are sampled estimates from separately selected calls, not exact additive
CPU accounting: do not equate their sums with a coarse scope or treat a CPU/wall
difference between cohorts as a wait measurement. Draw samples: 1293 CPU and
1364 wall; state: 1340 CPU and 1317 wall. The profiler slightly changes code and
cache behaviour. Existing all-call draw wall timing in the same instrumented run
is 5.472 ms, corroborating the sparse wall estimate of 5.480 ms. The uninstrumented
coarse cohort remains the reference for the normal path (5.234 ms draw wall).

Command construction aggregates 80346 existing `flush` intervals: **0.0798 ms**
per presentation. No additional hot-loop clocks are needed for that field. This
pure host work is outside the inclusive CPU command-submission scope; the existing
`[PROFILE] submission` wall statistic includes it and is not an identical scope.
Sparse fine timers report CPU medians/p95 in the logs; the coarse totals establish
which work is large enough to optimise.

Final sampled submission CPU is 6.9855 ms versus 6.5501 ms coarse (+6.65%);
total presentation CPU is 16.5091 versus 16.1831 ms (+2.01%). This is an indicative
instrumentation-overhead comparison, not five repeated throughput pairs. It also
includes nonblocking GPU timer queries and ordinary run variation. Headline builds
contain none of the optional diagnostic hooks. Submission wall is 7.0029 ms: only
0.0174 ms exceeds thread CPU, with the same busy-spin caveat as above.

The nonblocking GPU ring returns all 100 spans: zero disjoint intervals, zero
full-ring skips and no pending queries. Mean/median/p95/max elapsed spans are
8.632/8.597/9.161/9.508 ms. They include GPU idle during CPU command production, **not active
GPU utilisation**; no driver-internal fence or hardware utilisation counter is
available here. Ring/result latency and CPU/wall evidence do not indicate a large
sleeping submission stall, but cannot rule out CPU-consuming polling.

## Experiment, validation and source disposition

The rejected `RR_GLES_INDEXED_SUBMISSION` flag used immutable indices 0..59999,
`GL_UNSIGNED_SHORT` and byte offsets selecting the same contiguous ranges. It
preserved triangle order, attributes, textures, scissor, depth and alpha. It
allocated no new per-frame buffers and used no architecture assembly or extensions.
It is removed from active CMake/backend source. The inactive
`INDEXED_SUBMISSION_EXPERIMENT.patch` preserves its exact implementation for
reproduction; applying it is deliberate and restores a default-OFF flag.

Retained changes: `engine/gles_submission_profile.inc`,
`engine/gles_texture_trace.inc`, optional hooks in `quad_gles2.c`/`tex_bake.c`,
the default-OFF CMake diagnostic flag, benchmark/correctness/analyser tools and
this report. No simulation, decoder, scheduler, audio or generated code changes.

- Native desktop build and native headless regressions pass.
- Native baseline/indexed captures at 600/1700/3600 are byte-identical; PCM/voice,
  simulation/board/video/geometry digests and ordered texture-event hashes match.
- All 14 benchmark processes exit successfully with zero traps/unmapped/ROM writes
  and no DSP/sound faults. Complete state and board digests match across all runs.
  Fixed frames match per-tick geometry, texture/cache and VBO counters.
- ARM64 ELF uses the matching H700 SDK, Linux 4.9 interpreter and SDL2/GLESv2/EGL/
  Mali/libm/libc; no desktop libGL dependency.
- After removing the experiment, native and ARM64 production `.text` match the
  baseline binaries actually benchmarked byte for byte.

Hardware same-GPU captures at 600/1700/3600 pass with identical full texture-event
sequences. Their event counts are 111209, 3155673 and 6527792; FNV64 hashes are
`cd51b065013bb464`, `5dd47fc7dc559155`, `b40897a42ea3cf41` respectively. All PCM,
voice, simulation/board/video/geometry digests also match across native/ARM64.
GPU texture IDs in the event hashes are compared only within the same GPU, not
across Mesa/Mali. Final baseline `.text` additionally matches the prior validated
projection-optimised ARM64 executable, explaining why the historical FPS variation
must not be treated as a code regression. The working executable SHA-256 remains
`79f7abe165562b0dbf6e653a059fb0e3300cc936965ed59b45a9d6ae87ab242f`;
EmulationStation was verified restored (PID27486). Runners verify the checksum
and restore the frontend. No commit is created.

## Reproduction

Production diagnostic build (no indexed experiment):

```sh
cmake -S raverace -B /tmp/rr-submission-audit \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/raverace/cmake/knulli-aarch64.cmake" \
  -DKNULLI_HOST="$HOME/dev/knulli-scarab/output/h700/host" \
  -DCMAKE_BUILD_TYPE=Release -DRR_GLES2=ON \
  -DRR_FAST_MEMORY_READS=ON -DRR_SPLIT_MEMORY_READER=ON \
  -DRR_C352_INACTIVE_FASTPATH=ON \
  -DRR_ORDERED_SUBMISSION=ON -DRR_GEOMETRY_INDEX_ORDER=ON \
  -DRR_GEOMETRY_REUSE_PROJECTION=ON \
  -DRR_MEMORY_READ_COUNTERS=OFF -DRR_C352_VOICE_COUNTERS=OFF \
  -DRR_GEOMETRY_PROJECTION_PROFILE=OFF \
  -DRR_GLES_SUBMISSION_PROFILE=ON -DRR_PIPELINE_AUDIT=ON
cmake --build /tmp/rr-submission-audit -j4
scp /tmp/rr-submission-audit/rr_gles2 \
  root@192.168.1.69:/userdata/roms/ports/rave-gles2/rr-submit-sparse-final-arm64
```

On the handheld, stop the frontend and use an EXIT trap to restore it, then:

```sh
cd /userdata/roms/ports/rave-gles2
GLES_PROFILE_FROM=1400 GLES_PROFILE_TO=1700 SDL_VIDEODRIVER=mali \
./rr-submit-sparse-final-arm64 extracted --frames 1700 \
  --coin 180 --gas 200 --steer 300:2304 --steer 700:2048 \
  --timing independent --test-present-every 3 \
  --skip-geometry-on-dropped-frames on --texture-budget-mib 64 \
  --vbo-upload frame --profile-from 1400 --profile-stages \
  --audit-csv submission.csv > submission.json 2> submission.log
```

For the rejected A/B experiment, deliberately apply the archived patch in a
separate checkout. Build OFF/ON with the settings above, but:

- Headline: `RR_PIPELINE_AUDIT=OFF RR_GLES_SUBMISSION_PROFILE=OFF`; copy to
  `rr-submit-OFF-arm64` / `rr-submit-ON-arm64`.
- Coarse: `RR_PIPELINE_AUDIT=ON RR_GLES_SUBMISSION_PROFILE=OFF`; copy to
  `rr-submit-audit-OFF-arm64` / `rr-submit-audit-ON-arm64`.
- Correctness traces: `RR_PIPELINE_AUDIT=OFF RR_GLES_SUBMISSION_PROFILE=ON`;
  copy to `rr-submit-trace-OFF-arm64` / `rr-submit-trace-ON-arm64`.

Deploy those separately named binaries and the two `RaveRacer-Submission-*.sh`
runners. Run the benchmark over SSH; after it completes, run correctness:

```sh
ssh root@192.168.1.69 'sh /userdata/roms/ports/rave-gles2/RaveRacer-Submission-Benchmark.sh'
ssh root@192.168.1.69 'sh /userdata/roms/ports/rave-gles2/RaveRacer-Submission-Correctness.sh'
# Download the printed result directory, then:
python3 raverace/tools/analyse_submission_benchmark.py /tmp/RESULT_DIRECTORY
```

Both existing experimental binaries and raw data remain on the device and locally;
no recompilation/ROM extraction is required to repeat those exact runs. Correctness
requires the preserved `rr_headless-c352-fast` reference. Native builds omit the
cross toolchain; use `SDL_VIDEODRIVER=offscreen MESA_GLES_VERSION_OVERRIDE=2.0`
and `test_gles_submission.py` with the two trace builds and headless reference.

Raw data: `/tmp/rr-submission-results/` and
`/userdata/roms/ports/rave-gles2/submission-benchmark-20261012-221026-25952`;
`submission-investigation` contains initial and final sparse CSV/logs;
`submission-correctness-20261012-221732-26094` contains hardware captures/digests.
The benchmark analyser validates all state/fault/cohort and per-tick geometry,
texture/cache and VBO invariants. No source or result file is committed.

## Recommended next experiment

Further small state caching, static sequential indexing or another VBO strategy
is unsupported by these results. Reducing **ordered draw count** is the remaining
large submission opportunity. A bounded 2–4-texture batch with statically selected
GLES2 samplers could combine consecutive materials without global sorting or a
large atlas. It must retain triangle/scissor order and drain before any referenced
texture changes. Its additional shader branches/resource validation may erase
CPU gains; first measure that shader cost against the existing single-sampler
path before implementing the batching architecture. A bounded atlas is another
candidate, but gutters, UV precision and in-flight page mutations make it more
complex. Neither is implemented here and no precise speedup is promised.

Full arcade rendering is not established: about 9.5 ms of simulation plus 16.18 ms
of presentation run on the same thread. Even removing the entire 6.55 ms submission
scope would leave roughly 19.1 ms per fully rendered tick, above 16.69 ms. Submission
work alone cannot close the 59.904 FPS gap; further preparation reductions or
carefully validated overlap would also be necessary.
