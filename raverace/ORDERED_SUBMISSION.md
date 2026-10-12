# Ordered GLES2 submission experiment

This is a narrow experiment, disabled by default with `RR_ORDERED_SUBMISSION=OFF`.
No texture atlas, texture-key changes, global sorting or simulation changes.
The dense opening (1400–1700 inclusive) is the primary score. The longer
1400–3600 window is an extended-drive/wall-tail regression, not normal racing FPS.

## Pipeline and evidence

The existing game scene pipeline decodes, projects, near-clips and sorts polygons
by System 22 priority. `quad_gles2.c` receives that ordered stream, performs screen
clipping and expands fans into an existing 2.4 MB vertex arena. UVs, brightness,
alpha and per-polygon priority depth are embedded in the vertices. Adjacent
polygons already share a batch when texture and scissor match. Different texture,
scissor, capacity or texture mutation ends the batch. Blend, depth, shader and
vertex format remain constant throughout submission. Global texture sorting would
break priority and transparency; this experiment never changes primitive order.

The frame VBO mode already uploads each contiguous ordered prefix once, then draws
using offsets. Cache mutation drains queued uses of the affected texture before
changing/deleting it. That barrier is essential and remains intact.

Existing handheld audit: dense opening has about 796 draws and 799 binds per
presentation. Scene preparation is about 4.98 ms, backend construction about
2.18 ms, texture lookup/decode 2.35 ms and upload 0.81 ms. Inclusive submission is
about 6.91 ms: 5.25 ms in draw calls, 0.20 ms VBO upload and 1.47 ms remaining
submission work. Those last 1.47 ms include timing, command processing, GL error
checks, scissor and texture calls; they are **not a measured texture-binding cost**.
Earlier textured-no-rebind runs showed no convincing gain. The audit's reduced
resolution control barely changed FPS, while suppressing draws substantially
helped: CPU/client-driver submission is the relevant measured opportunity.

## Experimental change

1. Merge only consecutive commands with identical original texture and scissor,
   contiguous vertex offsets and unchanged blend/depth/program state. All vertex
   order, including depth and transparency, remains identical. No merge crosses a
   submitted chunk or texture-mutation barrier. In particular, harmless command
   splits caused by intervening discarded geometry can be recovered.
2. Enable scissor once per submitted chunk; update its rectangle only when changed.
   Re-establish the first rectangle in every chunk, since geometry/cache work can
   change GL state. No cache of GL state survives a chunk or frame.
3. In frame-upload mode, do not issue scissor calls during CPU command construction;
   commands carry that state until submission. Legacy immediate mode retains its
   original scissor updates.

Texture binds, sampler/shader, decoding, uploads, budget, cache policy and VBO
streaming are unchanged. No heap allocations or expanded command/vertex arrays;
only a four-integer stack-local rectangle is added during submission. The four
existing diagnostic renderer modes remain available.

## Build and validation

Use all validated optimisations in both variants, with audit instrumentation OFF
for headline measurements:

```sh
SDK="$HOME/dev/knulli-scarab/output/h700/host"
for variant in baseline optimized; do
  enabled=OFF
  [ "$variant" = optimized ] && enabled=ON
  cmake -S raverace -B "/tmp/rr-ordered-$variant" -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE="$PWD/raverace/cmake/knulli-aarch64.cmake" \
    -DKNULLI_HOST="$SDK" -DCMAKE_BUILD_TYPE=Release -DRR_GLES2=ON \
    -DRR_FAST_MEMORY_READS=ON -DRR_SPLIT_MEMORY_READER=ON \
    -DRR_C352_INACTIVE_FASTPATH=ON -DRR_MEMORY_READ_COUNTERS=OFF \
    -DRR_C352_VOICE_COUNTERS=OFF -DRR_PIPELINE_AUDIT=OFF \
    -DRR_ORDERED_SUBMISSION="$enabled"
  cmake --build "/tmp/rr-ordered-$variant" -j 4
 done
```

For native builds omit the toolchain and SDK arguments. Profiling variants use
separate directories with `RR_PIPELINE_AUDIT=ON`; the runner expects
`rr_gles2-ordered-profile-baseline` and `rr_gles2-ordered-profile-optimized`.
Do not compare audit-enabled scores directly against uninstrumented scores.

```sh
SDL_VIDEODRIVER=offscreen MESA_GLES_VERSION_OVERRIDE=2.0 \
 python3 raverace/tools/test_ordered_submission.py \
 /path/to/baseline/rr_gles2 /path/to/optimized/rr_gles2 \
 /path/to/matching/rr_headless raverace/extracted /tmp/ordered-correctness
python3 raverace/tools/test_headless.py /path/to/matching/rr_headless raverace/extracted
```

The comparison forces identical presentations every third tick and captures frames
600, 1700 and 3600. It compares pixels byte-for-byte on the same GPU, PCM/voice,
board/video digests and full JSON against the headless reference. Cross-GPU pixel
identity is not expected. State and PCM must remain cross-architecture identical.

## Handheld procedure

Copy the experimental binaries under separate names into
`/userdata/roms/ports/rave-gles2/`; **never overwrite `rr_gles2`**. Copy the benchmark
script into `/userdata/roms/ports/`. With authenticated SSH:

```sh
scp /tmp/rr-ordered-baseline/rr_gles2 \
 root@192.168.1.69:/userdata/roms/ports/rave-gles2/rr_gles2-ordered-baseline
scp /tmp/rr-ordered-optimized/rr_gles2 \
 root@192.168.1.69:/userdata/roms/ports/rave-gles2/rr_gles2-ordered-optimized
# Copy the two separately built audit-enabled variants under profile names too.
scp raverace/tools/RaveRacer-Ordered-Submission-Benchmark.sh \
 root@192.168.1.69:/userdata/roms/ports/
ssh root@192.168.1.69 \
 'FIXED_ATTRIBUTION=1 sh /userdata/roms/ports/RaveRacer-Ordered-Submission-Benchmark.sh'
```

The runner executes five alternating A/B pairs, then one coarse CSV profiling run
per variant, using the same inputs and 64 MiB cache. `FIXED_ATTRIBUTION=1` adds two
alternating attribution-only pairs rendering every third emulated tick. Never use
their forced render FPS as a throughput result. It stops EmulationStation and
restores it with an EXIT trap, records exit codes, requires identical JSON and
checks binary hashes (including the working executable). Logs are stored in
`rave-gles2/ordered-benchmark-<timestamp>-<pid>/`. Keep the SSH connection open.
Run handheld correctness separately with `SDL_VIDEODRIVER=mali` and the same test
script/headless reference while EmulationStation is stopped; use a restoration trap.

Use `[SIM-RATE]` for simulation Hz and rendered FPS over the identical measured
wall-clock range. `[PROFILE] total` supplies presented-frame stage costs and draw,
bind/upload counts. `[MEMORY]` supplies process memory. Coarse CSV sums scene,
backend and swap main-thread CPU time; do not add nested inclusive stages twice.
Compare five-pair medians, ranges and paired changes; report profiled timings
separately from headline performance. Texture usage can differ in independent
mode because faster variants present different subsets of the same trajectory.

## Validation results and decision status

Native desktop target built successfully. Native and ARM64 GLES2 baseline,
experimental and ARM64 coarse-profile variants built using the matching H700
Buildroot compiler. AArch64 ELF targets Linux 4.9 with the expected loader and
SDL2/GLESv2/EGL/Mali dependencies; no desktop `libGL` dependency.

Native Mesa ES2 fixed-presentation comparisons at 600, 1700 and 3600 passed:
identical PPM bytes, PCM checksum and voice/chip state, board/video digests and
full simulation JSON versus the existing headless reference. Existing headless
regressions passed. All runs were fault-free. Desktop code and generated programs
are untouched. Test artifacts: `/tmp/rr-ordered-native-validation/`.

As an opportunity check, native fixed-presentation **boot through frame 1700**
(567 presentations, not the dense-only benchmark) reduced average draws from
467.4 to 462.4 and binds from 472.5 to 467.6. Triangles remained 1086.6, vertex
upload count/bytes and cache events were unchanged. This is only about 1% fewer
draws; the software Mesa submission timings showed no gain. These measurements
cannot establish the value of suppressing redundant state calls on Mali and are
not handheld headline performance results.

## Physical handheld results, 2026-10-12

SSH authentication subsequently succeeded. All native checks above also passed
on the physical RG40XX H (same-GPU pixel equality at 600, 1700, 3600, deterministic
state, PCM/voice/chip and board/video digests), plus the existing ARM64 headless
regressions. Native versus ARM64 JSON and PCM/voice/board/video digests match at
all three checkpoints. Mesa-versus-Mali pixel equality is not required.

Primary scores use five alternating uninstrumented pairs: baseline/optimised,
optimised/baseline, baseline/optimised, optimised/baseline, baseline/optimised.
Both contain all previously validated CPU/audio optimisations, independent timing,
skipped geometry ON, frame VBO streaming and 64 MiB textures. Measured range is
1400–1700 inclusive (301 ticks), after the identical boot/input warm-up. There is
no audit CSV, GPU-query, sampler or voice/memory-read counter instrumentation in
headline runs. Existing aggregated renderer wall timers and final memory reporting
are retained equally in both builds. No governor or system configuration changes.

| Headline metric (median of five runs) | Baseline | Optimised | Change |
| --- | ---: | ---: | ---: |
| Simulation Hz | 58.5517 | 58.5470 | effectively unchanged |
| Rendered FPS | 23.7781 | 24.4838 | +2.97% |
| FPS min–max | 23.6079–23.9264 | 24.1550–24.7242 | non-overlapping observed ranges |
| Presentations / skipped ticks | 122 / 179 | 126 / 175 | four more presentations |
| Draws / presentation | 794.5 | 792.2 | -0.29% |
| Texture binds / presentation | 797.4 | 795.1 | -0.29% |
| Submission wall ms / presentation (inclusive) | 6.906 | 6.546 | -0.360 ms |
| Draw-call wall ms / presentation | 5.245 | 5.170 | -0.075 ms |
| VBO upload wall ms / presentation | 0.200 | 0.196 | essentially unchanged |
| Scene preparation wall ms / presentation | 4.946 | 4.939 | essentially unchanged |
| Backend clip/build/state wall ms / presentation | 2.128 | 2.127 | essentially unchanged |
| Texture lookup/decode wall ms / presentation | 2.334 | 2.284 | different presented subsets |
| Texture upload wall ms / presentation | 0.810 | 0.781 | different presented subsets |
| Texture uploads in measured window | 235 | 235 | unchanged median |
| Resyncs | 2 | 2 | unchanged |
| Discarded lag ms | 102.969 | 106.233 | no improvement |
| Process RSS MiB | 199.414 | 199.480 | +0.066, within observed variation |
| System MemAvailable MiB | 633.08 | 633.31 | no pressure increase |

Paired FPS changes were +2.08%, +3.07%, +2.44%, +1.45%, +4.73%; all five positive.
The **median paired** change is +2.44%, distinct from +2.97% comparing the two
medians. Five short windows are evidence of a repeatable modest gain here, not a
confidence guarantee across gameplay or thermal conditions. Alternation limits
order bias; continuous frequency/thermal monitoring was not enabled in headline
runs. One later attribution snapshot was 1320 MHz / 47.4°C, during a paced
forced-presentation workload; it does not establish the active-draw frequency or
prove absence of transient throttling.

RSS ranges overlap: 197.67–200.07 MiB baseline versus 197.16–199.66 MiB optimised.
The fixed schedule's GPU frame-end peak is identical at 67,107,416 bytes, below
64 MiB; actual charged peak remains below the budget. No new GPU textures, shadow
buffers, cache allocations or per-frame allocations are introduced. Driver
physical storage can differ from charged RGBA bytes; that existing limitation of
budget accounting is unchanged.

### CPU attribution using identical presentation frames

Two additional alternating fixed-schedule pairs used the existing correctness
switch `--test-present-every 3` and coarse `RR_PIPELINE_AUDIT=ON` CSV attribution.
All four runs present the same 100 frames in the measured range (1401–1698 every
third tick). These are **attribution diagnostics**, not normal independent-mode
FPS tests. No screenshot or detailed per-draw CPU scopes were enabled.

| Mean ms / presentation, averaged over two runs per variant | Baseline | Optimised | Change |
| --- | ---: | ---: | ---: |
| Scene main-thread CPU | 4.9704 | 5.0353 | +0.0649 |
| Backend inclusive main-thread CPU | 12.7144 | 12.3058 | -0.4086 |
| Submission inclusive main-thread CPU (nested in backend) | 6.9458 | 6.5637 | -0.3821 |
| VBO main-thread CPU (nested in submission) | 0.1985 | 0.2014 | +0.0029 |
| Swap main-thread CPU | 0.4732 | 0.4755 | +0.0023 |
| Total presentation CPU: scene + backend + swap | 18.1580 | 17.8165 | -0.3415 (-1.88%) |
| Submission wall | 7.0390 | 6.7105 | -0.3285 |
| Draw-call wall (nested in submission) | 5.3200 | 5.2645 | -0.0555 |
| Remaining submission wall excluding VBO/draw | 1.5175 | 1.2405 | -0.2770 |
| Draws / presentation | 803.5 | 800.1 | -3.4 (-0.42%) |
| Binds / presentation | 806.8 | 803.4 | -3.4 |
| Triangles / presentation | 1926.8 | 1926.8 | identical |
| Texture misses / evictions / uploads | 234 / 791 / 234 | 234 / 791 / 234 | identical |

Both pairs reduced submission CPU: 6.9326→6.5416 ms and 6.9590→6.5857 ms.
Presentation CPU also fell in both: 18.1711→17.8149 and 18.1448→17.8181 ms.
Every CSV row matched frame selection, quads/triangles, cache hits/misses/evictions,
texture uploads and VBO transfer counts/bytes exactly. The experiment therefore
changes submission without changing geometry or cache behaviour.

Most of the measured wall reduction lies outside `glDrawArrays`, consistent with
suppressing redundant scissor calls. This combined experiment cannot assign an
exact isolated cost to scissor versus binding/coalescing; no costly per-state-call
clocks were added. Only 0.42% of draws are coalesced in the fixed dense workload.
The opportunity for **same-texture adjacent merging alone is small**. The previous
no-rebind control and this result do not support a separate texture-bind-only
optimisation. About 5.3 ms/presentation remains in actual driver draw calls, with
roughly 800 differently textured ordered submissions still required.

One separate normal-independent coarse-profile pair gave 17.800→17.585 ms
presentation CPU, but presented different subsets (115 versus 116 frames).
Those values are supporting observations; the identical-frame comparison above
is the stronger attribution. Coarse-profile FPS was 22.291→22.428 and is not mixed
with headline FPS. No new profiling scheme was added, so overhead characteristics
remain those measured in the architecture audit; that audit reported about 8%
city throughput overhead for coarse scopes, making uninstrumented scores essential.

### Integrity and artifacts

All 16 benchmark/attribution runs exited 0, with identical final JSON, zero traps,
unmapped accesses, unexpected ROM writes or DSP/sound faults. No shader, GLES or
texture-allocation failures were reported. All three native and physical
correctness checkpoints matched exactly. No simulation, audio, DSP, rendering
order or cache-policy changes. The extended-drive regression is used for correctness
only, not as representative racing FPS.

Persistent raw logs/CSV/JSON/exit files and binary manifest:
`/userdata/roms/ports/rave-gles2/ordered-benchmark-20261012-194445-12131/`.
Local copies: `/tmp/rr-ordered-results/ordered-benchmark-20261012-194445-12131/`.
Physical capture/PCM/state artifacts are in `rave-gles2/ordered-correctness/`, also
copied locally under `/tmp/rr-ordered-results/`. Five pairs plus the coarse profile
pair ran in the main runner; the two fixed pairs ran immediately afterward via an
isolated attribution script. `FIXED_ATTRIBUTION=1` now reproduces that same combined
procedure in the checked-in runner without a second frontend stop/start.

The working `rr_gles2` SHA-256 before and after remains
`79f7abe165562b0dbf6e653a059fb0e3300cc936965ed59b45a9d6ae87ab242f`.
EmulationStation restored successfully (verified PID 12879 after testing).
No commits were made and the working handheld executable was never replaced.

## Decision

**Retain behind `RR_ORDERED_SUBMISSION`, OFF by default**, as requested. The modest
benefit repeats across five pairs and is supported by identical-frame CPU savings,
with exact rendering/audio/state equivalence and negligible additional complexity
or memory. This is not a large architectural batching gain or progress to full
arcade-speed rendering: city remains about 24.5 FPS and simulation about 58.55 Hz.

Do not globally texture-sort or add more bind caching. The dominant submission
cost still comes from hundreds of genuinely different texture draws. A bounded
texture co-residency/ordered key trace would be the next investigation if larger
render gains are sought; it must establish achievable draw reduction and memory
cost before considering an atlas or another shader/batching architecture. No such
additional optimisation is implemented here.
