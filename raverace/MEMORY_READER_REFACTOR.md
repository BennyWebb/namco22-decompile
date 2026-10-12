# WRAM reader function-boundary experiment

This experiment compares the **existing WRAM fast reader** against a split
reader on the physical RG40XX H (Knulli Scarab). The original unoptimised reader
is retained as a third correctness reference. Both experiments remain disabled
by default. No generated game code, memory write path, scheduler, renderer,
texture cache, device implementation or emulated clock rate was changed.

## Existing overhead and compiler investigation

The H700 SDK compiler is GCC 13.3.0 (Buildroot `-gc2d7985b75`). Its wrapper injects
`-mcpu=cortex-a53 -mtune=cortex-a53 -fstack-protector-strong`; the Release build
uses `-O3` for `rr_mem.c`, retaining `-fno-strict-aliasing -fwrapv`. Translated 68K
and sound code retain their existing `-O1`, and DSP code its existing `-O2`.
There is no new LTO, whole-program inlining, global compiler flag or header-level
inline implementation.

Before refactoring, `rr_read` combined the WRAM checks and fixed-width loads with
callback dispatch, journaling, mapping, byte assembly, tracing and faults.
Fallback locals (`v`, mapping offsets/length and read-only flag) have their
addresses passed to callbacks and `region`. This triggers strong stack
protection. Registers holding arguments, mapping data and callback state must
survive fallback calls. GCC places the frame, common register saves and canary
setup before even the successful WRAM path; it does not shrink-wrap them away
on that path. `x25` is additionally saved/restored on fallback branches.

A first **plain C split without attributes failed to isolate that overhead**:
GCC inlined the fallback back into the entry at `-O3`, produced a 1196-byte entry
and retained the 112-byte frame/canary. That assembly justified applying
`__attribute__((noinline))` to the private fallback. No `cold`, `always_inline`,
branch hint or architecture-specific assembly is used: ordinary ROM/device
reads still reach the fallback 5–7% of the time, so declaring it universally
cold was unnecessary.

### Actual ARM64 assembly

These are measurements from matching H700 SDK Release executables, with read
counters OFF and tracing OFF (normal game configuration):

| Property | Original | Existing WRAM | Split WRAM |
|---|---:|---:|---:|
| Public `rr_read` text bytes | 892 | 1168 | 164 |
| Private fallback text bytes | — | — | 892 |
| Combined reader text bytes | 892 | 1168 | 1056 |
| Entry stack frame | 112 B | 112 B | 0 B |
| Fallback stack frame | same function | same function | 112 B |
| Common WRAM registers saved | n/a | x19–x24, x29/x30 | none |
| Successful WRAM calls | n/a | 0 | 0 |
| Fallback transfer | same function | same function | direct tail branch |
| Canary on address-taken fallback locals | yes | yes | yes |

Successful **contained reads with the standard non-null certified callback,
journaling/read logging OFF**, counting actual executed instructions including
`ret` and excluding caller instructions:

| Width | Existing | Split |
|---|---:|---:|
| Byte | 56 | 31 |
| Word | 60 | 35 |
| Long | 59 | 34 |

The representative word path drops from ten conditional branches, one
unconditional branch and `ret` to eight conditional branches and `ret`.
Counts describe instructions, not cycles; cache behaviour, branch prediction,
loads and driver/scheduler interference still require physical measurement.
With no callback installed the instruction path is slightly different. The
counter/trace builds also differ and are not used for headline performance.

Existing entry/return excerpt:

```asm
sub sp, sp, #0x70
stp x29, x30, [sp, #32]
add x29, sp, #0x20
stp x19, x20, [sp, #48]
stp x21, x22, [sp, #64]
stp x23, x24, [sp, #80]
ldr x3, [x2]                 // guard
str x3, [sp, #24]
// ... WRAM checks and load ...
ldr x3, [sp, #24]
ldr x2, [x1, #3456]          // guard
subs x3, x3, x2
b.ne __stack_chk_fail_path
ldp x29, x30, [sp, #32]
ldp x19, x20, [sp, #48]
ldp x21, x22, [sp, #64]
ldp x23, x24, [sp, #80]
add sp, sp, #0x70
ret
```

Split entry has only caller-saved `x0–x5`, no stack access, no saved link register
and no stack canary because it has no address-taken stack locals after GCC
lowers constant-size `memcpy` to loads. It begins directly with address/width
checks. Its final word and fallback paths are:

```asm
// failed eligibility: arguments w0/w1 intact, caller's LR intact
b rr_read_fallback
// eligible word: x0 is WRAM base, w3 is byte offset
ldrh w0, [x0, w3, uxtw]
rev16 w0, w0
and w0, w0, #0xffff
ret
```

Byte and long paths use `ldrb`, and `ldr`/`rev`, respectively. Odd addresses
remain supported. The private fallback still has its 112-byte frame,
callee-saved registers, guard load/store/check and `__stack_chk_fail` call.
Protection has not been disabled or weakened: GCC simply applies the unchanged
strong-protection rule separately to the two functions.

Record/reproduce complete assembly and compiler wrapper flags:

```sh
sdk="$HOME/dev/knulli-scarab/output/h700/host"
mkdir -p /tmp/rr-reader-assembly
for mode in original existing split; do
  "$sdk/bin/aarch64-buildroot-linux-gnu-objdump" -d --disassemble=rr_read \
    "/tmp/rr-reader-arm64/rr_gles2-reader-$mode" \
    > "/tmp/rr-reader-assembly/$mode-rr_read.txt"
  "$sdk/bin/aarch64-buildroot-linux-gnu-nm" -S \
    "/tmp/rr-reader-arm64/rr_gles2-reader-$mode" \
    > "/tmp/rr-reader-assembly/$mode-symbols.txt"
done
"$sdk/bin/aarch64-buildroot-linux-gnu-objdump" -d --disassemble=rr_read_fallback \
  /tmp/rr-reader-arm64/rr_gles2-reader-split > /tmp/rr-reader-assembly/split-fallback.txt
"$sdk/bin/aarch64-buildroot-linux-gnu-gcc" -### -c raverace/src/rr_mem.c \
  -o /tmp/no-build.o 2> /tmp/rr-reader-assembly/compiler-flags.txt
```

## Implementation and semantic boundaries

`RR_SPLIT_MEMORY_READER=ON` requires `RR_FAST_MEMORY_READS=ON`. All read checks,
WRAM/mirror selection and `memcpy`/byte-swap loads are the same as the existing
fast reader. If they fail, the entry tail-calls the unchanged original fallback
body; its original callback/probe/journal order, output accumulator, polygon
bus semantics, unsupported widths, mapping/boundary checks and fault behaviour
are retained. No additional memory region becomes eligible.

The fast path remains gated by certified plain-WRAM callbacks or no callback,
no journal/read logging, valid widths 1/2/4 and containment in one of the exact
128-KiB WRAM windows. A trace build also requires no active read probe. Generic
hook installation still revokes certification. Unknown hooks, including ones
that reject reads after changing the output, use the original fallback.
Read counters are recorded once at entry; hits/fallbacks/callbacks remain in
their existing places. No per-access option branch or timing was introduced.
There is no new allocation. Writes are byte-for-byte unchanged in C.

| Build | RR_FAST_MEMORY_READS | RR_SPLIT_MEMORY_READER |
|---|---|---|
| Original | OFF | OFF |
| Existing WRAM (primary baseline) | ON | OFF |
| Split WRAM | ON | ON |

`RR_MEMORY_READ_COUNTERS=OFF` is used for every benchmark in this experiment.
One exit-time `[MEM-READER] implementation=original|existing|split` line identifies
the variant. Existing diagnostic modes and profiling remain available.

Changed/added files:

- `CMakeLists.txt`: optional split definition only on `src/rr_mem.c`, invalid
  combination check; all source defaults remain OFF.
- `src/rr_mem.c`: guarded private fallback boundary and exit-time variant label.
- `tools/test_memory_game.py`: optional third executable, same strict state,
  ordered video/board digest and pixel checks.
- `tools/build_reader_variants.sh`: three variants using unchanged game objects
  in an existing Ninja tree; resets options OFF and rebuilds its default artifact.
- `tools/RaveRacer-Reader-Benchmark.sh`: three alternating A/B pairs plus one
  separate sampling/stage pair and original correctness run, per workload.
- `tools/summarise_reader_benchmark.py`: validates all 18 runs, faults, state,
  board/video digests and stage reconciliation before producing `comparison.tsv`.
- This report and the navigation link in `GLES2.md`.

## Build and correctness procedure

No packages, toolchains or ROMs are downloaded. Use the existing matching SDK:

```sh
cmake -G Ninja -S raverace -B /tmp/rr-memory-native-build \
  -DRR_GLES2=ON -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Release
raverace/tools/build_reader_variants.sh /tmp/rr-memory-native-build /tmp/rr-reader-native
cmake -G Ninja -S raverace -B /tmp/rr-memory-arm64-build \
  -DRR_GLES2=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/raverace/cmake/knulli-aarch64.cmake" \
  -DKNULLI_HOST="$HOME/dev/knulli-scarab/output/h700/host"
raverace/tools/build_reader_variants.sh /tmp/rr-memory-arm64-build /tmp/rr-reader-arm64
file /tmp/rr-reader-arm64/rr_gles2-reader-split
readelf -h -l -d /tmp/rr-reader-arm64/rr_gles2-reader-split
```

ELF is AArch64, interpreter `/lib/ld-linux-aarch64.so.1`, targeting Linux 4.9,
with SDL2/GLESv2/EGL/Mali, libc and libm; no desktop libGL dependency.
For a standalone build select `-DRR_FAST_MEMORY_READS=ON
-DRR_SPLIT_MEMORY_READER=ON -DRR_MEMORY_READ_COUNTERS=OFF` explicitly.

Direct tests use actual `rr_mem.c`; checker/probe seams alone are stubs:

```sh
for mode in original existing split; do
  defs=
  [ "$mode" = original ] || defs=-DRR_FAST_MEMORY_READS
  [ "$mode" != split ] || defs="$defs -DRR_SPLIT_MEMORY_READER"
  cc -O2 -fstack-protector-strong -fno-strict-aliasing -fwrapv -DRR_TRACE $defs \
    -Iengine -Iinclude -Iraverace/include \
    raverace/tools/test_memory_reads.c raverace/src/rr_mem.c \
    -o "/tmp/test-reader-native-$mode"
  "/tmp/test-reader-native-$mode"
done
```

Use the SDK compiler instead of `cc` for ARM64 direct tests; run those binaries
on the handheld. Compare stdout AND stderr (the fixture intentionally causes
faults). Coverage already includes byte/word/long, alignment, odd addresses,
mirrors, starts/ends/crossing faults, invalid aliases, arbitrary hooks,
certification revocation, rejecting hooks modifying output, future I/O effects,
active probes, journaling/replay/logging, polygon behaviour, ROM/shared/adjacent
mapping boundaries, widths 0/3/8/-1, fault counts/deduplication, live writes and
20,000 seeded reads. All three native and physical ARM64 variants passed with
identical transcripts. Split native ASan/UBSan passed (LeakSanitizer disabled
for the sandbox's ptrace limitation).

The actual game harness compares all three implementations to headless state:

```sh
SDL_VIDEODRIVER=offscreen MESA_GLES_VERSION_OVERRIDE=2.0 \
 python3 raverace/tools/test_memory_game.py \
 /tmp/rr-reader-native/rr_gles2-reader-original \
 /tmp/rr-reader-native/rr_gles2-reader-existing \
 /tmp/rr-headless-native/rr_headless raverace/extracted \
 /tmp/rr-reader-validation /tmp/rr-reader-native/rr_gles2-reader-split
```

Native and physical ARM64 captures at **600 and 1700** match exactly across all
three readers. Fixed `--test-present-every 3` ensures equivalent presentation
histories independent of wall clock; that option is not used for benchmarks.
Physical PPM SHA256: frame 600
`32597188c5e15cde67fbbb8eee1db3db8bbdc8cf9032429044a7bfa0b934eb31`,
frame 1700 `869d718ce31dc2aa86c858a24d3f2f67e438d9396ce7b9dd7156e84c17956eb4`.
The latter also matches the prior experiment's physical reference.

Existing desktop and headless builds, headless input/determinism/CLI checks,
coupled/independent state/pixel tests and optional stage/sampling regressions
passed. No traps, unmapped accesses, unexpected ROM writes, DSP or sound faults
occurred in game validation. Timing, controllers and render modes are unchanged.

## Physical benchmark procedure

Deploy separately; never overwrite the working `rave-gles2/rr_gles2`:

```sh
scp -F /dev/null /tmp/rr-reader-arm64/rr_gles2-reader-* \
 root@192.168.1.69:/userdata/roms/ports/rave-gles2/
scp -F /dev/null raverace/tools/RaveRacer-Reader-Benchmark.sh \
 root@192.168.1.69:/userdata/roms/ports/
ssh -F /dev/null root@192.168.1.69 'chmod +x /userdata/roms/ports/rave-gles2/rr_gles2-reader-*'
ssh -F /dev/null root@192.168.1.69 '
 trap "/etc/init.d/S31emulationstation start" EXIT
 /etc/init.d/S31emulationstation stop
 sh /userdata/roms/ports/RaveRacer-Reader-Benchmark.sh
'
```

With an authenticated SSH ControlMaster, append the appropriate `-S` (SSH) or
`-o ControlPath=...` (SCP); this session used `/tmp/rr-simulation-knulli-ssh`.
Logs default to `rave-gles2/reader-benchmark-DATE-TIME-PID/`. `RESULTS_DIR` can
select a new absolute directory; do not reuse an existing directory.

Per workload, original correctness run, then pairs ordered existing/split,
split/existing, existing/split, then one separate existing/split profiling pair.
Every invocation starts from reset with the same ROMs and scripted controls:

```sh
./rr_gles2-reader-split extracted --frames 3600 --coin 180 --gas 200 \
 --steer 300:2304 --steer 700:2048 --timing independent \
 --skip-geometry-on-dropped-frames on --texture-budget-mib 64 \
 --render-mode textured --vbo-upload frame --profile-from 1400 \
 --simulation-profile-from 1400 --profile-memory
```

City uses `--frames 1700`: inclusive measurement ranges contain 301 and 2201
ticks, respectively. `--profile-memory` only samples process memory at the
existing aggregate interval; it does not enable read counters or simulation
stage clocks. Headline runs have neither `--profile-stages` nor sampling.
Separate runs add `--profile-stages --sample-simulation PATH`; use these for
average/p95 simulation/native 68K times and CPU PCs, not headline rate medians.
No verbose memory-access logging, ROM extraction or recompilation occurs on
the device. All timing/catch-up policy and 59.904059904 Hz target remain intact.

Retrieve results and validate before analysing:

```sh
scp -F /dev/null -r root@192.168.1.69:/userdata/roms/ports/rave-gles2/reader-benchmark-DATE-TIME-PID /tmp/
python3 raverace/tools/summarise_reader_benchmark.py /tmp/reader-benchmark-DATE-TIME-PID
```

Optional second/third arguments supply actual native city/race JSON references
for cross-architecture comparison. Use the **matching** variant ELF to symbolise
each profiling run with `tools/analyse_simulation_samples.py` and SDK `nm` and
`addr2line`. Compare reader+fallback self samples as fractions of core and
simulation samples, not raw counts or total samples including presentations:
more rendered frames change the sample distribution. Sampling records current
PC only; there are no call stacks, so self-sample fractions are not inclusive
memory-access timing. Existing stage clocks measure inclusive 68K cost.

## Measured handheld results (2026-10-12)

All 18 runs completed successfully: nine per workload. Three unprofiled A/B
pairs are the headline performance evidence; separate single instrumented
pairs provide stage and sampling evidence. None of the latter numbers are
presented as three-run stage medians.

### Unprofiled performance medians

| Metric | Existing WRAM reader | Split reader | Change |
|---|---:|---:|---:|
| City simulation Hz | 58.441770 | 58.620717 | +0.306% |
| City render FPS | 20.426958 | 22.400234 | +9.660% |
| Race simulation Hz | 59.363733 | 59.477838 | +0.192% |
| Race render FPS | 39.467121 | 42.330629 | +7.255% |
| City rendered / skipped ticks | 105 / 196 | 115 / 186 | +10 presentations |
| Race rendered / skipped ticks | 1463 / 738 | 1566 / 635 | +103 presentations |
| City resyncs | 2 | 2 | unchanged median |
| Race resyncs | 9 | 6 | −3 |
| City discarded lag, ms | 115.235 | 103.612 | −11.623 |
| Race discarded lag, ms | 353.627 | 272.078 | −81.549 |
| Reader entry stack frame | 112 B | 0 B | −112 B |

The measured ranges execute **the same 301/2201 emulated ticks**, not equal wall
clock durations or equal presentation counts. Each pair improves both rates:

| Workload / pair | Existing sim Hz | Split sim Hz | Existing FPS | Split FPS |
|---|---:|---:|---:|---:|
| City 1 | 58.557280 | 58.606725 | 20.426958 | 22.391274 |
| City 2 | 58.441770 | 58.620717 | 20.386664 | 22.591373 |
| City 3 | 58.277447 | 58.630178 | 20.522955 | 22.400234 |
| Race 1 | 59.268714 | 59.477838 | 39.476572 | 42.318171 |
| Race 2 | 59.376030 | 59.438437 | 39.467121 | 42.425164 |
| Race 3 | 59.363733 | 59.495348 | 39.431975 | 42.330629 |

City FPS ranges 20.387–20.523 versus 22.391–22.591; race 39.432–39.477 versus
42.318–42.425. Those ranges do not overlap. Simulation-rate gains are small,
though positive for all pairs: city ranges 58.277–58.557 versus 58.607–58.630;
race 59.269–59.376 versus 59.438–59.495. Alternating order reduces, but does not
eliminate, thermal/background-load bias. These are six pairs on one device in
one session, not a claim of statistical certainty across all firmware/states.
The correct comparator is the **contemporaneously rebuilt existing WRAM
reader**, not the original unoptimised reader or previous session's medians.

### Separate simulation stage measurements

Single `--profile-stages` + CPU-sampling pair per workload; averages/p95 are
across its 301/2201 measured ticks. These are execution costs inside simulation,
not whole paced-frame wall times including host waits/presentation work.

| Stage metric, ms/tick | Existing | Split | Saving |
|---|---:|---:|---:|
| City native 68K inclusive mean | 4.614391 | 3.991888 | 0.622503 (13.49%) |
| City native 68K p95 | 5.060880 | 4.368682 | 0.692198 |
| City total simulation wall mean | 10.697791 | 10.078178 | 0.619613 (5.79%) |
| City total simulation wall p95 | 12.546325 | 11.101014 | 1.445311 |
| City simulation thread CPU mean | 10.623211 | 10.051623 | 0.571588 |
| Race native 68K inclusive mean | 4.708714 | 4.095644 | 0.613070 (13.02%) |
| Race native 68K p95 | 5.178786 | 4.504581 | 0.674205 |
| Race total simulation wall mean | 10.951857 | 10.334617 | 0.617240 (5.64%) |
| Race total simulation wall p95 | 12.888701 | 12.086486 | 0.802215 |
| Race simulation thread CPU mean | 10.907361 | 10.281397 | 0.625964 |

Race DSP 2.316472 → 2.316434 ms, sound 2.163697 → 2.158640 ms, C352 mixing
1.427164 → 1.428022 ms and IRQ handlers 0.290208 → 0.286262 ms are essentially
unchanged. Savings are concentrated in the reader's native 68K stage. Wall and
thread CPU reductions broadly agree; this is not merely less waiting reported
as faster execution. Stage sums reconcile with overall simulation, and all
profile/sample overflow checks passed.

The target remains **59.904059904 Hz**. The longer race **does not sustain it**:
59.477838 Hz is still 0.71% below target, with resyncs and discarded lag. City
remains 2.14% below target. CPU cost improves more than actual simulation rate
because pacing caps the latter near the arcade target and freed time also
permits more presentations. Rendering gains mean more frames are presented,
not an acceleration of the unchanged GPU rendering algorithm.

### CPU hotspot comparison

Samples use the existing thread CPU-time signal sampler with recorded stage
and native PC, symbolised using each matching ELF. The simulation denominator
is core + DSP + sound + C352 + IRQ + scheduler + serial/board; presentation and
video samples are excluded. An entry/fallback split moves work between symbols,
so **combine both reader symbols** for a fair comparison.

| Workload / reader | Simulation samples | Core samples | Reader + fallback self samples | % simulation | % core |
|---|---:|---:|---:|---:|---:|
| City existing | 312 | 133 | 53 | 16.99% | 39.85% |
| City split | 300 | 128 | 22 + 5 | 9.00% | 21.09% |
| Race existing | 2334 | 982 | 346 | 14.82% | 35.23% |
| Race split | 2180 | 885 | 173 + 44 | 9.95% | 24.52% |

Small city sample counts are coarse evidence; the longer race and measured
stage clocks carry more weight. Raw count reductions alone would be invalid:
CPU durations differ, and split runs present more frames. The table normalises
to core/simulation distributions. This is self-PC sampling, not inclusive
call-stack timing, and does not directly measure nanoseconds per read.

Existing reader PCs attributable to the entry/setup region (`+0x00..+0x38`) and
return/guard/restore region (`+0x104..+0x130`) accounted for 26 city core samples
and 173 race core samples. For the race that is 7.41% of all simulation samples.
These regions include setup instructions as well as stack accesses; do not
attribute all their time uniquely to stack protection. The new entry cannot
sample register-save/restore or canary instructions because those instructions
are absent there. Remaining split-reader samples represent eligibility checks,
loads/swaps/return and, in the separate fallback, its unchanged complex work.
No additional memory optimisation was implemented.

### Profiling overhead and memory

Compared with the three-run unprofiled medians, the single instrumented runs
showed rendering-rate changes of −3.90%/−2.69% (city existing/split) and
−2.59%/−2.10% (race). Simulation-rate changes were +0.11%/−0.07% city and
−0.04%/−0.15% race. These differences mix instrumentation and run variability;
they are an overhead indication, not a precise paired repeated overhead test.
Consequently headline comparisons use unprofiled runs only. Read counters were
OFF throughout this experiment.

Largest process RSS high-water values in unprofiled trials: city 198.73/198.87
MiB, race 229.46/229.93 MiB (existing/split). The reader itself allocates nothing;
small RSS differences accompany different presentation/cache histories. Texture
capacity/eviction behaviour is unchanged at 64 MiB, with no allocation failures.
No cache, texture, VBO or draw-call optimisation was made.

## Correctness results and retained evidence

All eighteen benchmark JSON states matched actual native city/race references,
with zero traps/unmapped accesses/ROM writes/DSP faults/sound faults. Board and
ordered video bookkeeping digests matched all three implementations and all
phases. Full state SHA256 (identical to the previous WRAM experiment):

- City: `b2a926aa3c0990c3ad27a512f7eda27abc9f22438b743487416e869a46821f1a`;
  board `6375e251`.
- Race: `ed0f4680d04ac73f7e642a2dc625856df545bc81bff0c26dfb932772a4e59ca3`;
  board `b93b4e5a`.

Default and split native headless input/determinism/CLI regressions passed.
The rebuilt matching-SDK headless executable also passed the full existing
Python regression on the actual handheld. Native split timing, profiling and
texture/diagnostic-mode regressions passed; no rendering code was changed.
Physical frames 600/1700 and their full JSON match native references and all
three physical variants. Independent timing chooses different presentation
histories in performance runs, so those are not pixel comparisons; fixed-stride
capture tests provide the equivalent-frame rendering validation.

Retained physical results:
`/userdata/roms/ports/rave-gles2/reader-benchmark-20261012-140505-16647/`.
Complete local copy:
`/tmp/rr-reader-h700-results/reader-benchmark-20261012-140505-16647/`, including
raw logs/state/exit codes, manifest executable SHA256, CPU samples/maps,
symbol analyses and generated `comparison.tsv`.
Physical captures: `rave-gles2/reader-check-{600,1700}-{original,existing,split}.{ppm,json,log}`;
local copy `/tmp/rr-reader-h700-captures/`. Native captures/logs:
`/tmp/rr-reader-validation/`; assembly `/tmp/rr-reader-assembly/`.

The installed working executable's SHA256 matched before/after testing.
EmulationStation was restored and its running process verified. Firmware/system
files were not modified. No changes were committed. Experimental source options
and reusable native/ARM64 Ninja trees finish OFF.

## Recommendation

**Retain the split implementation and adopt it with the WRAM fast path after
review.** It remains disabled by default for this experiment. The added code is
a private function boundary plus a justified `noinline`, with the original
fallback body untouched. Smaller actual assembly, approximately 0.62 ms/tick
core savings, normalised CPU hotspots, repeated unprofiled FPS gains and strict
state/pixel validation justify that small complexity. The result is not merely
within rendering-rate measurement noise, although the near-target simulation
Hz improvement is modest.

Do not expand memory regions or tune compiler flags in this task. Remaining
reader cost is still measurable, but WRAM already covers 93–95% of calls and
now has a stackless entry. The next controlled optimisation should investigate
**C352 inactive-voice processing**: mixing consumes about 1.43 ms/tick (~14% of
post-refactor simulation time), including in this no-audio-output milestone.
Audit key-on/voice, envelope/address and status semantics before skipping any
voice work; sample generation must remain deterministic. DSP/sound stages are
also significant (~4.48 ms combined) but require separate evidence and carry
greater behavioural risk. None of those optimisations was implemented here.
