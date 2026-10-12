# GLES2 geometry-index experiment

The controlled candidate is `RR_GEOMETRY_INDEX_ORDER`, OFF by default and only
applied to GLES2. This investigates removing full-record permutation after the
existing priority sort, without changing decoding, projection, clipping or
emulated board operation. No NEON, compact replacement decoder or new geometry.

## Geometry path and dependencies

Translated 68K/master DSP execution updates polygon/point RAM, display-list state
and the direct-polygon queue. Those hardware operations, frame/video latches,
interrupts and direct consumption still happen every emulated tick. The existing
independent scheduler determines presentation before host geometry when skipped
geometry is enabled. Discarded ticks consume commands, advance palette/cache
bookkeeping and never build a scene; the next presentation is freshly decoded.

On a presented tick, `rr_gl_prepare_gles2`:

1. Converts planar palette state and latches board/video state.
2. Decodes queued direct polygons into `geo_quad` records and consumes commands.
3. Calls `eng_walk_list` over polygon RAM. View/light/object records configure
   Q15 matrices, translation, priority, clip window and texture/palette selection.
4. `geo_hw_object` walks point-ROM/point-RAM object packets; `quad_fixed` reads UVs
   and vertices, performs exact integer transforms, culls, computes priority,
   projects raw vertices, near-clips/interpolates fixed attributes and projects
   the render polygon. Guard-band `dv` construction remains unchanged even though
   Rave Racer uses `rv`; other engine consumers need that representation.
5. `push_quad` copies the complete record into the emission arena, assigns a
   unique per-frame order and object picker code. Direct polygons precede the
   display-list stream. The 8192-record initial arena grows only if required.
6. `eng_quad_sort` builds `(zsort, order, idx)` keys and qsorts them. Far priority
   first; ties are reverse emission order for Rave Racer. Baseline then permutes
   complete records cycle-by-cycle into that ordering.
7. GLES2 traverses the ordered stream, selects scissor and original texture,
   derives UV scale/bias and inverse depth, screen-clips, expands triangle fans,
   fills position/colour/perspective-UV vertices and ordered commands. Per-polygon
   depth encodes sorted priority rank, rather than physical eye distance.
8. Cache mutations drain queued uses before update/deletion. Frame VBO streaming
   transfers the contiguous prefix; ordered draws use offsets. Blend/depth and
   polygon ordering are unchanged. Swap/pacing follow the existing scheduler.

Static arrays cover vertex/command staging (2.4 MB / 114,688 bytes). Geometry and
key buffers grow via realloc, not by per-polygon allocation. Stack temporaries in
projection/near/guard/screen clipping remain. Texture metadata/scratch and the
64 MiB GPU budget are independent and unchanged.

## Measurements selecting this candidate

Initial physical fixed-presentation baseline: 100 identical presentation ticks
within 1400–1700, ordered submission OFF, all validated CPU/audio optimisations ON.
The new coarse sort sub-timers are enabled only in the optional pipeline audit.

| Component | Main-thread CPU ms / presentation |
| --- | ---: |
| Entire scene preparation | 5.1083 |
| List traversal + decode + transforms + emitted-record copy | 3.0742 |
| Entire priority sort | 1.7663 |
| Key construction/allocation/qsort subset | 0.4653 |
| Full-record permutation subset | 1.2916 |

The two sub-timers are **subsets** of scene/sort, not additional stages to add to
them. Their small remaining difference is timing/loop overhead. No per-vertex
or per-polygon clock reads were added. Traversal, decoding, exact transforms and
emission copy remain a combined stage; assigning exact exclusive costs inside
`quad_fixed` from call counts would be unjustified. Existing audit native-PC
samples identify that function and video memcpy as hot, but do not give caller
stacks or clean exclusive matrix/near-clip times. The directly measured permutation
cost alone is sufficiently large to justify this one conservative experiment.

`geo_vert` is 40 bytes and `geo_quad` is 1044: raw vertices 160 bytes, near-clipped
vertices 400, guard vertices 400, other fields 84. The reserved 8192-record arena
is 8,552,448 bytes (8.156 MiB). Average emitted count was 1230.85, about 1.285 MB (1.225 MiB) of
active records. Compact keys use 12 bytes each (~14.4 KiB for that count).
Permutation copied a logical average 1,299,612.96 record bytes per presentation,
including the cycle's temporary record. This is source-level copy volume, **not
measured DRAM bandwidth**. There were no sort reallocations in the measured window.
The emission copy itself also remains (~1.28 MB per scene before compiler effects).

Original sort copies complete raw/near/guard records although GLES2 only needs
near-render vertices and a subset of metadata. Physically sorted storage gives
sequential draw reads, but incurs write/copy traffic first. Indexed traversal
avoids writes at the cost of less sequential record reads; whether the trade is
beneficial is measured, not assumed. No PMU estimate is substituted for timings.

Fixed-point Q15 products, floor rounding, culling, wide projection divisions,
near-plane interpolation and priority arithmetic constrain vectorisation. NEON
float replacements could change pixels and are not justified. Existing AArch64
compiler vectorisation/NEON use documented in the architecture audit does not
make branch-heavy `quad_fixed` a straightforward SIMD loop. This change targets
memory movement rather than arithmetic and introduces no intrinsics or flags.

## Implementation

The existing allocated key array becomes the retained logical order. In the
flagged GLES2 build, `eng_quad_sort` returns immediately after the **same qsort**;
`eng_quad_index(position)` maps to the emission slot. GLES2 and scene diagnostics
use that mapping, including per-polygon priority rank and picker identity. No
second index/pointer array, per-frame allocation or new copy is introduced.
The mapping is valid until the next sort; the current single-thread presentation
path consumes it before any next scene. Empty/singleton sorts explicitly select
identity mapping so old keys cannot leak into a small next scene. Original
physical permutation is preserved when the flag is OFF, including desktop builds.

ARM64 disassembly confirms no `bl eng_quad_index` inside `eng_gles2_draw`: the
compiler inlines the lookup in its translation unit. Raw/near/guard data types,
all decoding and hardware-visible state remain byte-for-byte unchanged. Debug
scene traversal uses the same logical sorted view rather than accidentally showing
emission order. Header documentation describes the logical index lifetime.

Diagnostics append sort-key CPU/wall, permutation CPU/wall, logical moved bytes
and sort reallocations to the existing CSV. These are inclusive subsets, separate
from the exclusive audit-scope arrays, so existing CPU reconciliation is preserved.
Both headline variants compile with `RR_PIPELINE_AUDIT=OFF`; counter/timer loops
compile out. The static diagnostic stats record adds only a few dozen bytes.

## Reproduction

Build every variant with all validated CPU/audio options, matching SDK and
correctness flags. Separate copied binaries prevent an older/default build from
accidentally becoming a baseline:

```sh
SDK="$HOME/dev/knulli-scarab/output/h700/host"
for ordered in OFF ON; do
 for indexed in OFF ON; do
  build="/tmp/geometry-ordered-$ordered-index-$indexed"
  cmake -S raverace -B "$build" -G Ninja \
   -DCMAKE_TOOLCHAIN_FILE="$PWD/raverace/cmake/knulli-aarch64.cmake" \
   -DKNULLI_HOST="$SDK" -DCMAKE_BUILD_TYPE=Release -DRR_GLES2=ON \
   -DRR_FAST_MEMORY_READS=ON -DRR_SPLIT_MEMORY_READER=ON \
   -DRR_C352_INACTIVE_FASTPATH=ON -DRR_MEMORY_READ_COUNTERS=OFF \
   -DRR_C352_VOICE_COUNTERS=OFF -DRR_PIPELINE_AUDIT=OFF \
   -DRR_ORDERED_SUBMISSION="$ordered" -DRR_GEOMETRY_INDEX_ORDER="$indexed"
  cmake --build "$build" -j 4
  scp "$build/rr_gles2" \
   "root@192.168.1.69:/userdata/roms/ports/rave-gles2/rr_gles2-geometry-ordered-$ordered-index-$indexed"
 done
 done
# Build two additional separate directories with RR_PIPELINE_AUDIT=ON,
# RR_ORDERED_SUBMISSION=ON and index OFF/ON; copy under names
# rr_gles2-geometry-profile-OFF / rr_gles2-geometry-profile-ON.
scp raverace/tools/RaveRacer-Geometry-Benchmark.sh root@192.168.1.69:/userdata/roms/ports/
ssh root@192.168.1.69 'sh /userdata/roms/ports/RaveRacer-Geometry-Benchmark.sh'
```

For native builds omit the toolchain/SDK arguments. Keep
`-fno-strict-aliasing -fwrapv`, generated-code optimisation levels, SDK Cortex-A53
flags and stack protection unchanged. Existing desktop/headless targets are
unaffected. Both new and ordered-submission options remain OFF by default.

The runner executes five alternating index OFF/ON pairs **for each** ordered
submission setting (20 headline runs), interleaving settings to limit order bias.
It then runs two alternating fixed-presentation profiling pairs (ordered ON).
Every process boots the same ROM/EEPROM and applies coin180, gas200, steer300:2304,
steer700:2048. Only 1400–1700 inclusive is measured; the longer drive/wall-tail is
used for regression correctness, not representative racing FPS. `[SIM-RATE]`
simulation/render rates share the same wall interval. Detailed instrumentation is
OFF in headline builds; fixed every-third-tick profiles are attribution-only and
must not be interpreted as independent throughput FPS. Profiles use existing
wall stages plus main-thread CPU scopes and the two coarse sort sub-timers.

The runner writes separate logs/JSON/CSV/exit files, checks identical state and
binary hashes, stops EmulationStation temporarily and restores it with an EXIT
trap. It never replaces `rr_gles2`. Use SSH authentication normally; no credentials
are embedded. Runtime RAM/GPU budget remain 64 MiB textures and the original
geometry capacity; no packages, SDK downloads or ROM extraction.

Direct sort tests, both on host and via the SDK compiler:

```sh
for indexed in OFF ON; do
 flag=
 [ "$indexed" = ON ] && flag=-DRR_GEOMETRY_INDEX_ORDER
 gcc -O2 -fno-strict-aliasing -fwrapv $flag -Iengine \
  raverace/tools/test_geometry_index.c -o "/tmp/geometry-sort-$indexed"
 "/tmp/geometry-sort-$indexed"
 "$SDK/bin/aarch64-buildroot-linux-gnu-gcc" -O2 -fno-strict-aliasing -fwrapv \
  $flag -Iengine raverace/tools/test_geometry_index.c \
  -o "/tmp/geometry-sort-$indexed-arm64"
 done
```

Use the existing same-GPU comparison harness for each ordered submission setting:

```sh
SDL_VIDEODRIVER=offscreen MESA_GLES_VERSION_OVERRIDE=2.0 \
 python3 raverace/tools/test_ordered_submission.py \
 /path/to/index-OFF/rr_gles2 /path/to/index-ON/rr_gles2 \
 /path/to/matching/rr_headless raverace/extracted /tmp/geometry-correctness
python3 raverace/tools/test_headless.py /path/to/matching/rr_headless raverace/extracted
# Same command on device: SDL_VIDEODRIVER=mali, deployed paths, existing
# rr_headless-c352-fast, frontend stopped with a restoration EXIT trap.
```

That harness forces identical every-third-tick presentations and captures frames
600, 1700 and 3600, checking full JSON versus headless, PCM and voice/chip state,
board/video digests and PPM bytes. Cross-Mesa/Mali pixel equality is not required;
native/ARM state and audio equality is. Sort tests separately exercise full-record
payloads, priority ties in both directions, zero/singleton, increasing/shrinking
counts and repeated use. ARM64 assembly files are retained in `/tmp/rr-geometry-*assembly.txt`.

Allocation-count qualification: `sort_allocations` counts key-arena realloc calls,
not every libc allocation. Matching SDK glibc source
`output/h700/build/glibc-2.40-18-g5641780762723156b0d20a0b9f7df1d76831bab0/stdlib/qsort.c`
(lines 349–397) has a 1024-byte stack scratch buffer and malloc/free when
`n * element_size` exceeds it for elements ≤32 bytes. These 12-byte keys therefore
use one ~14.4 KiB library scratch allocation/free per typical dense sort (unless
malloc fails, when glibc falls back to heapsort). That library behaviour is the
same in both variants and is not dynamically counted by our diagnostic counter.
No new allocations are introduced or claimed to be eliminated by index traversal.

Observed uninstrumented AArch64 `eng_quad_sort` stack allocation is 1184 bytes
(96-byte prologue plus 1088 bytes) for baseline, versus 80 bytes indexed. Baseline
has three `memcpy@plt` call sites for the temporary and cycle moves; indexed has
none. Compiler stack protection is unchanged globally; disappearance of the
large local record naturally changes compiler-generated function protection.

## Physical handheld validation and results (2026-10-12)

All native and physical sort tests passed. Native full simulation/PCM/chip/board/
video and same-GPU pixel comparisons passed at frames 600, 1700 and 3600. Physical
comparisons passed all three checkpoints with ordered submission both OFF and ON.
Native versus ARM64 state and PCM/voice/board/video digests also match. Existing
native/ARM64 headless regressions and the desktop executable build passed. ELF
inspection confirms AArch64, the Linux 4.9 target loader and SDL2/GLESv2/EGL/Mali
libraries; no desktop `libGL` dependency.

### Headline independent-mode performance

Five alternating pairs per ordered-submission setting, interleaved by trial;
medians below are over five **uninstrumented** runs. The same 301 emulated ticks
and initial conditions are measured. Presented subsets vary with wall timing.

| Ordered submission | Index order | Simulation Hz | Render FPS | FPS min–max | Scene wall ms / presentation | Draws / presentation | Triangles / presentation |
| --- | --- | ---: | ---: | --- | ---: | ---: | ---: |
| OFF | OFF | 58.7523 | 23.6524 | 23.4276–23.8132 | 5.023 | 794.7 | 1907.8 |
| OFF | ON | 58.6565 | 25.4922 | 25.3380–25.7231 | 3.689 | 796.2 | 1911.0 |
| ON | OFF | 58.6159 | 24.5103 | 24.1474–24.5794 | 4.981 | 792.0 | 1909.5 |
| ON | ON | 58.6312 | 26.1421 | 25.8878–26.2964 | 3.716 | 792.2 | 1910.1 |

Comparing medians: **+7.78% FPS with ordered submission OFF; +6.66% with it ON**.
Every paired comparison improved:

- Ordered OFF: +8.755%, +7.632%, +6.403%, +8.781%, +8.196%; median paired +8.196%.
- Ordered ON: +7.287%, +6.351%, +6.358%, +7.207%, +7.770%; median paired +7.207%.

Median paired changes and ratios of medians are different statistics. Ranges are
non-overlapping within each setting here; five short deterministic windows do
not prove an identical benefit across all gameplay or thermal conditions.

Simulation Hz is essentially unchanged; it does not reach 59.904059904. Resyncs
remain two per measured window in every configuration's median. Discarded lag
medians OFF/OFF 99.675 ms, OFF/ON 96.093 ms, ON/OFF 101.853 ms, ON/ON 98.464 ms
show no major timing-policy change. The scheduler uses the saved CPU capacity
primarily for additional presentations. Do not present this as a simulation-code
speedup or compare against an unmatched historical baseline.

Uninstrumented submission wall time medians were 6.939→6.989 ms (ordered OFF),
6.590→6.626 ms (ON): the geometry change does not reduce GL work per presentation.
Binds medians 797.6→799.0 / 794.9→795.0; texture uploads per measured window median
235 in all four. Per-presentation lookup/decode and uploads fall slightly as more
presentations amortise similar miss counts; that is not a faster decoder.

### Identical-frame main-thread CPU attribution

Two alternating coarse-profile pairs with ordered submission ON and the existing
`--test-present-every 3` correctness override. All variants present the same 100
frames (1401–1698 every third tick). These are stage attribution diagnostics,
not independent-mode throughput results. No per-vertex clocks, native-PC sampler,
GPU timers or detailed per-draw scopes were enabled.

| Mean ms / presentation, averaged over two runs | Physical permutation | Indexed order | Change |
| --- | ---: | ---: | ---: |
| Entire scene CPU | 4.9077 | 3.7270 | -1.1807 (-24.1%) |
| List/decode/transforms/emission CPU, subset | 2.9491 | 3.0008 | +0.0516 |
| Entire sort CPU, subset | 1.6923 | 0.4598 | -1.2325 |
| Key build/qsort CPU, subset of sort | 0.4445 | 0.4523 | +0.0078 |
| Record permutation CPU, subset of sort | 1.2377 | 0 | eliminated |
| Backend inclusive CPU | 12.3587 | 12.4798 | +0.1212 |
| Submission inclusive CPU, subset of backend | 6.5357 | 6.5745 | +0.0389 |
| VBO CPU, subset of submission | 0.2106 | 0.1866 | minor variation |
| Swap CPU | 0.4771 | 0.4630 | minor variation |
| Total presentation CPU: scene + backend + swap | **17.7435** | **16.6698** | **-1.0736 (-6.05%)** |

Both individual CPU comparisons improved: 17.8010→16.6671 ms and
17.6859→16.6725 ms. Key sorting remains necessary. A small increase in backend
cost offsets part of the removed permutation. Less sequential indexed access is
a plausible contributor, but not isolated from cache/driver/timing variation;
we do not assign an exact per-polygon gather cost from these averages.

The remaining scene work is chiefly the combined ~3 ms list/decode/transform/
emission stage. Screen clipping, attribute conversion, fan expansion and state
construction remain combined in existing `render_other` wall timing. CPU texture
lookup is included in backend CPU in coarse mode; existing wall timings separate
it. The GL draw/submission stage remains substantially larger than key sorting.
Do not add nested subset timings to parents or confuse CPU time with GPU elapsed.

Existing wall-stage siblings (same fixed-frame profiles) provide the finer
render attribution; these include driver waiting and are not labelled exclusive
CPU. Together they reconcile with total presentation wall time:

| Wall ms / presentation | Baseline | Indexed |
| --- | ---: | ---: |
| Scene (includes list and sort) | 4.9375 | 3.7425 |
| Screen clip / UV & colour conversion / fan & command building / setup | 2.2245 | 2.3775 |
| Texture lookup/decode | 2.6570 | 2.5920 |
| Texture upload | 0.9040 | 0.9020 |
| VBO upload | 0.2140 | 0.1895 |
| Actual GL draw calls | 5.2130 | 5.2455 |
| Other submission state/command work | 1.2190 | 1.2390 |
| Swap | 0.4820 | 0.4665 |
| Total, excluding host wait/capture/log formatting | 17.8515 | 16.7545 |

Indexed reads increase the combined clip/build portion by ~0.15 ms here; this is
an observed trade-off, not evidence that every geometry operation became faster.
The saved large-record permutation dominates that penalty. No isolated timing is
claimed for each matrix multiply, vertex conversion or emission memcpy.

Detailed CPU scopes around each polygon/vertex were deliberately avoided. The
new coarse timers read clocks only at two sort-phase boundaries per presentation,
plus a simple logical copy counter in the diagnostic permutation loop. That
counter and its clocks compile out in headline builds. Reported phase costs
include this small diagnostic overhead; the actual speedup is established by
uninstrumented pairs, not inferred from instrumentation alone. Previously measured
coarse-audit overhead remains relevant (about 8% city FPS in the architecture
audit); forced-schedule FPS cannot quantify it and is not used as a headline.

### Geometry, allocations and memory

In **every row** of both fixed-frame pairs, emitted quads, rendered triangles,
draws, texture binds/hits/misses/evictions/uploads and VBO transfer counts/bytes
are identical. Each presentation averaged 1230.85 emitted quads, 1926.83 triangles
and 800.07 draws. Cache totals each run: 122845 hits, 234 misses/uploads, 791
evictions. GPU frame-end peak 67,107,416 bytes remains within 64 MiB; CPU/GPU
texture accounting and allocation policy are unchanged.

Logical permutation-copy bytes: **1,299,612.96→0 per presentation**. Key-arena
reallocations: **0→0 in the measured window**. Geometry still initially reserves
8,552,448 bytes and keys retain the same maximum capacity. This improves memory
traffic and stack use, **not buffer capacity**. No thin-record arena or duplicate
shadow buffer is introduced. The ~14.4 KiB libc qsort scratch allocation per dense
sort is unchanged and is not counted by the key-reallocation counter.

Fixed-frame RSS averaged 196.008 MiB baseline versus 195.963 MiB indexed: no
increase. Headline RSS medians (MiB) OFF/OFF 198.488, OFF/ON 199.789, ON/OFF 198.664,
ON/ON 199.137. Ordered-OFF independent runs therefore show ~1.30 MiB higher RSS
with indexed mode; this is reported rather than hidden. Different presentation
subsets/driver allocation lifetimes are plausible causes, not proven attribution.
The identical-frame comparison shows no algorithmic buffer increase, and all
capacities/texture budgets are unchanged. MemAvailable remained about 623–625 MiB
in headline medians, with no allocation failures or pressure observed. The finite runs completed within the configured budgets; a prolonged RSS-growth
or indefinite live-play test was not performed.

All 24 performance/attribution runs exited 0, with identical JSON and board
hashes, zero traps/unmapped accesses/unexpected ROM writes/DSP or sound faults.
No shader, GL or texture-allocation errors. The analyser also checks CPU-scope
reconciliation and exact fixed-frame geometry/cache/VBO equality. No controller,
audio, timing, texture-cache or generated-program changes.

### Artifacts and analysis

Persistent device directory:
`/userdata/roms/ports/rave-gles2/geometry-benchmark-20261012-202223-15446/`.
Local copy: `/tmp/rr-geometry-results/geometry-benchmark-20261012-202223-15446/`.
Initial attribution: device `geometry-investigation/`, local
`/tmp/rr-geometry-investigation/`. Physical pixels/state/PCM are in device
`geometry-correctness-OFF/` and `geometry-correctness-ON/`, copied under
`/tmp/rr-geometry-results/`. Native counterparts:
`/tmp/rr-geometry-native-correctness/`.

```sh
python3 raverace/tools/analyse_geometry_benchmark.py /path/to/downloaded/results
```

This requires all 24 runs complete, validates state/board consistency and row-level
same-frame counters, writes `summary.json`, and prints medians/ranges and paired
changes. CPU preparation/swap values exclude CSV formatting and report overhead;
headline rates include ordinary application/log/pacing time. No precise DRAM
bandwidth or per-instruction geometry-stage attribution was collected. CPU/GPU
governors were recorded and unchanged; continuous thermal/frequency monitoring
was not enabled in headline runs. Factorial interleaving reduces ordering bias.

Working executable SHA before and after remained
`79f7abe165562b0dbf6e653a059fb0e3300cc936965ed59b45a9d6ae87ab242f`.
EmulationStation restored and verified (PID 15936). Experimental executables use
separate `rr_gles2-geometry-*` names. No commits made.

## Decision and next step

**Retain `RR_GEOMETRY_INDEX_ORDER`, OFF by default.** All ten paired comparisons
improved, the directly targeted copy cost disappeared, total presentation CPU
fell in both fixed-frame pairs, and pixels/state/PCM/voice equivalence passed.
The extra indexed-read cost did not cancel the gain. It composes with ordered
submission and adds no buffer allocation. There was no failed optimisation needing
reversion in this pass; no speculative compact record or NEON experiment was added.

This does not make the renderer full-speed: combined mode is about 26.14 FPS in
this dense opening. A 16.67 ms **presentation** CPU cost is not a 60-FPS result:
the same thread must also execute roughly 10 ms of simulation per tick. Simulation
Hz remains about 58.6. The workload's wall-facing long tail is only a correctness
reference, not evidence of representative full-speed racing.

Remaining geometry opportunity is the ~3 ms traversal/decoding/transforms/emission
stage. Before changing representation, isolate how much of it is stack record
initialisation/emission copy versus exact arithmetic. Compact render-only records
could reduce that traffic/capacity but are **not yet justified by an isolated
emission measurement**. Further key-sort micro-optimisation has a small ~0.45 ms
ceiling. GL submission still consumes ~6.6 ms CPU/presentation, making ordered
texture co-residency investigation a higher overall rendering opportunity. No
additional optimisation is implemented here.

Changed files: `engine/quad_geometry.inc`, `quad_gl.h`, `quad_gles2.c`;
`raverace/src/rr_gl.c`, `rr_gles2.c`, `include/rr_gles2.h`, `CMakeLists.txt`;
new `tools/test_geometry_index.c`, `RaveRacer-Geometry-Benchmark.sh`,
`analyse_geometry_benchmark.py` and this report. Shared decoder/arithmetic types,
DSP, sound and generated code remain unchanged. Desktop physical sorting remains
the default path.
