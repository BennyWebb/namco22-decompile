# Native simulation profiling on RG40XX H

Profiling/investigation only. No clocks, budgets, device semantics, scheduler
policy, renderer, controller code or texture policy are optimised or replaced.
New profiling is optional and disabled by default.

## Architecture and boundaries

The main 68K-family program is statically translated C in `gen/rr_lifted.c`.
`g_rr_game->entry()` enters the native reset routine; translated calls use native
C calls/computed labels and a shadow return stack. This is not a conventional
fetch/decode CPU interpreter. Register/condition-code helpers in `lift_rt.h`
read/write big-endian byte slots in `R[]`; memory operations call `rr_read` and
`rr_write` at original addresses and widths. Those helpers perform optional
oracle checks, I/O callbacks, polygon-RAM handling, region mapping and byte
assembly. Device callbacks can have read side effects and must not be elided.

Instruction-budget exhaustion calls the cooperative `rr_tick` callback.
Sixteen slices make one video tick; each advances the master DSP's fixed share,
the translated C74 sound program, C352 samples, and 100-Hz serial pulses.
Vblank slices are split at VBEND. The translated master executes through
`c71_run`/`c71_step` and its native `xlat` callback, with timer/interrupt handling
and already-existing verified idle/spin fast-forwarding. No new fast-forwarding
is introduced. The slave enable gates host scene walking; there is no separately
running slave interpreter in this configuration.

Sound `snd_execute` runs the translated program to its fixed cycle deadline;
`m37710_service` maintains timers, ADC and IRQ recognition. C352 generation
continues even with audio output disabled because voice/sample state advances.
At each frame edge, DSP vblank, board IRQ/input state and link polling advance;
interrupt delivery invokes the actual translated game handlers. IRQ callbacks
that exhaust budget return to the handler without stepping devices again.

The GLES2 host performs mandatory PDP frame bookkeeping/direct-command
consumption once per simulated tick. With skipped geometry enabled, only
presented ticks decode/project/clip/sort host geometry and use the texture/GPU
backend. This remains unchanged. Target pacing is 59.904059904 Hz; the same
four-step catch-up/resync policy is used in every comparison.

### Exclusive stage accounting

| Stage | Boundary and inclusive dependencies |
| --- | --- |
| `translated68k_inclusive_memory` | Native execution between scheduler callbacks; includes register/flags, memory mapping and synchronous I/O callbacks; excludes separately timed IRQ delivery |
| `scheduler` | Callback glue, budgets, scripted inputs, counters/logging and frame-boundary housekeeping, excluding nested devices and presentation |
| `dsp_inclusive_pdp` | Master execution, timer/interrupt checks, point/polygon RAM access and synchronous PDP/render callbacks |
| `sound_inclusive_bus` | Translated C74 execution including peripheral service, bus callbacks and C352 register writes |
| `c352_mix` | Sample/voice generation and existing slice output/dump handling; no audio device enabled |
| `serial` | Existing serial pulse accumulator and DSP serial IRQ work |
| `board_io` | DSP vblank signalling, board vblank and link updates |
| `irq_inclusive_handlers` | IRQ selection/entry, actual translated handlers, acknowledgement and state restoration; nested scheduler callback glue is charged to scheduler instead |

Transitions use one monotonic timestamp; the previous stage accrues exclusive
elapsed time before switching. A simulation interval is sealed before host
preparation/pacing and resumed immediately after presentation returns. It includes
the preceding post-presentation frame-edge IRQ work; the terminal edge after the
last picture belongs to the next, unsealed interval (also how the existing
simulation timer is defined). All
categories sum to that interval's wall time, with a reported reconciliation
error. Interrupt/device callbacks inside native memory accesses remain in their
calling stage; no additional memory-access timers double-count them. IRQ handler
and dispatch costs cannot be reliably separated by the aggregate scope alone;
CPU samples identify the active function. Register/flags cost is largely inline
and cannot be assigned its own exact timer without intrusive instruction hooks.

PDP command callbacks are included in DSP, while mandatory host video latch/
consume is outside the simulation interval, in existing `[STAGES] video_latch`.
Palette and scene/GPU preparation are also outside simulation. This distinction
matches the previous simulation interval and prevents GPU blocking or deadline
waits being labelled translated CPU time. `[STAGES]` preserves rendered/skipped
cohorts for host preparation and video; new simulation distributions cover all
ticks in the measured range.

`CLOCK_THREAD_CPUTIME_ID` is read once at each interval endpoint. Comparing it
with monotonic wall time distinguishes time executing on the main thread from
preemption/blocking. These are interval totals, not exact CPU times for every
stage. Clock quantisation/endpoint overhead can affect very small differences.

## Options and instrumentation overhead

- `--profile-simulation`: exclusive stage statistics, average/median/p95/max.
- `--sample-simulation FILE.tsv`: additionally CPU-triggered native-PC sampling;
  implies simulation stage profiling.
- `--simulation-profile-from 1400`: selects the measured range and enables a
  lightweight wall-rate monitor even if detailed profiling is disabled.
- `--profile-stages`: existing rendering/device cohorts, independently optional.

The range monitor is common to every benchmark configuration: one monotonic
clock read at each completed simulation/presentation boundary and counters,
with no hot-loop logging. `off` disables both detailed simulation timers and
legacy `--profile-stages`. `stages` enables both; `sampled` adds SIGPROF.
Thus the timing comparison measures combined detailed instrumentation cost,
not only the newly added timer cost. Default launches with none of these options
do not start timers, allocate profile buffers or install signal handlers.

Stage storage is bounded to 8192 recorded ticks (786432 bytes), plus at most
65536 sort-scratch bytes at report time. CPU sampling reserves 65536 records
(1048576 bytes); no instruction-level allocation/logging occurs. Overflow is
reported explicitly. Reports occur only at exit, outside measured execution.
Median averages the two middle values; p95 uses nearest-rank quantiles. Each
aggregate cell in the final comparison is the median across three runs of the
specified statistic, not a pooled per-tick percentile.

## Sampling availability and limitations

The physical Knulli environment has no `perf` executable; its kernel reports
`perf_event_paranoid=3`. No package/tool is installed and that setting is not
changed. The fallback is an optional `ITIMER_PROF` timer requesting 5000 us,
`SIGPROF`/`ucontext` native PC capture and a bounded preallocated buffer. It
records only signals delivered to the main TID, preserves errno, uses no
stdio/allocation in the handler and saves process maps for symbolisation.

This is CPU-triggered sampling, not a hardware cycle counter or call-stack
profiler. The old kernel quantises timer delivery; achieved sampling frequency
need not be 200 Hz. The process CPU timer includes helper threads, and signals
delivered to other TIDs are discarded. Signal delivery can be delayed during
kernel/driver waits and periodic code can alias a fixed interval. Samples show
function **self** occupancy; inclusive parent costs must not be summed with
callee samples. External shared-library PCs are retained as external mappings;
no matching proprietary Mali symbols are available. Native PIE relocation is
handled using recorded maps; the ARM64 executable is ET_EXEC.

The host analyser uses the matching unstripped ELF, Buildroot `nm` and
`addr2line`. Release builds preserve function symbols but generally omit source
line debug information. Native instruction offsets/disassembly still permit
inspection of hot instructions. Sampling beyond the last measured frame is
excluded (exit-summary work belongs to the subsequent interval).

## Build, validate and deploy

```sh
cmake --build /tmp/rr-gles2-native --target rr_gles2 -j4
cmake --build /tmp/rr-gles2-arm64 --target rr_gles2 -j4
SDL_VIDEODRIVER=offscreen MESA_GLES_VERSION_OVERRIDE=2.0 \
  python3 raverace/tools/test_simulation_profile.py \
  /tmp/rr-gles2-native/rr_gles2 /tmp/rr-headless-native/rr_headless \
  raverace/extracted /tmp/rr-sim-validation
```

The existing configured ARM64 build uses the H700 Buildroot compiler/sysroot,
not Ubuntu's compiler. `-fno-strict-aliasing` and `-fwrapv` are retained; generated
68K/sound files retain their existing `-O1` and master DSP `-O2`. See [GLES2.md](GLES2.md)
for fresh CMake configuration commands.

```sh
KNULLI_IP=192.168.1.69
scp /tmp/rr-gles2-arm64/rr_gles2 \
  "root@$KNULLI_IP:/userdata/roms/ports/rave-gles2/rr_gles2-simulation-profile"
scp raverace/tools/RaveRacer-Simulation-Benchmark.sh \
  "root@$KNULLI_IP:/userdata/roms/ports/"
ssh "root@$KNULLI_IP" 'cd /userdata/roms/ports
chmod +x rave-gles2/rr_gles2-simulation-profile RaveRacer-Simulation-Benchmark.sh
/etc/init.d/S31emulationstation stop
trap "/etc/init.d/S31emulationstation start >/tmp/rr-sim-es-restart.log 2>&1" EXIT
./RaveRacer-Simulation-Benchmark.sh'
```

Use `ssh/scp -F /dev/null` if the host SSH configuration is broken. The separate
filename preserves both working interactive and skipped-geometry executables.
The runner reuses ROMs, performs three configurations in three varied orders
per workload, pins independent timing/skipped geometry/frame VBO/64 MiB, and
compares complete final JSON. City executes 1700 ticks and measures 1400–1700;
race executes 3600 and measures 1400–3600. Profiling-disabled runs still retain
the pre-existing render counters and five-second reports; this is not a build
with every existing diagnostic removed.

```sh
scp -r "root@$KNULLI_IP:/userdata/roms/ports/rave-gles2/simulation-profile-*" /tmp/
SDK="$HOME/dev/knulli-scarab/output/h700/host/bin/aarch64-buildroot-linux-gnu"
python3 raverace/tools/analyse_simulation_samples.py \
  /tmp/rr-gles2-arm64/rr_gles2 /tmp/RESULTS/race-1-sampled-samples.tsv \
  "$SDK-nm" "$SDK-addr2line" 3600
```

New files are `include/rr_sim_profile.h`, `src/rr_sim_profile.c`, benchmark,
sampling analyser and real-ROM regression scripts. Hooks are restricted to
GLES2 in `rr_main.c`, `rr_gles2.c`, `rr_gles2_profile.inc`, `rr_headless.inc`
and CMake. No generated code, memory/device implementation or renderer algorithm
is modified.

## Validation and measured results

Native profiling-off/stages/sampling runs produced identical full headless
state and byte-identical frame-600 screenshots. Exclusive stage sums reconciled
with interval wall time; record/sample bounds were not exceeded. The final
build passed the same checks in `/tmp/rr-sim-validation-final/`. Existing headless,
texture/four-mode, timing, fixed-clock and virtual-controller regressions passed.
Desktop and matching Buildroot ARM64 builds passed; the AArch64 ELF retains
Linux 4.9/glibc-compatible loader and SDL2/GLESv2/EGL/Mali dependencies, no libGL.

All eighteen physical handheld runs exited zero and matched native headless
references byte for byte: city SHA
`b2a926aa3c0990c3ad27a512f7eda27abc9f22438b743487416e869a46821f1a`, race SHA
`ed0f4680d04ac73f7e642a2dc625856df545bc81bff0c26dfb932772a4e59ca3`.
Registers, RAM/graphics hashes, IRQ counts, DSP steps and sound cycles matched;
no traps, unmapped accesses, ROM writes or DSP/sound faults occurred. Video-tick
walk/direct-consumption digests matched every configuration. This checks the
existing deterministic summary and video counters; it is not a new exhaustive
hash of every private peripheral field or a waveform comparison. No hardware
or generated-program implementation was changed.

The exact clock/catch-up policy is unchanged. Real-time presentation choices
can differ because instrumentation consumes wall time; matching presented counts
in independently paced runs is not a semantic invariant. Deterministic state
and native screenshot checks distinguish that effect from emulation changes.

## Physical handheld measurements

Three runs per configuration/workload, order rotated between trials, using the
same experimental executable. Tested binary SHA:
`dc328378e9b561a56bdaf10d61e9e473842ff980adc7a1fb31db5c5f31389d4d`.
The existing working executables were preserved and EmulationStation was
confirmed running after the restoration trap.

### Performance and overhead

Each cell below is the median of three runs; timing is the measured range,
excluding boot/menu warm-up.

| Workload / profiling | Steps | Rendered / skipped | Simulation Hz | Render FPS | Resyncs | Discarded lag ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| City, off | 301 | 82 / 219 | 56.991 | 15.526 | 8 | 246.963 |
| City, stages | 301 | 81 / 220 | 56.800 | 15.285 | 9 | 258.323 |
| City, sampled | 301 | 79 / 222 | 56.911 | 15.003 | 9 | 264.887 |
| Race, off | 2201 | 1161 / 1040 | 58.686 | 30.949 | 27 | 766.147 |
| Race, stages | 2201 | 1143 / 1058 | 58.639 | 30.469 | 27 | 793.631 |
| Race, sampled | 2201 | 1115 / 1086 | 58.488 | 29.678 | 31 | 896.181 |

Versus off, detailed stages changed median simulation throughput by -0.33%
(city) and -0.08% (race); stages plus sampling changed it by -0.14% and -0.34%.
City simulation ranges overlap: off 56.727–57.208, stages 56.661–57.250,
sampled 56.469–57.162 Hz. Race ranges are tighter: off 58.672–58.755,
stages 58.627–58.673, sampled 58.310–58.584 Hz. Do not interpret the sampled
city median beating stages as a negative instrumentation cost.

Rendered FPS is more sensitive to clock selection: stages were ~1.55% lower
than off in both workloads; sampled was ~3.37% lower (city) and ~4.11% lower
(race). These are observed comparison deltas, not exact additive timer taxes.
Use **off** for performance baselines, **stages** for quantitative attribution,
and **sampled** for hotspot discovery. Profiling is not free despite the small
simulation-Hz difference. The baseline rate monitor remains common to all cases.

Median measured simulation wall times from detailed stages were 12.284 ms
(city) and 12.416 ms (race); sampled values were 12.319 and 12.494 ms. Off does
not measure a detailed range simulation-time distribution by design. Its total
wall-rate denominator still includes all simulation, presentation, waits and
other host work, so FPS is not calculated by summing selected stage means.

### Simulation stage breakdown

Averages below pool all equally sized **stage-only** runs (903 city ticks and
6603 race ticks). Percentages divide each stage's pooled mean by pooled interval
wall mean; totals reconcile. This table excludes presentation/pacing/GPU work.

| Exclusive category | City average ms/tick | City % | Race average ms/tick | Race % |
| --- | ---: | ---: | ---: | ---: |
| Translated 68K, including memory/I/O calls | 6.257709 | 51.001 | 6.267566 | 50.543 |
| Scheduler glue | 0.030369 | 0.248 | 0.035830 | 0.289 |
| Master DSP, including PDP/device callbacks | 2.198229 | 17.916 | 2.261443 | 18.237 |
| Translated sound, including peripheral/bus service | 2.069614 | 16.868 | 2.127362 | 17.156 |
| C352 sample generation | 1.428916 | 11.646 | 1.409284 | 11.365 |
| Serial pulse | 0.004662 | 0.038 | 0.004734 | 0.038 |
| Board/vblank/link updates | 0.003553 | 0.029 | 0.003794 | 0.031 |
| IRQ dispatch and translated handlers | 0.276696 | 2.255 | 0.290330 | 2.341 |
| **Simulation wall total** | **12.269747** | **100** | **12.400342** | **100** |
| Main-thread CPU total (separate clock, not additive) | 12.162282 | 99.124 | 12.362235 | 99.693 |

Rounding can alter the displayed category sum in the last decimal place.
Per-run reconciliation maximum was below 0.01 us (reported 0.000000 us).
The mean wall-minus-thread-CPU gap is ~0.107 ms/city tick and ~0.038 ms/race
tick; most simulation wall cost is actual execution, not a GPU wait or large
scheduling delay. CPU time includes the profiler itself and kernel CPU work.

### Distributions and outliers

The median/p95 columns are the median of each run's per-tick statistic; maxima
are the worst observed across three stage-only runs. These are **not pooled
percentiles**. Raw per-run reports retain every stage's average/median/p95/max.

| Category | City typical median ms | City typical p95 ms | City worst max ms | Race typical median ms | Race typical p95 ms | Race worst max ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Translated 68K inclusive memory | 6.081 | 7.106 | 9.683 | 6.184 | 6.927 | 9.601 |
| Scheduler | 0.022 | 0.032 | 0.636 | 0.028 | 0.034 | 7.048 |
| DSP | 2.149 | 2.453 | 8.467 | 2.190 | 2.603 | 5.781 |
| Sound | 2.006 | 2.481 | 5.484 | 2.038 | 2.879 | 7.291 |
| C352 | 1.415 | 1.584 | 4.280 | 1.385 | 1.694 | 2.858 |
| IRQ inclusive handlers | 0.259 | 0.332 | 0.886 | 0.271 | 0.350 | 0.994 |
| Simulation wall | 11.952 | 14.429 | 19.325 | 12.256 | 14.339 | 22.206 |
| Simulation thread CPU | 11.933 | 13.440 | 16.932 | 12.234 | 14.283 | 17.008 |

Outliers matter: some intervals exceed the 16.693-ms arcade period even before
rendering. The rare 7.048-ms scheduler maximum is not evidence that its ordinary
~0.036-ms glue is a dominant CPU hotspot. Existing logging, preemption and
kernel delays can inflate wall scopes. The stored/report aggregates do not
identify the same tick across maxima, so **do not subtract unrelated maxima**
or assert a specific outlier's cause. Paired per-tick stall records would be a
separate diagnostic if those tails become the next priority.

### CPU hotspot samples

Pooled from the three sampled runs: 1089 simulation samples out of 1571 total
city samples, and 8054 out of 11037 total race samples. Host video/presentation
samples are excluded from the denominator below. The latter include Mali,
texture bake, host geometry and libc sorting and are not blamed on simulation.
Observed collection is roughly 100 samples per busy wall second, below the
requested 200-Hz interval on this kernel. Sample fractions are approximate,
subject to quantisation, aliasing and delivery bias; they are not exact timings.

| Active native function | City simulation self samples % | Race simulation self samples % |
| --- | ---: | ---: |
| `rr_read` | 14.33 | 15.68 |
| `io_read` | 10.93 | 8.91 |
| `region` | 2.94 | 2.83 |
| `rr_write` | 1.01 | 1.02 |
| `io_write` | 0.64 | 0.30 |
| `L_4000_at` | 8.17 | 7.85 |
| `rr_c25_exec` | 12.12 | 12.95 |
| `c71_run` | 2.48 | 2.64 |
| `c352_generate` | 11.85 | 11.14 |
| `m37710_service` | 3.86 | 4.02 |
| Sound `run_chunk` | 3.76 | 5.12 |
| Sound `m377_exec` | 3.31 | 3.33 |

The five memory/map/I/O functions account for **58.1% of normal 68K-stage
samples in the city and 55.7% in the race**, about 29.8%/28.7% of simulation
samples overall. Thus the largest measured stage is not equivalent to pure
translated game arithmetic or register handling. `L_4000_at` is the largest
individual sampled native 68K routine, but it is only ~8% of simulation samples;
its large translated body includes multiple original-PC regions. There is no
instruction-level claim that guest entry address 0x4000 itself consumes that
whole fraction.

Matching ARM64 assembly supports the memory-path diagnosis. `rr_read` samples
at 0x405e70/0x405e7c occur in the variable-width byte-assembly loop, including
a per-byte stack store. `io_read` samples at 0x4067ac/0x406880 hit range checking
and callback prologue/epilogue; even ordinary mapped reads call this hook before
region resolution. The code tests several device ranges and has stack-canary
and register-save overhead. These PCs belong to this exact ELF; re-symbolise
for another build. No PMU measurements establish branch/cache miss rates.

`m37710_service` samples at 0x9e8bb8 inspect timer-mask bits in an eight-slot
scan executed at instruction boundaries. It represents ~23.4% of sound-stage
samples, not all sound cost. DSP samples mainly hit native `rr_c25_exec`, with
additional native stepping/interrupt/timer machinery. C352 samples hit the
voice interpolation and four-channel accumulation loop, including 0x9e82b4/
0x9e82c4. The compiler **already emits NEON vector multiply/accumulate** here;
a generic suggestion to add SIMD is not supported by this evidence.

Largest process RSS high-water across runs was 230.8 MiB rounded up. No GPU or
CPU allocation failures occurred; the texture budget remained 64 MiB. RSS
includes renderer/driver memory and does not independently measure physical
GPU allocation or all system-memory pressure.

## Ranked next experiments — not implemented

1. **Mapped-memory read fast path / exact-width BE loads.** Highest-value target:
   memory/map/I/O self samples consume roughly 3.5–3.6 ms/tick when scaled by the
   measured normal 68K stage. This is an approximate sample-derived exposure,
   not a separate exact timer. Cortex-A53's in-order execution cannot hide the
   dependent byte assembly, repeated range branches, callback stack traffic and
   loads well. Try a narrowly bounded WRAM/ROM fast path plus exact-width loads,
   retaining the original fallback, side-effecting device reads, mirrors,
   overlapping mappings, width/bounds/ROM-write checks and journal/oracle paths.
   Mapping and endian semantics are necessary; repeatedly rejecting every I/O
   range for known RAM need not be. **Conditional estimate:** saving 20% of this
   exposure would save ~0.7 ms/tick, about 4% of the arcade period. No saving is
   measured yet. Medium implementation complexity/risk; verify native/ARM64
   states, IRQ/PDP, byte/word/long access boundaries and oracle/read side effects.

2. **C352 inactive-voice mixing work.** The mixer is an independently isolated
   ~1.41–1.43 ms/tick stage, and sampling confirms `c352_generate` itself.
   Every output sample scans 32 voices; an inactive voice keeps sample `s=0`
   but still executes phase/volume setup and four-channel accumulation.
   Evaluate a guard at the beginning of the inactive-voice path, preserving
   every active voice's counters, fetches, ramps, noise/loop/termination effects,
   outer sample count and output ordering. Do not remove sample advancement or
   mute-device execution. Compiler-generated SIMD already exists; the opportunity
   is avoiding redundant zero work. Active-voice fraction has **not** been
   measured, so no speedup can be predicted. **Conditional estimate:** a 20%
   mixer reduction saves ~0.28 ms/tick; the full-stage ceiling is ~1.4 ms.
   Low-to-medium implementation risk; require exact all-channel PCM and voice/
   noise/register-state comparisons, including a voice ending during a sample.

3. **Sound peripheral timer scan at instruction boundaries.** `m37710_service`
   accounts for ~23.4% of a ~2.13-ms sound stage: roughly 0.5 ms/tick exposure.
   ARM64 disassembly shows repeated eight-slot mask/next-event checks. Investigate
   iterating only active timer bits in original order, or a separately validated
   next-due fast check. Timer/ADC/IRQ recognition at the original instruction
   boundary is essential; returning early from all peripheral service just
   because a timer is not due would be unsafe. **Conditional estimate:** saving
   25% of that exposure gives ~0.12 ms/tick, with ~0.5 ms as the impossible-to-
   exceed full-service ceiling. Medium correctness risk: timer enable/reload,
   interrupt masks/priority, pending IRQ and cycle deadlines must match exactly.

DSP remains substantial (~2.26 ms/tick), but the existing timer-aware idle/spin
fast-forward already handles obvious waits. Sampling points to native execution,
not instruction fetching/decoding. More aggressive batching/fusion requires
retaining every instruction boundary/timer/IRQ/PDP effect and has higher risk
than the first experiment. Do not reduce step counts or treat this as an
interpreter replacement problem. Pure scheduler/board/serial overhead is far
smaller than the leading opportunities and should not be the next optimisation.

**Recommendation:** choose the memory-read experiment first, retain this
profiling-disabled baseline, and measure one small change before proceeding.
Neither these exposure estimates nor the measured ~58.7-Hz long-race average
establish full arcade speed in heavy city scenes.

## Retained artifacts

Handheld persistent directory:
`/userdata/roms/ports/rave-gles2/simulation-profile-20261012-114757-6900/`.
Retrieved copy:
`/tmp/rr-sim-h700-results/simulation-profile-20261012-114757-6900/`.
Contains per-run logs/state/exit, sampled native-PC TSVs/process maps,
manifest/binary SHA, and generated `comparison.tsv`, per-run symbol analyses
and pooled function tables. Reproduce the comparison on the host with:

```sh
python3 raverace/tools/summarise_simulation_profile.py /tmp/RESULT_DIRECTORY
```

All changes remain uncommitted. No proposed optimisation has been implemented.
