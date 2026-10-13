# Ordered four-texture batching follow-up — 2026-10-13

The bounded prototype passes exact draw equivalence and provides a repeatable,
small end-to-end benefit: median first-lap FPS **34.476 → 35.276 (+2.32%)**.
This is not a substantial representative racing FPS increase. The existing dense
opening improves **9.41%**, showing why the earlier short benchmark overstated
its value for a complete race.

Retain the prototype **disabled by default**, because all five pairs improve FPS
and the representative pacing/image-age metrics also improve. It was not deployed.
The extra shader/GPU work consumes much of the submission saving; batching alone
does not approach 59.904 rendered FPS. No shader colour adjustments, pixel
tolerances or texture-quality policy changes were introduced.

## 1. Resource consistency

The [shader-equivalence resolution](SHADER_EQUIVALENCE_RESOLUTION.md) identified
different baked resources, rather than different shader arithmetic. This follow-up
traced their selection and eviction under normal playback.

The cache checks UV rectangle, bank, palette group, cmode, bake cap and palette
generation. Opacity participates in its hash; this GLES path keeps opacity at 1.
ROM texture arrays and allocation/sample configuration remain immutable during a
run. The failing road case shows no incomplete-key or missed-invalidation evidence:
source signature and palette generation match, while the returned caps differ.
This conclusion is about the Rave Racer GLES path, not arbitrary runtime changes
to the shared engine's otherwise fixed configuration.

For the road key `(1871,576,593,79, bank 12, palette 103, cmode 0)`, one diagnostic
history records:

| Event | Tick | Cap |
|---|---:|---:|
| Initial bake | 2293 | 256 |
| Larger projected request, bake | 2483 | 512 |
| 529-pixel request, bake | 2499 | 1024 |
| Budget eviction | 2651 | 256 |
| Budget eviction | 2734 | 512 |
| Budget eviction | 2888 | 1024 |

A skipped tick-2499 presentation can miss the request that creates the sharper
entry. The sticky-tier lookup subsequently returns a resident sharper tier, or
bakes the requested smaller one if no sharper entry remains. Frame dropping and
thread scheduling affect the request/residency history; they do not change the
sampler's arithmetic. These event ticks describe this captured history only.

Equal source texture state alone cannot guarantee equal selected tiers under
this intentional residency policy. Always choosing the current requested tier
would remove the existing anti-popping behaviour. Persisting higher-tier decisions
through eviction, or processing dropped geometry, would change rebaking cost,
cache pressure and/or skipped-frame work. None was introduced as a batching fix.

The user explicitly chose to preserve the policy, validate exact draws, and audit
independent runtime cache differences. Whole-renderer image equality therefore
requires equivalent presentation/resource history, not just equal simulation tick.

### Normal runtime attribution

An independent off/batch pair at tick 2500 differed at 187 pixels. Complete
resource manifests show identical polygon sets, positions, depth, colours and
source signatures, but two legal tier differences:

| Texture rectangle and source | Off | Batch |
|---|---|---|
| 1871,576,593,79; bank 12, palette 103, generation 8 | cap 1024, 594×80 | cap 512, 298×80 |
| 1663,1791,44,257; bank 5, palette 66, generation 9 | cap 256, 45×130 | cap 512, 45×258 |

All 187 changed pixels are covered last by polygons with those differing resources:
978 (16), 1351 (75), 1066 (89), 673 (2), and 1396 (5). The first key differs in U
scale; the second differs in V scale. An earlier preliminary pair differed at 96
pixels while the original road key matched; it was not misclassified as a shader
failure. The complete manifests identify the additional rectangle.

## 2. Bounded implementation

- `RR_GLES2_MULTITEXTURE_BATCH=OFF` is the source default. Enabling it requires
  `RR_ORDERED_SUBMISSION=ON`; runtime selection still defaults to `off`.
- `--gles-multitexture off|shader|batch` selects the original shader, the explicit
  four-sampler shader with unchanged draws, or ordered batching.
- GLES2/GLSL ES 1.00 uses four explicit samplers and conditional selection by a
  constant slot at all three corners of each triangle. Four available fragment
  texture units are checked at startup.
- A group contains only consecutive commands with the same scissor and at most
  four resident texture names. Triangles and their original inputs remain ordered.
- Existing upload/deletion hooks drain pending references before mutation.
  The 60,000-vertex and 4,096-command bounds remain; no new worker or queue exists.
- Batch mode requires frame VBO upload. Depth, priority, alpha/discard, blending,
  palette decoding and original HUD/composition semantics are preserved.

Only the draw transformation is enabled by this option. Cache selection and
geometry preparation are unchanged.

## 3. Correctness gates

| Gate | Result |
|---|---|
| Preserved road reproduction, triangles selecting four actual units | Conditional and straight-line shaders: zero RGBA differences |
| Normal-policy replay through tick 6006, live original-program reference | 2,296 presented scenes; zero RGBA differences |
| Fixed presentation history, normal caches: off/shader/batch | Exact state, board, PCM, voices, ordered non-binding texture events and HUD captures |
| Actual feature-disabled build versus prototype | Same fixed-history state/audio/draw digests and captures |
| 8 MiB cache stress through tick 1700 | 650 scenes, 827 chunks; zero RGBA differences with 262 mutation barriers |

The live reference draws each command chunk before later texture updates/deletions,
using the normally selected resources. It performs no cache lookups, tier overrides
or polygon exceptions. Its RGBA8/depth16 target compares every scene pixel including
alpha. The normal cache/palette policy and frame dropping remain active. Reference
rendering changes timing and adds memory/readback cost, so it supplies correctness
validation, not performance measurements.

The fixed-history test presents every third tick, exercising the normal decoder,
cache, palette invalidation and asynchronous drawing. Twelve composed 640×480 HUD
captures match byte for byte at ticks **600, 1500, 1701, 1902, 1965, 2001, 2499,
2502, 3600, 4800, 5637 and 6006**. The final tick-6006 capture is also saved separately.
These include dense geometry, original HUD/priority/shadow transparency, first-time
loading and later palette/texture changes. Replay checking remains enabled through
tick 6006. Fixed-cadence correctness pacing is not representative racing FPS.

All modes share:

```
ordered texture events: 12809029
ordered texture digest: 2a6578ef363d13f3
PCM digest:              0c24e8fca4b6400e
voice digest:            b76f90445ba87e18
```

The semantic event digest retains ordered requests, hits/misses, deletion and
upload events; it excludes texture binds because reducing/reassigning binds is
intentional. The existing complete binding-inclusive trace remains available.

The constrained-cache test performs **41,618 uploads and 41,489 evictions**.
It tests pending-resource draining more heavily than the representative 64 MiB
run, which has no within-frame mutation barriers. No tolerance or visually-similar
acceptance was used.

## 4. Representative handheld performance

Five alternating A/B pairs used the recorded first-lap replay,
`race-20261013-122242-25136.rec`, measured at ticks **1800–5639**. Both builds use
the validated memory/C352/projection/geometry/ordered-submission optimisations,
three-stage preparation/presentation, wake-on-snapshot OFF, frame VBO upload,
64 MiB textures, original HUD, audio ON and overlay OFF. The baseline compiles the
multi-texture feature OFF; the candidate compiles it ON and selects batch mode.
Diagnostic profiling, reference rendering and captures are disabled.

Presentation CSVs, replay tick timestamps and one-second `/proc` samples use
comparable tracing in both variants. All ten endpoint JSONs match the deterministic
reference. Values below are medians of five run summaries, not pooled frame samples.

| Ticks 1800–5639 | Baseline | Batched |
|---|---:|---:|
| Rendered FPS | 34.476 | 35.276 |
| 1% low FPS | 18.189 | 19.064 |
| p99 presentation interval | 43.491 ms | 40.704 ms |
| Median per-run worst hitch | 105.257 ms | 97.376 ms |
| Worst observed hitch across five runs | 1503.571 ms | 104.236 ms |
| Median snapshot image age | 59.665 ms | 57.897 ms |
| p95 snapshot image age | 73.968 ms | 71.454 ms |
| Draw calls/presentation | 878.938 | 213.337 |
| Submission wall time/presentation | 11.219 ms | 7.414 ms |
| Presentation-owner CPU/presentation | 23.823 ms | 19.827 ms |
| Swap wall time/presentation | 4.081 ms | 6.834 ms |
| Simulation rate | 59.898 Hz | 59.887 Hz |
| Median audio underrun samples | 0 | 0 |
| Peak process RSS | 295.031 MiB | 296.430 MiB |

Draws fall **75.73%**, submission wall time **33.91%**, and presentation CPU about
**4.00 ms**. Most of the saved time does not become earlier swap returns: swap
wall time rises **2.75 ms**. Image age improves **1.77 ms** at the median and
**2.51 ms** at p95. This software age is not controller-to-photon latency.

| Pair | Baseline FPS | Batched FPS | Gain |
|---|---:|---:|---:|
| 1 | 33.737 | 35.370 | 4.84% |
| 2 | 34.570 | 35.245 | 1.95% |
| 3 | 34.529 | 35.271 | 2.15% |
| 4 | 33.661 | 35.403 | 5.17% |
| 5 | 34.476 | 35.276 | 2.32% |

The first pair reports **80,229 baseline and 3,243 candidate underrun samples**;
the other eight runs report zero. Baseline run 1 also has the 1.504-second hitch
and about 1.73 seconds of discarded pacing lag. Its cause was not isolated and
no run was discarded. The improvement also appears in every other pair.

These contemporaneous absolute results must not be substituted for or compared
as a controlled A/B against the older 41.262-FPS handover baseline. Device/runtime
history and measurement sessions differ. CPU-frequency/thermal observations remain
in the dataset. Most bounded samples read 1512 MHz; candidate run 5 includes a
1200-MHz sample. Recorded temperatures span approximately 57–73 °C.

### Additional memory

Measured peak RSS increases **1.398 MiB**. Fixed CPU storage adds 240,000 bytes for
vertex slots and 180,224 bytes for the bounded group arena: **420,224 bytes** total.
A maximum-size VBO store adds 240,000 bytes per store; driver storage renaming may
retain several stores. The measured RSS delta also includes driver/working-set
variation, so it is not attributed solely to the fixed arrays. Texture budget and
presentation queue bounds do not grow. Optional reference targets exist only when
validation is enabled and are excluded from these measurements.

## 5. Shader/GPU and workload effects

A separate off/shader/batch diagnostic uses the same four-sampler-capable binary,
complete submission CPU timers and nonblocking `GL_EXT_disjoint_timer_query`.
The measured range is the same first-lap window. Each mode matches endpoint state.

| Diagnostic | Off | Shader only | Batch |
|---|---:|---:|---:|
| Complete submission CPU, mean/presentation | 11.302 ms | 11.693 ms | 7.117 ms |
| Complete submission CPU, median/presentation | 11.207 ms | 11.566 ms | 7.062 ms |
| Median 3D GPU command span | 10.046 ms | 11.691 ms | 10.464 ms |
| p95 3D GPU command span | 13.950 ms | 15.717 ms | 14.626 ms |
| Valid GPU samples | 2605 | 2577 | 2885 |
| Disjoint samples / query-ring drops | 0 / 0 | 0 / 0 | 0 / 0 |

The shader-only path adds **1.645 ms (+16.38%)** to the median 3D command span.
Batching saves **4.185 ms** of submission CPU relative to off, including about
0.263 ms/presentation of grouping work. Its median command span is nevertheless
0.418 ms above off. This supports the observed limit on racing gains.

A command span includes GPU idle while commands/textures are supplied; it is not
active GPU utilisation and cannot simply be added to CPU time. These timers and
profiling change pacing: the instrumented runs render faster than the unprofiled
headline runs. Their FPS is not used for the retention decision. No unverified
claim about Mali machine-code generation or active utilisation is made.

Three alternating dense-opening pairs used the existing scripted coin/gas/steer
workload, ticks **1400–1700**, with unprofiled binaries and original HUD/audio.
This is a workload comparison, not the representative racing score.

| Dense opening | Baseline | Batched |
|---|---:|---:|
| Rendered FPS, bounded presentation intervals | 48.407 | 52.964 |
| 1% low FPS | 18.687 | 18.307 |
| p99 interval | 34.639 ms | 35.540 ms |
| Median per-run worst hitch | 77.293 ms | 75.287 ms |
| Draws/presentation | 792.826 | 179.883 |
| Submission wall time | 8.380 ms | 4.920 ms |
| Median image age | 41.163 ms | 34.974 ms |
| p95 image age | 54.025 ms | 51.765 ms |

Dense-opening FPS improves **9.41%**, but its 1% low and p99 interval worsen
slightly. A dense-opening gain does not establish a comparable whole-lap gain.

## Artifacts and reproduction

[Machine-readable summaries](benchmarks/multitexture-20261013-summary.json) retain
all five run results, monitor summaries, GPU/dense measurements, resource
attribution and correctness counts. Detailed host copies are under
`/tmp/rr-multi-resume/`. On the handheld:

```
/userdata/system/rave-racer-multitexture-resume-20261013/bin/
/userdata/system/rave-racer-multitexture-resume-20261013/results/fixed-correctness/
/userdata/system/rave-racer-multitexture-resume-20261013/results/runtime-manifests/
/userdata/system/rave-racer-multitexture-resume-20261013/performance/
/userdata/system/rave-racer-multitexture-resume-20261013/gpu-dense/
```

The former `/tmp/rr-multi-resume` path remains an alias. `/tmp` is RAM-backed on
Knulli: newly created binaries/results were moved to the SD card to avoid filling
it. Binary relocation and SD-card performance output setup completed before
the timed pairs; completed correctness data was archived afterwards. Earlier
correctness/resource runs are not performance data. Prior captures and diagnostic tools were preserved.

Build the baseline and candidate in isolated directories with `playable-knulli`,
`RR_PARALLEL_FRAME_PREP=ON`, `RR_PREP_WAKE_ON_SNAPSHOT=OFF`, and profiling OFF.
Select `RR_GLES2_MULTITEXTURE_BATCH=OFF` for the baseline and ON for the candidate.
For the diagnostic binary use ON and `RR_GLES_SUBMISSION_PROFILE=ON`.
Place these as `bin/rr_baseline`, `bin/rr_batched`, `bin/rr_checks` and `bin/rr_gpu`
in a new SD-card experiment directory; retain the deterministic tick-5639 reference
from the existing multicore workload. Use:

- [Correctness runner](tools/RaveRacer-Multitexture-Correctness.sh)
- [Five-pair benchmark](tools/RaveRacer-Multitexture-Benchmark.sh)
- [GPU and dense-opening runner](tools/RaveRacer-Multitexture-GPU-Dense.sh)
- [Existing five-pair analyser](tools/analyse_multicore_replay.py)
- [Resource attribution](tools/analyse_multitexture_resources.py), requiring NumPy
- [GPU/dense analyser](tools/analyse_multitexture_gpu_dense.py)

`RR_MULTITEXTURE_TEST_DIR` selects the experiment root; `RESULTS_DIR` selects a new
output directory. Default outputs include timestamps and refuse an existing path.
For full scene reference checks set `GLES_MULTI_VALIDATE` to an existing output
directory and select runtime batch mode. `GLES_MULTI_VALIDATE_FROM/TO/STRIDE`
control the checked range. Manifest capture uses `GLES_RESOURCE_DUMP` and
`GLES_RESOURCE_DUMP_FRAME`; targeted tier tracing uses `GLES_TIER_TRACE` and
`GLES_TIER_KEY`. These diagnostics never override the chosen resources.

The installed playable and backup SHA-256 values were verified unchanged:

```
active: 77536f56ad17de9cf8772175c73442d9cb246d87aeb6fbf6c8d7f07c3d6ab53e
backup: ed27bc04b41c402e2181a1f36c4b3876becffea6224a9499bd9a181120dbedb1
```

EmulationStation was restored and the sole active Ports entry remains
`Rave Racer.sh`. No experimental executable was installed there and no commit
was created.
