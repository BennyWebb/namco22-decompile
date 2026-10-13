# Ordered GLES2 multi-texture submission on RG40XX H

The later [shader-equivalence resolution](SHADER_EQUIVALENCE_RESOLUTION.md)
reproduced the original 91-pixel mismatch through different cached bake tiers
and submitted UV scales, using the same single-sampler shader. Frozen-resource
shader-only scene tests passed. The historical fragment-shader attribution below
is superseded; batching remains disabled and has not passed its own validation.

The subsequent [bounded batching follow-up](GLES2_MULTITEXTURE_FOLLOWUP.md)
restored a default-off prototype, passed exact draw/resource validation and measured
five representative A/B pairs. Racing FPS improved 2.32%, versus 9.41% in the dense
opening. The prototype is retained disabled by default and was not deployed.
The sections below describe the earlier rejected experiment.

## Scope and baseline

The primary workload is the recorded first racing lap, simulation ticks
1800–5639, at 640×480 with the original 3D scene, HUD, audio, three-stage
pipeline, 64 MiB texture budget and frame VBO upload. This experiment leaves
`RR_PREP_WAKE_ON_SNAPSHOT=OFF`. It does not alter simulation, snapshot queues,
texture decoding, or the installed Ports executable.

The preceding uninstrumented five-pair baseline with preparation wake-up OFF
measured 41.262 median rendered FPS, 20.411 median 1% low FPS, 39.296 ms
median p99 presentation interval, 49.126 ms median image age and 59.904 Hz
simulation. Those figures come from
[the pacing experiment](FRAME_PACING_LATENCY_EXPERIMENT.md); the instrumentation
below is for architectural attribution rather than a replacement baseline.

The current renderer submitted an average of 860.83 draws and 1816.45
triangles per presented picture in a 2626-picture diagnostic run. Average
submission wall time was 10.86 ms, including 7.91 ms in `glDrawArrays` calls,
0.32 ms in VBO upload, and state submission. Sparse, paired CPU-clock samples
estimated 9.81 µs per draw call and 1.79 µs per state/bind sequence. The
sampling cap was reached before the end of the lap; the whole-lap aggregate
timers are the more representative totals. Texture decoding and upload added
0.52 and 0.56 ms per picture on average, but their p99 values were 6.52 and
5.82 ms. The largest picture had 265 texture uploads. The endpoint JSON
matched the deterministic reference byte for byte.

The diagnostic ARM64 build uses `playable-knulli` with
`RR_PARALLEL_FRAME_PREP=ON`, `RR_PREP_WAKE_ON_SNAPSHOT=OFF` and
`RR_GLES_SUBMISSION_PROFILE=ON`. Set `GLES_BATCH_TRACE` to a new CSV path and
`GLES_PROFILE_FROM=1800`, `GLES_PROFILE_TO=5639`, then run the recorded replay
with the same options as `RaveRacer-Multicore-Replay-Benchmark.sh`. The device
trace and profiles are under
`/userdata/system/rave-racer-multicore/batching-profile-20261013-180530-23095`.
The rejected partial A/B/captures are under
`/userdata/system/rave-racer-multicore/multi4-ab-20261013-182049-28305`.

## Legal batch boundaries

The draw command stream is already ordered by polygon priority. The 3D
frame has one blend/depth/priority setup, and triangle alpha carries polygon
priority through the shader. Transparent texels use discard. There are no
per-polygon state changes for these properties. Each presented picture had
two scissor transitions, and this lap recorded no pending-command texture
mutation barriers. Texture changes split nearly every draw. A batch may merge
only contiguous commands with the same scissor and a bounded set of resident
textures. Vertex and triangle order remain unchanged. A cache mutation must
still drain any queued commands that use the affected texture.

| Ordered sampler capacity | Mean draws/picture | Optimistic reduction |
|---|---:|---:|
| 1, current | 860.83 | — |
| 2 | 424.84 | 50.6% |
| 4 | 207.58 | 75.9% |
| 8 | 100.84 | 88.3% |
| 16 | 49.31 | 94.3% |

These are contiguous-group bounds from the recorded stream. They do not
include the cost of additional texture-unit state changes, shader branching,
or larger vertex records. A four-unit design captures most of the theoretical
draw saving with modest shader complexity.

## Device limits and shader cost

The actual Knulli Mali-G31 driver reports OpenGL ES 3.2, 16 fragment texture
units, 1024 fragment uniform vectors, 15 varying vectors, 16 vertex
attributes, 8192 maximum texture size, and 23-bit fragment high precision.
The renderer deliberately uses GLSL ES
1.00 for its GLES2 path. The [GLSL ES 1.00 limitations](https://registry.khronos.org/OpenGL/specs/es/2.0/GLSL_ES_Specification_1.00.pdf)
do not guarantee dynamic sampler-array indexing, so the prototype uses four explicit samplers and
forward branches. The baked textures are nearest-filtered and do not use
mipmaps.

In a three-way device probe over ticks 1800–2100, the same experimental binary
ran in `off`, `shader`, and `batch` modes. Nonblocking
`GL_EXT_disjoint_timer_query` measurements had no disjoint samples or full
query-ring drops. Median elapsed GPU command span was 12.65 ms in `off`,
14.12 ms in `shader` (same draws and pixels, four-sampler shader), and 11.87 ms
in `batch`. This span can include GPU idle while the CPU submits commands; it
is a comparison on this driver, not an active-GPU utilisation measure. The
shader alone added about 1.47 ms median elapsed span in this window. Batching
reduced average draws from about 972 to 234, and average CPU submission wall
time from 12.02 to 6.88 ms. The tick-2000 captures were byte-identical across
all three modes, and the deterministic endpoints matched.

The 5.14 ms submission saving would remove roughly a quarter of the measured
22.5 ms presentation CPU time if the other work were unchanged. In this
instrumented opening window, observed rendered FPS rose from 39.80 to 45.90,
while the simulation ran at only 58.16 and 58.54 Hz respectively and each run
resynchronised once. This is a useful scale estimate, not a valid full-lap
smoothness result. Extra GPU texture work, vertex-slot preparation, binds and
audio scheduling all limit the attainable end-to-end gain.

The short-window result suggested a throughput improvement, but the pixel
checks below rejected the prototype. Its GPU timing and capture queries were
diagnostic; they should not be compared as headline pacing results against
earlier uninstrumented five-pair runs.

## Why atlasing was deferred

The existing desktop atlas relies on CPU page shadows and
`GL_UNPACK_ROW_LENGTH`, which GLES2 does not expose. A GLES2 atlas would need
row-packed subrect uploads and a separately accounted, bounded shadow or
staging buffer. It would also need guarded UV remapping for exact nearest
sampling and versioned ownership when slots are evicted or palette content
changes. Most frames have few uploads, but the measured p99 burst of 64 and
maximum of 265 uploads would touch shared pages repeatedly. Updating a page
after an earlier draw refers to that page needs an ordered drain, even if a
different slot is updated, unless the driver can prove independent storage
versions. That creates synchronisation and potentially whole-page hazards.
The four-sampler design retains one texture object per baked image and the
existing mutation hook, so it tests the draw-call opportunity without those
new resource-lifetime questions.

## Prototype and rejection

The bounded prototype used an OFF-by-default CMake flag and an independent
`off|shader|batch` runtime switch. It retained the existing texture objects,
mutation hook, 4096-command arena and polygon order. It added no threads,
queues, affinity or busy waiting. The unmodified first-texture renderer and
the prototype matched the deterministic endpoint JSON and the tick-2000 and
tick-1900 captures.

An alternating full-lap test stopped at pair 2 because the equivalent tick-2500
captures differed at 91 of 307,200 pixels. The ordered geometry digest was
identical (`27ee8fd4f1dbeb4a`), and the differences were confined to a
diagonal textured region. A separate `shader` run with the original draw count
had exactly the same changed pixels as the batched run. A vertex-only shader
probe with the original fragment shader matched the baseline. Keeping the
first texture lookup unconditional increased the difference to 187 pixels.
Sampling all four units unconditionally made `shader` mode pixel-identical,
but `batch` mode still changed 96 pixels. Thus neither tested multi-sampler
form preserves pixel-identical output on this Mali driver. The exact cause
within Mali's fragment and texture-unit paths has not been isolated, so the
result should not be attributed solely to rounding or primitive ordering.

The five alternating pairs were **not completed** because pair 2 failed the
required correctness gate. The instrumented partial runs also had audio
underruns and simulation resynchronisation. In pair 2, the baseline reported
114,608 underrun samples, 58.06 Hz simulation and three resyncs; the candidate
reported 31,934 underrun samples, 59.21 Hz and four resyncs. These runs are
not suitable as a deployment FPS, pacing or audio benchmark. No candidate was
deployed.
The fixed-cadence ordered texture-event and PCM/voice equivalence run was not
started after the pixel failure; the deterministic endpoint and tick-2500
ordered geometry digest did pass. The prototype used the existing single GLES
owner thread and added no shared queue, but it was not promoted to a runtime
ThreadSanitizer gate after rejection.
The installed executable, its backup and the single Ports launcher were
left unchanged; EmulationStation was restored. The experimental source was
removed from the working tree after rejection. Only the diagnostic draw-group
trace, behind `RR_GLES_SUBMISSION_PROFILE`, remains for future approaches.
