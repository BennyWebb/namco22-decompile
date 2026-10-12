# Independent timing performance investigation

This records the original profiling baseline. The subsequent opt-in geometry
split and measured A/B results are documented in
[SKIPPED_GEOMETRY_OPTIMISATION.md](SKIPPED_GEOMETRY_OPTIMISATION.md).

This change adds profiling only. The simulation frequency, catch-up limit,
resynchronization policy, input sampling, rendering, texture cache and VBO
streaming remain unchanged. No optimization has been enabled. New handheld
stage timings are needed before selecting the next implementation.

## Findings from the reported RG40XX H run

Reported averages are 48.03 simulation steps/sec, 19.10 presentations/sec,
12.08 ms simulation and 4.31 ms scene preparation per step; heavy scenes fall
to 40–50 steps/sec and 8–16 rendered FPS. There were 642 resyncs and 19.52 s of
discarded wall-clock lag. These are user-supplied handheld results, not results
from the newly instrumented executable.

The fixed period is 16.693359375 ms. Simulation plus preparation already costs
16.39 ms/step, leaving only **0.303 ms/step** before rendering, swap, input and
other overhead. This explains why preserving the target clock is insufficient:
there is almost no remaining budget on that measured workload.

Approximately 60.2% of simulation steps were not presented
(`1 - 19.10 / 48.03`). If skipped steps also cost the reported average 4.31 ms and all that work
were removable, the saving would be about **2.60 ms per current simulation
step**. This is an optimistic estimate, not a measured result or a strict bound:
the essential handshake must remain, and rendered/skipped scenes may differ
in complexity.
Increasing the target rate or hiding discarded lag would not fix this budget.

## Essential work versus host graphics work

| Work | Currently on skipped steps? | Required every emulated tick? |
| --- | --- | --- |
| Translated 68K/DSP/sound program execution | Yes | Yes; game/device state and IRQ behavior |
| C352 sample generation, serial pulses, board I/O/vblank | Yes | Retain; playback output is disabled but device state still advances |
| `rr_scene_frame()` | Yes | Yes; advances scene frame, clears refresh and applies PDP visibility rule |
| `rr_scene_consume()` | Yes | Yes; discards this tick's direct polygons instead of accumulating stale commands |
| `g_eng_frame` advancement | Yes | Host/cache clock; preserve its relationship to simulation ticks in a split |
| Mixer/background latch | Yes | Host presentation snapshot; presented tick must use its own state |
| `eng_palette_from_planar()` | Yes | Host palette conversion/generation bookkeeping; candidate for deferral, with cache-generation care |
| Direct polygon decode into `qbuf` | Yes | Host geometry; candidate for presentation-only execution before consumption |
| List walking, point-ROM decoding, transforms/projection, near/guard clipping | Yes | Host geometry; candidate for presentation-only execution |
| `eng_quad_sort()` | Yes | Host ordering; needed for presented geometry, not the emulated board |
| Final screen clipping, UV generation, fan expansion | No | In `eng_gles2_draw()` only |
| Texture lookup/bake/eviction/upload | No | In `eng_gles2_draw()` only |
| Ordered GPU command construction, VBO upload, draws and swap | No | In the presented path only |

The current decision order is `rr_tick` -> `rr_gl_prepare` -> `rr_gles2_frame`
-> render/skip decision. Thus a skipped frame has already paid the whole CPU
scene cost. There are two clipping stages: geometry/near-plane clipping within
list decoding happens every step; final screen clipping in `quad_gles2.c`
happens only when drawing. Texture and GPU command work is already deferred.

`poly_word()` reads `g_rr.poly`; the point accessor reads DSP point RAM without
changing the DSP. Geometry output callbacks only copy into host `qbuf` and
update host ordering/diagnostic state. No emulated RAM write or IRQ acknowledgement
was found in these geometry paths. There is no architectural requirement to
project/sort a picture that is never submitted.

It is **not** safe to skip `rr_gl_prepare()` wholesale today: its scene latch
and consume operations implement the hardware-facing video bookkeeping.
It is feasible to split that minimal tick operation from presentation geometry.
A presentable tick must decode its direct queue before it is consumed and walk
that tick's current polygon list before later simulation overwrites it. Do not
collect polygons from multiple ticks or render an old raw list afterward.
Palette updates/generations and cache frame aging need explicit tests; initially
retaining palette conversion every tick would reduce the risk of a first split.

## Driver stalls and thread coupling

Simulation and GLES use the same thread. Any blocking GL call delays the next
emulated tick. Independent mode requests swap interval zero but Knulli/Mali may
still block inside `SDL_GL_SwapWindow()` on queue depth, display ownership or
GPU completion. Check the existing swap-interval warning and the `swap` field;
requested zero is not proof of a nonblocking driver.

There is no unconditional `glFinish`, fence wait or mapped-buffer synchronization
in the GLES2 path. Frame VBO `glBufferData` re-specifies contiguous storage;
its earlier Mali stall improvement is already validated. Occasional allocations,
`glDrawArrays`, texture/scissor binds and `glGetError` may still spend wall time
inside the driver. The profile measures that wall time, not asynchronous GPU
execution. Low draw-call or swap timings do not prove that the GPU is idle.

Texture misses decode pixels, allocate/upload storage and can evict/delete
textures on this thread. A cache mutation barrier can submit queued vertices
before changing a referenced texture. Nested vertex/draw time is subtracted
from texture lookup time by the existing backend, avoiding double attribution.
Upload timing includes texture setup/upload and error checking. The remaining
`texture_lookup_decode` includes cache bookkeeping, CPU decode and potential
allocation/deletion driver calls; it is **not** a pure decoder microbenchmark.
Compare misses, evictions, uploads and texture barriers with spikes before
blaming decoding. Skipped steps do not call this cache path. Palette generation
bookkeeping on skipped steps is CPU work, not a GPU texture upload.

Capture calls `glReadPixels` and writes a PPM, which can synchronize and materially
change timing. Do not capture during the performance baseline. Capture in a
separate correctness run. No new synchronization calls were added for profiling.

## Added profiling

`--profile-stages` enables the detailed scopes in either timing mode.
`--profile-from 1400 --frames 1700` restricts totals to 301 simulation ticks.
It is off by default; the existing benchmark launchers are unchanged.
Reports are aggregated every five seconds and at exit, never printed per tick.

`[STAGES-RATE] total` counts **all measured simulation steps**, rendered and
skipped, and reports elapsed wall seconds, actual `sim_hz` and `render_fps`.
It includes pacing/wait time in the wall-clock denominator. This is the correct
coupled-versus-independent comparison; comparing only rendered `[PROFILE]`
samples understates independent simulation work.

`[STAGES] total` has separate `cohort=rendered` and `cohort=skipped` rows.
Each field is an average over that row's sample count. Frame endpoints can have
gaps. A coupled run has a rendered row only; independent rows must sum to the
measured simulation count. No rendered-only cost is charged to skipped samples.

| Field | Attribution |
| --- | --- |
| `core68k_other` | Simulation interval minus explicitly timed devices; includes 68K and IRQ handlers, scheduler glue, scripted/interactive input writes and existing scheduler logging |
| `dsp` | DSP execution and vblank-window split work |
| `sound` | Translated sound-program slice |
| `audio_mix` | C352 emulated sample generation; no output device |
| `serial`, `board_io` | Serial pulse work; frame-boundary DSP/board vblank and link polling |
| `video_latch` | `rr_scene_frame` plus `rr_scene_consume` |
| `palette` | Host planar palette conversion/generation tracking |
| `direct_decode`, `list_decode`, `sort` | Direct geometry, list/object decoding including projection/near clipping, host sort |
| `scene_other` | Scene total minus scoped phases, including mixer/cache-frame bookkeeping and timing overhead |
| `clip_build_state` | Draw total minus texture/submission scopes: screen clipping, UV/fan/vertex construction and initial GL state/clear calls |
| `texture_lookup_decode`, `texture_upload` | Shared bake/cache path and GL texture setup/upload respectively |
| `vertex_upload`, `draw_call`, `submission_other` | Disjoint subdivision of existing submission time; other includes command recording, scissor/binds and GL checks |
| `swap` | SDL swap, including any driver wait |
| `host_wait` | Input polling and pacing/deadline wait; includes sleep, not a pure CPU cost |
| `capture` | Optional readback/file capture, separately reported |

These fields are disjoint accounting categories; do not additionally add the
old `[PROFILE] submission` to its three subdivisions. Residual categories have
clock noise and are not instruction-level attribution. GPU state calls in
`clip_build_state`/`submission_other` can block, so those fields are not pure
geometry math. Scope timers add roughly 140 counter reads per simulation tick;
compare an otherwise identical run without `--profile-stages` to assess overhead
on H700. Turning profiling on can change which presentations the clock selects,
while fixed-count emulation remains deterministic.

Rows also report prepared quads, submitted triangles/draws, measured-range cache
misses/evictions/uploads and frame-end texture resident/peak bytes. Skipped rows
show prepared geometry but zero submitted triangles/draws/cache activity.
Their resident bytes reflect storage retained from the last presentation. The
existing `[TEX]` line reports the whole-run allocation high-water and CPU memory;
frame-end sampling can miss transient within-frame peaks. Driver overhead and
in-flight retained storage are not measurable through GLES2.

## Exact handheld procedure

Build/deploy using [GLES2.md](GLES2.md)'s Buildroot instructions. Deploy the
rebuilt binary only; no launcher change is required:

```sh
scp /tmp/rr-gles2-arm64/rr_gles2 root@knulli:/userdata/roms/ports/rave-gles2/rr_gles2
ssh root@knulli 'chmod +x /userdata/roms/ports/rave-gles2/rr_gles2'
```

On the handheld with EmulationStation's framebuffer ownership released, run
identical scripted city input in fresh processes. Keep both runs **paced**:

```sh
cd /userdata/roms/ports/rave-gles2
for timing in independent coupled; do
    SDL_VIDEODRIVER=mali ./rr_gles2 extracted --frames 1700 \
      --coin 180 --gas 200 --steer 300:2304 --steer 700:2048 \
      --timing "$timing" --vbo-upload frame --render-mode textured \
      --profile-from 1400 --profile-stages \
      > "$timing-stage-state.json" 2> "$timing-stages.log"
done
cmp independent-stage-state.json coupled-stage-state.json
```

Confirm exit zero, identical final JSON, zero traps/faults and
`[STAGES-RATE] total steps=301`. Compare `[STAGES] total` cohorts plus actual
`[STAGES-RATE] total` rates and `[TIMING]` lag/resync fields. `[TIMING] total`
is still whole-run; `[STAGES-RATE] total` covers only 1400–1700. Repeat in
opposite order to check thermal variability. Repeat without `--profile-stages`
with separate filenames for overhead calibration (the older `[TIMING]` report
still supplies whole-run/window rates). Do not use `--unpaced`, `--input-log`
or a capture when establishing the real-time baseline.

For physical play through Ports:

```sh
cd /userdata/roms/ports
./RaveRacer-GLES2.sh --profile-stages --profile-from 1400 --frames 1800
./RaveRacer-GLES2.sh --timing coupled --profile-stages --profile-from 1400 --frames 1800
```

These append to `rave-gles2/rave-gles2.log`; interactive input is not identical
between runs, so use them for responsiveness checks, not deterministic workload
comparison. All four render modes remain available. A separate diagnostic-color
control can append `--render-mode diagnostic` to the executable command; compare
the same scripted range and remember its submitted draw count differs.

Retrieve logs and state files from the host:

```sh
scp 'root@knulli:/userdata/roms/ports/rave-gles2/*-stages.log' /tmp/
scp 'root@knulli:/userdata/roms/ports/rave-gles2/*-stage-state.json' /tmp/
```

## Opportunities ranked by expected benefit and risk

1. **Presentation-only geometry preparation.** Highest directly evidenced
   opportunity: avoid list/direct decoding, projection/near clipping and sorting
   on the approximately 60% skipped steps. Expected saving is bounded by the
   measured 4.31 ms preparation; the new stage breakdown determines its useful
   portion. Moderate integration risk because the PDP tick and direct-queue
   consumption must remain exactly once per simulation frame. Initially retain
   palette conversion and `g_eng_frame` advancement each tick. No renderer rewrite.
2. **Reduce host palette work if `palette` is material.** Candidate after the
   geometry split: defer/restrict conversion while preserving generation-driven
   invalidation and palette correctness. Potential smaller CPU saving; low-to-
   moderate implementation risk, particularly A→B→A palette changes between
   presentations. Measure first; no dirty tracking is implemented here.
3. **Investigate core/device hotspots.** The 12.08 ms simulation cost is a large
   fixed floor. The new DSP/sound/audio scopes distinguish it from 68K/scheduler
   residual work. Potential large benefit if one specific operation dominates,
   but high correctness risk for translated execution and emulated side effects.
   Do not omit sound execution/sample advancement simply because output is muted.
4. **Driver/render/cache follow-up, conditional on measurements.** If swap or
   upload/submission dominates rendered cohorts, correlate driver waits with
   cache misses/evictions/barriers and draw count. Benefit could be substantial,
   but currently unquantified for independent mode. Cache or batching changes
   carry texture-lifetime/transparency risks and should follow the geometry split.

The proposed next implementation is a small split between the mandatory video
front-end tick and host geometry preparation, with the presentation decision
made before expensive host geometry. Keep every IRQ, input sample, sound slice,
PDP frame update, consume operation and cache-frame increment. Retain exact
geometry/batching for presented ticks. Validate fixed-frame JSON and forced
menu/city captures, including skipped frames with direct polygons and tight-cache
eviction, then repeat this same H700 profile. Do not alter the target frequency
or four-step catch-up policy. No such split is implemented in this profiling change.

## Validation status

Native and Buildroot ARM64 builds are checked, as are the desktop/headless
builds and existing deterministic/texture/eviction regressions. The timing
regression now checks profiler cohort counts, finite timings, zero GPU/cache
work on skipped samples and byte-identical final pixels/JSON with profiling
and skipped presentations. Native observations establish attribution/correctness,
not H700 stage costs. New instrumented handheld timings remain unmeasured.

Native city validation (Mesa offscreen, 1400–1700) produced the following
structural comparison; independent presentation was capped at 30 to exercise
skipped cohorts reliably on this much faster host:

| Measurement | Coupled | Independent, 30 cap |
| --- | ---: | ---: |
| Actual range simulation steps/sec | 59.91 | 59.92 |
| Actual range presentations/sec | 59.91 | 29.86 |
| Measured simulation steps | 301 | 301 |
| Rendered/skipped samples | 301 / 0 | 150 / 151 |

The independent skipped cohort prepared 1,230.3 quads/step, spent 0.249 ms in
list decode, 0.093 ms sorting and 0.016 ms palette conversion, yet submitted
zero triangles/draws and recorded zero texture misses/uploads/evictions. Both
modes' final 1,700-frame JSON matched headless exactly. These measurements
confirm the duplicated host work and sample accounting; they cannot predict
H700 timings or prove a particular driver stall mechanism. Very small scopes
can round to 0.000 ms in native logs.
