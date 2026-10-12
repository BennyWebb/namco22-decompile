# RG40XX H texture-cache budget experiment

This change permits 32/64/96/128 MiB runtime budgets and adds a controlled
comparison runner. **64 MiB remains the default.** Decoding, sampling, keys,
second-chance eviction, atlas setting, draw order and VBO streaming are unchanged.
No toolchain download, ROM extraction, package installation or automatic commit
is performed. Handheld results are required before changing the practical default.

## Implementation review

`engine/tex_bake.c` stores a 65,536-entry table, probing at most 32 slots.
A cache hit sets the entry's reference bit. Before allocating on a miss, the
clock hand walks entries, clears referenced bits on the first pass and deletes
unreferenced textures to fit the incoming storage. Its scan is bounded to two
table sweeps. Separately, a full 32-slot insertion window replaces the home
entry, so not every counted eviction is byte-capacity pressure.

The GLES2 path deletes evicted textures rather than keeping storage in the
desktop free-object pool. Queued draws referencing a changed/deleted texture
are submitted before that operation. The atlas remains disabled.

Accounting is the sum of each live object's **allocated width × height × 4**
for RGBA storage, including guard texels and the four-byte white texture.
Eviction occurs before `glTexImage2D`; oversized textures/allocation errors fail
explicitly. The application-owned texture storage never exceeds the selected
budget. Texture names generated in batches have no texel storage until allocated.
This is accurate accounting of requested texel payload, **not physical GPU
memory accounting**: driver metadata, alignment, retained in-flight resources,
GPU mappings and firmware-reserved memory are outside it. Mali memory can also
be outside process RSS. Compare RSS, MemAvailable and optional CmaFree as well.

Decoded RGBA lives in one reused 4,202,500-byte scratch buffer. It is not retained
per texture/key. The scratch allocation stays resident; its contents are
replaced by subsequent bakes. A GPU eviction therefore removes the reusable
bake, requiring decoding/upload if requested again. CPU cache metadata is
4,456,448 bytes, with additional logged auxiliary/ROM/geometry memory. Increasing
the GPU budget does not add a CPU-side decoded cache or large shadow atlas.

### Why a miss may occur

- Capacity: the desired entry was evicted to admit other texture storage.
- Key changes: normalized UV rectangle, texture bank, palette group, cmode,
  resolution cap and palette generation must match. Dynamic bounds, tier changes
  and palette updates can require new bakes even without byte pressure.
- Policy/table behavior: only 32 slots are probed; a saturated probe window
  replaces its home entry. Lookup also stops at an empty slot, while arbitrary
  eviction clears `occupied` without tombstones. An eviction-created hole can
  therefore hide a still-resident colliding entry further along the probe chain.
  This is a code-level false-miss hazard, not a measured explanation for a given
  percentage of H700 misses. No lookup/policy fix is included here.

The current counters combine those causes. A larger budget reducing misses is
evidence that capacity contributes; it does not identify every remaining miss.
Do not label all evictions as capacity misses or all misses as repeated decoding
of otherwise identical texels. Per-cause counters or a focused collision test
would be separate diagnostic work if this comparison leaves churn unexplained.

### Rendering qualification: existing sticky tiers

The existing `ENG_TEX_TIERSTICKY` default retains a higher-resolution bake for
wide UV rectangles while it remains cached. A smaller budget can evict that
bake and later decode a lower requested tier. Thus a budget experiment preserves
the policy but is not guaranteed to preserve every sampled pixel or identical
sampling resolution across budgets. Disabling tier stickiness or forcing a
single cap would change the experiment and has **not** been done.

Native frame-1700 checks found 3,806 of 307,200 pixels (~1.24%) different at
32 MiB versus 64 MiB. The 64/96/128 captures were byte-identical, and default
64 MiB also matched the pre-change frame-1700 capture exactly. Simulation JSON
was identical at all four budgets. Treat 32 MiB as a potentially different LOD
workload and inspect handheld captures separately; do not conceal this caveat
when interpreting speed differences. No decoder or geometry replacement is used.

## Build once

From the repository root, with the existing extracted ROMs/toolchain:

```sh
cmake -S raverace -B /tmp/rr-gles2-native \
  -DRR_GLES2=ON -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Release \
  -DRR_GLES2_TEXTURE_BUDGET_KB=65536 -DRR_GLES2_TEXTURE_MAX_BUDGET_KB=131072
cmake --build /tmp/rr-gles2-native --target rr_gles2 -j4

cmake -S raverace -B /tmp/rr-gles2-arm64 \
  -DRR_GLES2=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/raverace/cmake/knulli-aarch64.cmake" \
  -DKNULLI_HOST="$HOME/dev/knulli-scarab/output/h700/host" \
  -DRR_GLES2_TEXTURE_BUDGET_KB=65536 -DRR_GLES2_TEXTURE_MAX_BUDGET_KB=131072
cmake --build /tmp/rr-gles2-arm64 --target rr_gles2 -j4
file /tmp/rr-gles2-arm64/rr_gles2
readelf -h -l -d /tmp/rr-gles2-arm64/rr_gles2
```

The new ceiling is separate from the default and allocates no cache upfront.
A smaller build ceiling can be selected; runtime values beyond it fail. The
ceiling must be at least the default and no greater than 131072 KiB. Subsequent
budget changes use `--texture-budget-mib`, with no rebuild or extraction.
The ELF uses the matching Buildroot compiler/sysroot and has no desktop libGL.

Before deployment run the existing headless, GLES2 texture and timing tests
from [GLES2.md](GLES2.md), plus:

```sh
SDL_VIDEODRIVER=offscreen MESA_GLES_VERSION_OVERRIDE=2.0 \
  python3 raverace/tools/test_gles2_cache_budgets.py \
  /tmp/rr-gles2-native/rr_gles2 /tmp/rr-headless-native/rr_headless raverace/extracted \
  /tmp/cache-validation-images
```

This compares full final JSON, validates requested storage ceilings, verifies
default 64 MiB and invalid CLI values, captures frame 1700, reports cross-budget
pixel differences and checks the identical no-eviction 96/128 scene. Captures
are correctness runs, separate from throughput measurement.

## Deploy and run over SSH

Replace `KNULLI_IP` with the handheld's SSH address. Existing `rave-gles2/extracted`
is reused. Do not launch alongside another framebuffer application; release
EmulationStation's graphics ownership with the same procedure as prior tests.

```sh
KNULLI_IP=192.168.1.69
scp /tmp/rr-gles2-arm64/rr_gles2 \
  "root@$KNULLI_IP:/userdata/roms/ports/rave-gles2/rr_gles2-cache-bench"
scp raverace/tools/RaveRacer-GLES2-Cache-Benchmark.sh \
  raverace/tools/gles2-cache-summary.awk \
  "root@$KNULLI_IP:/userdata/roms/ports/"
ssh "root@$KNULLI_IP" \
  'chmod +x /userdata/roms/ports/rave-gles2/rr_gles2-cache-bench /userdata/roms/ports/RaveRacer-GLES2-Cache-Benchmark.sh'
ssh "root@$KNULLI_IP" \
  'cd /userdata/roms/ports
/etc/init.d/S31emulationstation stop
trap "/etc/init.d/S31emulationstation start >/tmp/rr-cache-es-restart.log 2>&1" EXIT
./RaveRacer-GLES2-Cache-Benchmark.sh'
```

The runner prefers `rr_gles2-cache-bench` when present, preserving the existing
interactive `rr_gles2`. `RR_CACHE_BINARY` can select another executable.
If local SSH configuration is broken, use `ssh -F /dev/null` and
`scp -F /dev/null`, as used for these measurements.

The runner requires only the executable, ROM folder, `/proc`, POSIX shell/awk,
`sha256sum` and standard shell utilities. **No Python is needed on Knulli.**
It runs 32, 64, 96, 128 sequentially in fresh processes using the same script:
1,700 simulated frames, coin 180, gas 200, steer 2304 at 300 and center at 700.
The measured range is **1400–1700 inclusive (301 frames)**. It explicitly selects
textured rendering, frame VBO uploads, coupled timing and `--unpaced`.
`--profile-stages` supplies actual measured-range wall throughput; its overhead
is the same configuration for every budget. `--profile-memory` supplies the
process peak before texture storage is released. There is no capture/per-frame
logging, game input, ROM extraction or build step in the runner.

For three repeated batches with alternating order, on the device:

```sh
cd /userdata/roms/ports
for trial in 1 2 3; do
    case "$trial" in
        2) order='128 96 64 32' ;;
        *) order='32 64 96 128' ;;
    esac
    BUDGET_ORDER="$order" ./RaveRacer-GLES2-Cache-Benchmark.sh
done
```

If the short city range lacks cache reuse/churn, use a longer identical race
workload without changing the input script:

```sh
FRAMES=3600 PROFILE_FROM=1400 ./RaveRacer-GLES2-Cache-Benchmark.sh
```

All budgets still execute the same frames and expose the same final state.
For interactive independent timing, test separately using:

```sh
./RaveRacer-GLES2.sh --texture-budget-mib 96 --profile-stages --profile-memory --frames 1800
```

That is wall-clock-paced gameplay with render skipping and physical input;
its `sim_hz`/`render_fps` cannot be compared directly to unpaced benchmark FPS.
Repeat other budgets on the same course, checking responsiveness and lag/resyncs
only after the scripted comparison. Do not automatically change the launcher default.

## Output and interpretation

Each batch creates a new persistent directory under
`/userdata/roms/ports/rave-gles2/cache-benchmark-<timestamp>-<pid>/`.
`RESULTS_DIR=/an/unused/directory` overrides it; an existing directory is refused.
Files include manifest/binary SHA, one stderr log, JSON state, exit code and
memory-sample TSV per budget, plus `summary.tsv`. The console prints a concise
table; the TSV includes all collected CPU timings, cache hits/misses/evictions/
uploads, GPU peaks, memory and SHA-256 of the complete final-state JSON.

`frame_ms` and `fps` use measured-range wall time divided by the 301 simulation
steps, from `[STAGES-RATE] total`. Every step is rendered in this coupled-unpaced
experiment. It is **scripted throughput**, not a 59.904-Hz real-time test. Texture
lookup/decode includes cache bookkeeping and some driver operations; submission
includes vertex transfer/draw/state costs. Do not add vertex time to submission
again. Cache counters are range-only; GPU `peak` and process RSS high-water are
whole-run, including warm-up. GPU range frame-end peak is also retained in TSV.
Driver/asynchronous GPU time is not measured by these CPU wall-clock scopes.

Memory samples are taken once a second, including pre/post-run system availability.
VmHWM logged by the application catches process RSS peaks between samples;
MemAvailable/CmaFree minima can miss very short transient pressure. Unavailable
fields are `NA`. RSS is not GPU payload and cannot simply be added to GPU bytes
without double counting shared mappings; native Mesa memory behavior is not
Mali memory behavior. CmaFree, if present, is useful context rather than proof
of the Mali/ION allocation pool's available capacity.

Default `MIN_AVAILABLE_KIB=32768` skips/terminates a run if sampled system
MemAvailable falls below 32 MiB. `MAX_SECONDS=600` bounds each run. Both settings
are written to the manifest; set the memory threshold to 0 explicitly if a
reviewed experiment requires disabling it. A memory guard, timeout, crash,
allocation/render failure, wrong sample range or mismatched state produces a
non-OK row and a nonzero script exit. Such rows are not valid speed comparisons.
The guard cannot guarantee prevention of a fast OOM or a driver-specific memory
failure. Exit/status files and raw logs remain available when final JSON is absent.

All valid states are compared byte for byte with the successful 64 MiB run,
including registers, RAM hashes and execution counters. A failing 64 MiB baseline
marks comparison states `BASEFAIL`. Do not accept matching truncated/failed
runs: the runner requires the full frame count, zero traps/sound/DSP faults,
complete profile samples and successful exit. Script state hashes supplement,
not replace, the game's existing FNV RAM/register checksums.

Retrieve results:

```sh
scp -r "root@$KNULLI_IP:/userdata/roms/ports/rave-gles2/cache-benchmark-*" /tmp/
```

## Native results (not handheld results)

Three sequential batches, with varied execution order and no concurrent game
tests, used native Mesa offscreen/llvmpipe. The table shows median throughput
and maximum observed process RSS across those runs. Every row had successful
exit, identical final simulation JSON and bounded application-owned storage.

| Budget MiB | Median frame ms | Median FPS | Range misses/uploads | Range evictions | GPU peak MiB | RSS peak MiB |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 32 | 4.571 | 218.75 | 701/701 | 1404 | 32.0 | 318.2 |
| 64 | 4.468 | 223.79 | 235/235 | 831 | 64.0 | 337.2 |
| 96 | 4.449 | 224.79 | 232/232 | 0 | 77.1 | 343.4 |
| 128 | 4.439 | 225.30 | 232/232 | 0 | 77.1 | 343.7 |

Median selected CPU times (ms per measured frame):

| Budget MiB | Simulation | Scene | Lookup/decode | Texture upload | Submission | Swap |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 32 | 1.033 | 0.361 | 0.178 | 0.034 | 2.650 | 0.003 |
| 64 | 1.033 | 0.361 | 0.116 | 0.010 | 2.625 | 0.003 |
| 96 | 1.040 | 0.352 | 0.109 | 0.012 | 2.634 | 0.003 |
| 128 | 1.030 | 0.355 | 0.109 | 0.012 | 2.612 | 0.003 |

32→64 reduced range misses/uploads from 701 to 235, with lower lookup/upload
time. 64→96 removed 831 evictions but avoided only three uploads in this range;
those evictions mostly removed data not requested again before frame 1700.
96→128 changed neither the misses nor storage requirement: both peaked at
77.1 MiB. All four range hit counts and timings remain in the TSV files.

The 64/96/128 FPS ranges overlap (64: 220.67–228.03; 96: 222.30–227.69;
128: 224.79–227.51). Their small median differences do **not** establish a
repeatable speed gain. Cache-counter reductions alone are not sufficient. The
32-MiB pixel/LOD qualification also limits pure performance comparisons.

The native host had roughly 29 GiB or more available system memory during these
runs and no failures/guard stops. That says nothing about memory pressure on
the roughly 1-GiB H700. Its driver may benefit more from avoiding deletes, or
have different physical allocation overhead; neither is established here.

Raw native results are `/tmp/rr-cache-solo-1/`, `/tmp/rr-cache-solo-2/` and
`/tmp/rr-cache-solo-3/`. The tested executable supports all four budgets without recompilation.

Native desktop/Buildroot ARM64 builds, headless, texture, timing and controller
regressions passed. Default rendering matched the pre-change frame-1700 image
byte for byte. Runner fault-injection checks report allocation-failure exit codes
and mismatched states as invalid comparisons; the preflight memory guard also
produces non-OK rows and nonzero exit. These are workflow tests, not actual
handheld allocation failures.

## Measured RG40XX H results

Three batches completed on the physical RG40XX H at `192.168.1.69`, using orders
32/64/96/128, 128/96/64/32 and 64/128/32/96. Each process executed 1700 scripted
frames; timings and cache counters cover frames 1400–1700 (301 frames).
These are coupled, unpaced throughput measurements, not independent-mode gameplay.

| Budget MiB | Median frame ms | Median FPS | FPS range | Misses/uploads | Evictions | GPU peak MiB | Maximum RSS MiB | Minimum available MiB |
| ---: | ---: | ---: | --- | ---: | ---: | ---: | ---: | ---: |
| 32 | 30.143 | 33.18 | 32.99–33.27 | 701/701 | 1404 | 32.0 | 159.1 | 707.1 |
| 64 | 29.442 | 33.97 | 33.84–34.02 | 235/235 | 831 | 64.0 | 201.0 | 671.6 |
| 96 | 29.379 | 34.04 | 33.99–34.22 | 232/232 | 0 | 77.1 | 208.1 | 664.3 |
| 128 | 29.372 | 34.05 | 33.93–34.11 | 232/232 | 0 | 77.1 | 208.1 | 664.5 |

Median CPU wall times in milliseconds per measured frame:

| Budget MiB | Simulation | Scene | Lookup/decode | Texture upload | Submission | Swap | Vertex upload (included in submission) |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 32 | 12.801 | 4.932 | 2.330 | 0.383 | 6.964 | 0.466 | 0.192 |
| 64 | 12.937 | 4.941 | 1.415 | 0.321 | 7.082 | 0.473 | 0.192 |
| 96 | 12.920 | 4.972 | 1.301 | 0.351 | 7.095 | 0.468 | 0.191 |
| 128 | 12.919 | 4.950 | 1.295 | 0.350 | 7.153 | 0.470 | 0.192 |

All 12 runs exited successfully with identical full simulation JSON, matching
native state SHA-256 `b2a926aa3c0990c3ad27a512f7eda27abc9f22438b743487416e869a46821f1a`.
No allocation/render errors, simulation traps or DSP/sound faults were reported.
No memory guard fired; sampled CmaFree minimum was 51.7 MiB for every budget.
This establishes comfortable sampled memory for this workload, not all later tracks.
EmulationStation was restored after testing; the interactive executable was preserved.

Persistent result directories under `/userdata/roms/ports/rave-gles2/`:

- `cache-benchmark-20261012-000503-28725`
- `cache-benchmark-20261012-001230-30080`
- `cache-benchmark-20261012-001524-30808`

Retrieved copies are in `/tmp/rr-cache-h700-results/`. Each contains raw logs,
state JSON, memory samples, binary SHA and the complete comparison TSV.

## Recommendation and separate follow-ups

Retain **64 MiB as the RG40XX H default**. Moving from 32 to 64 reduces
misses/uploads by about 66% and improves median throughput about 2.4%, although
the existing 32-MiB LOD difference qualifies that comparison.
Moving from 64 to 96 eliminates 831 evictions but saves just three uploads;
median FPS rises only about 0.2%, with overlapping run ranges. This does not
establish a repeatable performance improvement. 128 adds no benefit here:
96 and 128 both use only 77.1 MiB at peak and have identical cache counters.

96 MiB is an optional capacity/headroom setting for a longer race experiment,
not a measured speed fix. Aggregate counters cannot prove exactly why the
64-MiB evictions rarely cause repeat uploads in this interval. Before changing
policy, instrument miss causes and test a longer identical race if churn recurs.
The empty-probe-chain hazard merits a separate correctness-focused test.
A CPU decoded cache is not justified by these results; consider it separately
only if repeated identical keys are re-decoded and decoding dominates cost.

No decoder, eviction-policy, atlas, batching or other optimisation changes were
made. Changes remain uncommitted for review.
