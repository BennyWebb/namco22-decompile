# Controlled WRAM read experiment

## Existing semantics audited before implementation

`rr_read(a,size)` first invokes the optional trace probe. Journal/checker mode
may replay/capture I/O reads, followed by the ordinary I/O read callback. Polygon
RAM has its own 24-bit-connected/sign-extension behavior. Otherwise `region`
selects a backing byte array and rejects a read if its full width exceeds that
region. The read then records checker bytes if requested and assembles a
big-endian value one byte at a time. Writes are a separate path and remain unchanged.

Ordinary byte-array reads accept unaligned addresses. A boundary-crossing read
is not split between regions: it faults, increments `n_unmapped`, and returns
zero. Fault diagnostics are deduplicated by page; checker quiet mode suppresses
printing. This is the existing implementation's behavior, not a new 68K bus model.
The baseline also accepts some nonstandard widths; the new fast path must defer
them to the original implementation.

WRAM is 128 KiB at `0x10000000` and mirror `0x18000000`. Original selection uses
`a & 0xF8000000`, then `a & 0x07FFFFFF` as the offset; offsets beyond 128 KiB
fault rather than wrapping. ROM is 2 MiB at zero. Other backing regions include
shared RAM, EEPROM, graphics/palette RAM, SCI and syscon. A backing array alone
does **not** prove side-effect-free access: SCI register reads are intercepted,
polygon reads have special semantics, and generic/custom I/O hooks can override
any address. The resolver checks text before CGRAM; although an old comment says overlap,
the current size constants make these ranges adjacent. Crossing their boundary
still faults instead of combining bytes.

The actual board `rr_hw.c:io_read` handles keycus, SCI, DIP/serial input and
nop/diagnostic regions; it neither accepts WRAM/mirror addresses nor changes
state on their rejection. Serial reads shift state and keycus reads advance
random state. Those callbacks must remain active on their real device addresses.
Custom hooks and checker/trace facilities need a conservative fallback.

Previous CPU sampling identifies `rr_read`, `io_read` and `region` as hot, but
does not identify read addresses. New **separate instrumented builds** count
read widths/starting-address buckets and callbacks to establish whether WRAM
is frequent. No second region will be accelerated in this experiment.

## Selected candidate and controls

The candidate is WRAM plus its existing mirror only, with the entire 1/2/4-byte
read contained in that array. Fixed-width `memcpy` plus endian conversion avoids
unaligned casts/aliasing UB and allows native load/byte-swap instructions.

A build-time switch avoids an on/off branch or new indirect dispatch on every
baseline access: `RR_FAST_MEMORY_READS=OFF` remains the default. Separately,
`RR_MEMORY_READ_COUNTERS=ON` is for frequency analysis, not performance scores.
The existing arbitrary hook setter revokes WRAM bypass; an explicit board-only
certification API states that WRAM rejection is side-effect-free. Checker/journal,
read-log and active trace-probe modes must use the original path.

## Implementation

`rr_read` keeps its original body as the baseline. When compiled with
`RR_FAST_MEMORY_READS`, it first checks `(a & 0xF7FE0000) == 0x10000000`, which
selects precisely the two 128-KiB windows, validates size 1/2/4 and full bounds,
and checks callback certification and debug observers. It reads the existing
`g_rr.wram` bytes; no cache, shadow memory or second representation exists.
`memcpy` to fixed-width unsigned integers plus GCC endian conversion is aliasing
and alignment safe. ARM64 assembly contains `ldrh`/`rev16` for word reads and
load/`rev` for long reads. Odd-address loads passed on physical H700.

The original arbitrary `rr_set_io_hooks` clears certification. The audited board
uses `rr_set_plain_wram_io_hooks`, whose contract is that WRAM rejection leaves
`*out` and emulated state unchanged. A false callback return alone does not
establish that contract: an arbitrary callback can modify the output accumulator
or device state even when rejecting a read. Tests explicitly cover this case.
A NULL read hook is safe. Journal mode, read-byte logging, an active trace probe,
unsupported widths or a crossing read all fall through the original code with
its original callback/probe ordering. Writes and all other regions are unchanged.

The baseline contains no fast-path option check; selection is at build time.
Counters are separately compiled into `rr_mem.c` only. They have no per-read
clock or logging calls and count widths, starting-address buckets, hits,
fallbacks and actual callback invocations. Their interval is the existing
simulation interval (the preceding frame-edge IRQ is included; the terminal
edge after the last picture belongs to the next interval). Counter builds are
not used for headline performance or CPU hotspot sampling.

A byte-defined `[BOARD-STATE]` exit digest supplements existing JSON and video
digests. It serialises all `g_hw` scalar fields plus syscon/SCI registers and SCI
RAM, excluding padding/pointers and optimisation metadata. Reads are direct,
side-effect-free snapshot reads, not MMIO callbacks. It does not hash every
private DSP/C352 field; existing DSP/sound counters, faults and regressions
remain part of validation.

Affected files: `src/rr_mem.c`, `include/rr_mem.h`, audited hook installation in
`src/rr_hw.c`, CMake options, counter interval hooks in `src/rr_sim_profile.c`
and `src/rr_headless.inc`, and exit diagnostics/profile-range access in
`src/rr_gles2.c`/`include/rr_gles2.h`. New tests/build/benchmark/analysis scripts
are under `tools/`. No generated game, geometry, texture-cache, GPU submission,
write-path implementation, controller logic or clock policy was changed.

## Reproducible builds

Ninja is already installed on this host. It avoids Make's shared flags-file
recompilation when switching source-only defines: the generated game objects
are compiled once and each later variant rebuilds only `rr_mem.c` and links.
The builder requires a configured Ninja tree; it downloads/installs nothing.

```sh
cmake -G Ninja -S raverace -B /tmp/rr-memory-native-build \
  -DRR_GLES2=ON -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Release
raverace/tools/build_memory_variants.sh \
  /tmp/rr-memory-native-build /tmp/rr-memory-native

cmake -G Ninja -S raverace -B /tmp/rr-memory-arm64-build \
  -DRR_GLES2=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/raverace/cmake/knulli-aarch64.cmake" \
  -DKNULLI_HOST="$HOME/dev/knulli-scarab/output/h700/host"
raverace/tools/build_memory_variants.sh \
  /tmp/rr-memory-arm64-build /tmp/rr-memory-arm64
```

This creates `rr_gles2-baseline`, `rr_gles2-fast`, `rr_gles2-baseline-counts`,
and `rr_gles2-fast-counts`. The reusable Ninja tree finishes with both options
OFF, as do the source defaults. The usual cached build trees have also had
these options reset OFF; rebuild before using their old output artifacts.
Use the named variant artifacts above for this experiment. `-fno-strict-aliasing`,
`-fwrapv`, and existing generated-code optimisation levels are retained.

A standalone selection, without the matrix builder, is:

```sh
cmake -S raverace -B YOUR_GLES2_BUILD \
  -DRR_FAST_MEMORY_READS=ON -DRR_MEMORY_READ_COUNTERS=OFF
cmake --build YOUR_GLES2_BUILD --target rr_gles2 -j4
```

The ARM64 ELF uses the matching H700 SDK, `/lib/ld-linux-aarch64.so.1`, Linux 4.9
and SDL2/GLESv2/EGL/Mali; there is no desktop libGL dependency.

## Correctness tests and results

Direct tests compile the actual `rr_mem.c` with baseline/fast definitions.
Only checker/probe seams are stubs; the real game tests use the actual scheduler,
translated programs and devices. Covered cases:

- Byte/word/long values, aligned and odd addresses, both exact windows/mirrors.
- Region starts/ends, crossing faults, invalid extra aliases, unmapped addresses.
- ROM/shared memory, polygon 24-bit bus/sign extension and its original odd-word
  behavior, adjacent CGRAM/text boundary, unchanged writes/live alias visibility.
- Arbitrary true-returning hooks, rejecting hooks that modify the output,
  certification revocation, future side-effecting device reads after WRAM reads.
- Active trace probes, read logging, journal capture/replay and write journaling.
- Unsupported widths 0/3/8 and existing negative-width quirks: these use the
  original reader rather than silently imposing new validation.
- Fault counters, quiet mode and page-deduplicated diagnostics; baseline/fast
  stdout/stderr transcripts match, including intentionally generated faults.
- 20,000 deterministic varying-width contained reads over both mirrors.

Native and physical ARM64 direct tests passed. Address/undefined-behavior
sanitizers passed natively; LeakSanitizer was disabled because the execution
sandbox's ptrace environment prevents it operating. The path allocates no heap
memory. Native real-game A/B at frames 600 and 1700 matched the headless reference,
PPM bytes, video digests and board digest. Existing desktop/headless builds,
headless, texture/four diagnostic mode and coupled/independent timing regressions
passed. The optimization is inactive in the default desktop build.

```sh
for mode in baseline fast; do
  define=
  [ "$mode" = fast ] && define=-DRR_FAST_MEMORY_READS
  cc -O2 -fno-strict-aliasing -fwrapv -DRR_TRACE $define \
    -Iengine -Iinclude -Iraverace/include \
    raverace/tools/test_memory_reads.c raverace/src/rr_mem.c \
    -o "/tmp/test-memory-$mode"
  "/tmp/test-memory-$mode"
done
SDL_VIDEODRIVER=offscreen MESA_GLES_VERSION_OVERRIDE=2.0 \
  python3 raverace/tools/test_memory_game.py \
  /tmp/rr-memory-native/rr_gles2-baseline /tmp/rr-memory-native/rr_gles2-fast \
  /tmp/rr-headless-native/rr_headless raverace/extracted /tmp/rr-memory-validation
```

For ARM64 direct tests use
`~/dev/knulli-scarab/output/h700/host/bin/aarch64-buildroot-linux-gnu-gcc`
instead of `cc`, copy both executables and execute them on the device.

Physical frame-1700 baseline/fast captures also match byte for byte, including
the previous fixed-presentation reference SHA
`869d718ce31dc2aa86c858a24d3f2f67e438d9396ce7b9dd7156e84c17956eb4`.
All twenty benchmark runs match native full simulation JSON:
city SHA `b2a926aa3c0990c3ad27a512f7eda27abc9f22438b743487416e869a46821f1a`,
race SHA `ed0f4680d04ac73f7e642a2dc625856df545bc81bff0c26dfb932772a4e59ca3`.
Board digests are `6375e251` (city) and `b93b4e5a` (race), identical across every
phase/configuration. Ordered video digests are likewise identical. No game run
reported traps, unmapped accesses, ROM writes, DSP or sound faults.

## Deploy and reproduce on handheld

```sh
KNULLI_IP=192.168.1.69
for variant in baseline fast baseline-counts fast-counts; do
  scp "/tmp/rr-memory-arm64/rr_gles2-$variant" \
    "root@$KNULLI_IP:/userdata/roms/ports/rave-gles2/rr_gles2-memory-$variant"
done
scp raverace/tools/RaveRacer-Memory-Benchmark.sh \
  "root@$KNULLI_IP:/userdata/roms/ports/"
ssh "root@$KNULLI_IP" 'cd /userdata/roms/ports
chmod +x rave-gles2/rr_gles2-memory-* RaveRacer-Memory-Benchmark.sh
/etc/init.d/S31emulationstation stop
trap "/etc/init.d/S31emulationstation start >/tmp/rr-memory-es-restart.log 2>&1" EXIT
./RaveRacer-Memory-Benchmark.sh'
```

Use `-F /dev/null` with SSH/SCP if host configuration is broken. Original working
executables are preserved. The runner reuses existing ROMs and performs no
compilation/extraction. Both workloads use paced independent timing, skipped
geometry ON, textured/frame VBO, 64 MiB, coin 180, gas 200, steer 300:2304 and
center 700:2048. City measures 1400–1700 (301 ticks), race 1400–3600 (2201).
Each workload has three alternating unprofiled A/B pairs, one counter-only pair
and one detailed-stage/CPU-sampling pair. No performance run includes a capture,
per-read counters or detailed profiling. All variants share the lightweight
range wall-rate monitor and pre-existing renderer counters/reports.

For independent pixel comparison use `--test-present-every 3` and a forced
frame-1700 capture in separate baseline/fast correctness runs, as in the test
harness. Never use this deterministic presentation override in speed runs.

```sh
scp -r "root@$KNULLI_IP:/userdata/roms/ports/rave-gles2/memory-benchmark-*" /tmp/
python3 raverace/tools/summarise_memory_benchmark.py /tmp/RESULT_DIRECTORY
SDK="$HOME/dev/knulli-scarab/output/h700/host/bin/aarch64-buildroot-linux-gnu"
python3 raverace/tools/analyse_simulation_samples.py \
  /tmp/rr-memory-arm64/rr_gles2-fast \
  /tmp/RESULT_DIRECTORY/race-profile-1-fast-samples.tsv \
  "$SDK-nm" "$SDK-addr2line" 3600
```

Use the matching baseline ELF for its sample file. Analysis validates full state,
board/video digests, read widths/regions, callback savings and profile boundaries.
EmulationStation was confirmed restored after the actual run. For subsequent
physical play, with graphics ownership released, launch the separate fast binary
with `extracted --interactive --timing independent
--skip-geometry-on-dropped-frames on --texture-budget-mib 64 --vbo-upload frame`,
redirecting stderr/stdout to a new persistent log. START+SELECT exits. A new
operator-controlled session was not used as a matched performance comparison.

## Handheld results

### Unprofiled performance: medians of three runs

| Measured range / build | Simulation Hz | Render FPS | Rendered / skipped | Resyncs | Discarded lag ms |
| --- | ---: | ---: | ---: | ---: | ---: |
| City baseline | 57.102 | 15.602 | 82 / 219 | 8 | 226.563 |
| City fast | 58.615 | 20.532 | 106 / 195 | 2 | 111.257 |
| Race baseline | 58.656 | 31.074 | 1166 / 1035 | 26 | 785.861 |
| Race fast | 59.345 | 39.340 | 1460 / 741 | 9 | 358.325 |

Simulation ranges: city 56.925–57.270 baseline versus 58.304–58.725 fast;
race 58.636–58.767 versus 59.307–59.355. Render FPS ranges: city 15.366–15.697
versus 20.252–20.680; race 31.052–31.196 versus 39.015–39.372. All three A/B
pairs improve; the ranges do not overlap. Simulation gain is **2.65% city /
1.17% race**; rendering gain is **31.60% / 26.60%**.

This is independently paced wall-clock performance, not uncapped synthetic CPU
throughput. Saved simulation work permits more presentations, which consume
some recovered time; simulation Hz is also near its fixed target. Consequently
CPU-cost reduction and rendering-FPS gain are not expected to equal Hz gain.

**The game does not sustain 59.904059904 Hz yet.** City remains ~2.15% below
target, longer race ~0.93%, with nonzero resyncs/discarded lag. Do not extrapolate
this to every track, vehicle scene or longer interactive session.

### Single instrumented A/B pair per workload

Counters are OFF here; detailed stage timers/CPU sampling are ON. These are
per-run average/p95 over the identical emulated frames, **not three-run stage
medians**. Profiling and independent presentation choices can affect timing.

| Workload / build | Native 68K avg ms | Native 68K p95 ms | Simulation avg ms | Simulation p95 ms |
| --- | ---: | ---: | ---: | ---: |
| City baseline | 6.255 | 6.719 | 12.275 | 13.758 |
| City fast | 4.611 | 5.061 | 10.671 | 12.120 |
| Race baseline | 6.286 | 6.814 | 12.430 | 14.282 |
| Race fast | 4.702 | 5.201 | 10.912 | 12.815 |

Native 68K-inclusive-memory time saves **1.644 ms/tick city (26.3%)** and
**1.583 ms/tick race (25.2%)**. Overall simulation saves **1.604 ms (13.1%)**
and **1.517 ms (12.2%)**. Most other stages stay close: DSP/sound/mixing code
is unchanged; small differences are not separate optimisations. Main-thread CPU
averages are 12.225→10.643 ms city and 12.376→10.860 ms race, confirming the
saving is mostly CPU execution rather than removal of wall-clock waiting.

### Frequency and callback counts: separate counter-only runs

| Range | Total reads | WRAM fast hits | Hit rate | Baseline callbacks | Fast callbacks/fallbacks |
| --- | ---: | ---: | ---: | ---: | ---: |
| City | 13,859,324 | 13,216,101 | 95.36% | 13,859,324 | 643,223 |
| Race | 100,702,845 | 93,838,095 | 93.18% | 100,702,845 | 6,864,750 |

Every omitted callback is exactly one contained eligible WRAM read; callback
savings equal hits. All widths/starting-address counts match between builds.
City byte/word/long reads are 196,934 / 13,284,479 / 377,911; race counts are
1,423,594 / 96,590,044 / 2,689,207. About 95.9% of accesses are words. No other
width occurs in the measured game windows, although unit tests cover them.
ROM reads are 631,785 (4.56%) and 6,779,787 (6.73%). Polygon reads are 1,204 and
8,804; IO buckets 5,117 and 37,417; other buckets 5,117 and 38,742. Buckets
classify starting addresses, not proof of actual callback success or purity.
Counts exclude register-slot helpers (`R[]`) and host geometry's direct memory
reads; they describe calls to the real native game memory reader.

### CPU hotspot evidence

The existing optional SIGPROF sampler is ~100 Hz on this kernel, no `perf`
installation. Self samples are approximate and have no call stacks; see
[SIMULATION_PROFILE.md](SIMULATION_PROFILE.md) for quantisation/delivery limits.
Simulation-only samples exclude video/presentation/driver stages.

| Workload / build | Normal 68K-stage samples | Memory/map/I/O self samples | Share within 68K stage | Share of simulation samples |
| --- | ---: | ---: | ---: | ---: |
| City baseline | 184 | 114 | 61.96% | 31.58% |
| City fast | 136 | 46 | 33.82% | 14.60% |
| Race baseline | 1410 | 815 | 57.80% | 30.48% |
| Race fast | 1047 | 408 | 38.97% | 17.50% |

Groups include `rr_read`, `rr_write`, `io_read`, `io_write`, `region`; writes are
unchanged. City `io_read`/`region` samples drop 35/6→0/0; race drops 262/84→13/8.
Race `rr_read` samples drop 440→346. The mapper/callback hotspot improves;
`rr_read` itself remains significant. Do not equate sample-count changes with
exact time changes because the running durations and presentation mix differ.

Scaling memory-group share by the inclusive 68K timer suggests roughly
3.9→1.6 ms city and 3.6→1.8 ms race of self-time exposure. These are **rough
sample-derived estimates**, not per-access clock measurements or precise saved
milliseconds. They do not reconcile exactly with the measured total-stage
saving because sampling is coarse/biased. The reliable CPU-saving measurement
is the 1.58–1.64-ms inclusive-stage reduction; no expensive clock was added to
every read just to manufacture a memory-only number.

### Instrumentation overhead and memory

These are single diagnostic runs compared with each build's three-run
unprofiled median; variation limits precision. Negative percentages mean lower
throughput, not exact additive timer taxes.

| Workload/build | Counter-only sim Hz delta | Counter-only render FPS delta | Timers+sampling sim Hz delta | Timers+sampling render FPS delta |
| --- | ---: | ---: | ---: | ---: |
| City baseline | -4.07% | -12.52% | -0.99% | -1.28% |
| City fast | -0.42% | -12.17% | -0.62% | -1.97% |
| Race baseline | -1.73% | -23.90% | -0.18% | -2.58% |
| Race fast | -0.21% | -6.99% | -0.01% | -1.66% |

Counters distort performance materially, especially presentation selection.
Their exact deterministic frequencies are useful; their FPS is **not** used to
claim the speedup. Both scoring binaries compile the counters out. Sampling
runs also stay separate. Highest unprofiled RSS across builds was ~198.8 MiB
city and ~229.4 MiB race, with no allocation failures; no new fast-path buffer
is allocated. These are process high-water readings, not physical GPU accounting.

## Recommendation

The repeatable benefit justifies this small proven-region optimisation.
**Recommend enabling the WRAM fast path after review**; it remains OFF by default
in source/build controls for this evaluation, and no commit was made.

Do **not** expand to more regions yet: WRAM already covers 93–95% of reads and
ROM only 5–7%. A high hit rate alone was not the justification; matching states,
physical captures, callback invariants, inclusive CPU-stage reduction and three
unprofiled repeated gains provide the evidence.

The next focused memory experiment should inspect the existing fast reader's
common prologue/epilogue before adding address ranges. The fast ARM64 function
still creates a 112-byte frame, saves several registers and checks the stack
canary because it shares a function with complex fallback code. Samples land
in those paths (including `rr_read` return/prologue), and the reader remains
~15% of race simulation self samples. A separately tested fast-wrapper/cold-
fallback split could let the compiler emit a lighter fast leaf without removing
stack protections or changing memory semantics. This is a recommendation, not
an implemented second optimisation; no saving estimate is established.

If that experiment is not worthwhile, C352 inactive-voice processing remains
the next independent candidate; sound peripheral timer scanning follows it.
Neither has been changed. Additional-region support, read logging shortcuts,
write optimisation, register-layout rewrites and scheduler changes are outside
this experiment.

## Retained results

Handheld: `/userdata/roms/ports/rave-gles2/memory-benchmark-20261012-125502-11728/`.
Host copy: `/tmp/rr-memory-h700-results/memory-benchmark-20261012-125502-11728/`.
Includes raw logs, state, exit codes, manifest/binary SHA, per-phase timings,
samples/maps and generated `comparison.tsv`/symbol analyses. Correctness captures
remain at `rave-gles2/memory-check-{baseline,fast}.{ppm,json,log}` and locally in
`/tmp/rr-memory-h700-captures/`. Native final A/B captures/logs are in
`/tmp/rr-memory-validation-final/`. Working handheld executable and firmware
configuration were preserved; EmulationStation restoration was verified.

