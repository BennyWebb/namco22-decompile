# C352 entry-inactive voice experiment

This is an optional optimisation of the shared `engine/c352.c` mixer, tested
with actual Rave Racer translated sound/game execution. It introduces no new
sound decoder, audio device, timing policy, rendering or memory-access change.
`RR_C352_INACTIVE_FASTPATH` and detailed counters remain **OFF by default**.

## Architecture and safe condition

`c352_t` has 32 `c352_voice_t` objects (32 bytes each), a 256-entry mu-law table,
a shared noise LFSR, control register and immutable sample-ROM pointer/size.
Each voice contains a 32-bit position/counter, current/previous signed samples,
four 8-bit current volume ramps, two 16-bit volume target registers, 16-bit
frequency/flags and bank/start/end/loop registers. Register numbering is explicit,
not dependent on struct layout: eight words per voice at offsets 0..255, global
control at `0x200`, key command execution at `0x202`. Partial register writes
combine masks; command execution requires a full `0xffff` mask.

The translated C74 sound program writes the existing board bus. Its C352
shadow assembles a word on the odd byte and calls `rr_audio_c352_write`; this
still uses the original `c352_write`. Key-on/key-off bits are pending commands,
not inherently audible or active. On full-word execute, each voice processes
KEYON first: set bank/start position, clear samples, set counter `0xffff`, set
BUSY, clear KEYON/LOOPHIST and reset volume ramps. KEYOFF then clears BUSY and
KEYOFF and restores counter `0xffff`. With both bits set, on then off remains
the original order. Direct masked writes to flags can also change BUSY.

For **each generated output sample**, the baseline loops over all 32 voices:

1. Start local `s=0`. If BUSY, add frequency to the counter; its carry fetches
   a sample. Linear signed ROM bytes or the original mu-law table supply samples;
   noise advances the shared LFSR instead of a ROM position.
2. Fetch updates previous/current sample and handles normal/reverse, ordinary
   loop, ping-pong (LDIR), linked-bank loops and natural end. Natural end sets
   KEYOFF, clears BUSY and sets current sample to zero.
3. Counter-bit changes ramp each current volume by one towards its target.
   Keep the low 16 counter bits; apply the original interpolation unless FILTER.
4. Apply phase flags and four current volumes; accumulate four signed channels.
   Output shifts/narrowing retain the original wrap behaviour, not new clipping.

**Critical edge:** when fetch clears BUSY, the already-entered active body still
updates counter/ramps and may output nonzero interpolated previous-sample audio.
A post-fetch inactive test would be incorrect. The targeted fixture proves this
case produces nonzero PCM and compares it between implementations.

| Voice condition at sample entry | Existing effects | Optimisation |
|---|---|---|
| BUSY clear, including stale samples/volumes/phase/noise bits | No position, counter, sample, ramp, flag or LFSR update; four zero contributions | Skip only that voice's remaining body |
| BUSY clear with pending KEYON/KEYOFF | Same; command awaits an explicit register execute | Skip, preserving pending bits and all state |
| BUSY set with zero target/current volume or zero source | Playback/loop/status/ramps/noise may advance despite silence | Original body |
| BUSY set with pending KEYOFF | Plays until original execute clears BUSY | Original body |
| BUSY set but natural end occurs now | Transition plus possible interpolated final output | Complete original body |

There are no concurrent chip writes during `c352_generate`: translated sound
execution and mixing are sequential on the simulation thread. Desktop SDL audio
consumes the existing ring and does not mutate voices. This does not justify
skipping active silence, freezing noise, advancing time differently or dropping
an entire interval. The new code merely `continue`s when BUSY is clear **at
entry**, inside the existing sample/voice loop. Voice order, all active arithmetic,
all output stores, register semantics and number of samples are unchanged.

### Rave Racer clocks and muted output

There are still 16 sound/mix slices per emulated tick. Rave Racer loads the
original four 1-MiB wave lanes in their existing mapping into a 4-MiB ROM image.
`RR_HEADLESS` (also used by GLES2) excludes opening an SDL audio device: `push_out`
is a no-op, but **all original samples/status/voice updates are generated**.
Muting host output is not a reason to suppress the chip.

The existing audio accumulator is intentionally untouched: `RATE_NUM=85333333`
and `SLICE_DEN=59906*16`, with chunks at most 256 output frames. Its legacy
fixed-point approximation is distinct from the scheduler's 59.904059904 Hz
pacing target; this task does not correct or replace either. All sample totals
and accumulator remainders must match the baseline exactly.

## Measurement before implementation

The pre-guard ARM64 assembly confirmed that GCC 13.3 still loaded phase/volume
state and executed four-channel SIMD zero accumulation for BUSY-clear voices.
This was not already eliminated efficiently. Baseline optional counters then
measured actual native scripted city/race execution; physical baseline counts
and four-channel PCM/voice-state checks matched native results **before** the
performance experiment. Only after finding substantial inactive work was the
conditional guard implemented.

Diagnostics are compiled into `engine/c352.c` only with
`RR_C352_VOICE_COUNTERS=ON` (`C352_VOICE_COUNTERS` for standalone tests). They
live outside `c352_t`, so they add no emulated state or change its layout.
The process-wide profile serves this game's single chip. Range selection occurs
outside the mixer, including before register writes, so transitions in the
first measured sound slice are counted. It does not use per-voice clocks or
per-sample logging.

Counters include output frames and interval calls, BUSY-at-entry processing
iterations, 33-bin active-voice histograms at interval entry and per sample,
activation/deactivation edges, executed key commands and natural ends. Direct
flag-register edges and chip reset deactivations are counted too. Natural end
and KEYOFF execution are separate events: not every KEYOFF execute clears a
currently BUSY voice, and flag writes can end BUSY without that command.

`iterations=samples*32` is the number of voice visits in both algorithms.
`mix_iterations` counts bodies reaching phase/volume accumulation: every visit
in baseline, only entry-active visits in the fast path. `inactive` counts
provably unnecessary zero work. `active_zero_source` counts entry-active visits
whose computed `s` is zero or all four current volumes are zero, even though
playback state still needed processing. `zero_source_iterations` combines those
with inactive visits: a **conservative lower bound on zero PCM contributions**,
not a claim to count every case where fixed-point rounding yields zero. Active
zero-source processing is retained. Histograms distinguish distributions from
means; no active-list or interval-level skip was introduced.

### Actual workload distributions

Counts for inclusive simulation ranges 1400..1700 (301 ticks) and 1400..3600
(2201 ticks); baseline and fast diagnostics are compared separately from
performance, with all previous validated optimisations enabled:

| Measure | City | Race |
|---|---:|---:|
| Mixing intervals | 4816 | 35216 |
| Four-channel output frames | 428761 | 3135223 |
| Voice visits | 13720352 | 100327136 |
| Entry-active iterations | 7915366 | 55093238 |
| Inactive / eliminated accumulation bodies | 5804986 | 45233898 |
| Inactive fraction | 42.31% | 45.09% |
| Active zero-source iterations, retained | 1462195 | 6993827 |
| Mean active voices/sample | 18.461 | 17.572 |
| Median / p95 active voices/sample | 20 / 21 | 17 / 23 |
| Min / max active voices/sample | 13 / 23 | 9 / 26 |
| Mean active voices/interval entry | 18.469 | 17.576 |
| Activation / deactivation edges | 96 / 98 | 1312 / 1310 |
| Executed KEYON / KEYOFF commands | 96 / 57 | 1312 / 198 |
| Natural ends | 61 | 217 |

Full 33-bin histograms remain in the activity logs. Interval-entry median/p95
and min/max match the per-sample values here, but their counts and means are
not conflated. Silence is common in active voices too; it is not used as a
skip condition. The maximum saving is limited: even the fast version must
visit/check all 32 voices, and roughly 55–58% still need complete processing.

## Optional implementation and build matrix

`RR_C352_INACTIVE_FASTPATH=ON` adds just the entry BUSY guard in the shared
mixer (`C352_INACTIVE_FASTPATH` for direct compilation). It is compiled out
entirely by default. No global compiler flag, aliasing rule, stack protection,
clock, sound CPU, timer, memory reader, geometry or texture-cache policy changed.
The shared engine remains the only C352 implementation; no game-local fork.

| Artifact suffix | Inactive fast path | Voice counters | WRAM + split reader |
|---|---|---|---|
| baseline | OFF | OFF | ON |
| fast | ON | OFF | ON |
| baseline-counts | OFF | ON | ON |
| fast-counts | ON | ON | ON |

The new builder selects exactly that matrix and resets its reusable tree's
experimental flags OFF afterwards. Retain `-fno-strict-aliasing -fwrapv` and
existing translated-code optimisation levels. An exit-time `[C352]` line
identifies fast-path/counter status; no additional logging runs in the sample
loop. Original reader/coupled/renderer diagnostic modes remain available.

Actual H700 GCC assembly changes the inactive visit from 27 instructions to
six (counting both flag loads, BUSY branch, pointer advance and loop compare/
branch, excluding per-sample setup/output). Its branch now jumps directly to
voice-loop advancement, bypassing volume/phase and SIMD multiplication. Active
code still uses compiler-generated SIMD; no hand SIMD or assembly was added.
Function layout/register allocation change as GCC lowers the new control flow;
strict PCM/state tests, not instruction similarity alone, validate correctness.

## Audio-correctness tests

`tools/test_c352.c` links the actual `engine/c352.c` into baseline/fast test
executables. Its transcript includes every PCM channel and canonical explicit
voice fields, all readable voice/control registers, PRNG, mu-law table and ROM
size after each operation (no pointers or padding). PCM serialization is
injective over the exact s16 bits. All 4615 state snapshots and PCM transcripts
match byte for byte on native and physical ARM64, and across architectures:
SHA256 `49e64a6eb10f16deed1ce74702f7f3a7090988025a8f1e0107c25998db24d235`.

Covered behaviours include:

- All inactive, stale nonzero samples/volumes/phase/noise flags, pending commands:
  output is zero and the entire initialized chip remains unchanged.
- Zero/negative output count and buffer sentinels; one voice and all 32 voices.
- Rapid KEYON/KEYOFF, simultaneous commands, partial execute ignored, masked
  register writes immediately before mixing and at buffer boundaries.
- Ordinary, reverse, ping-pong and linked-bank loops; mu-law, filtered samples,
  phase inversions, multiple shared-noise voices and overflowing final mixing.
- Voice ending during a buffer and nonzero final interpolated PCM after BUSY
  clears; next sample becomes inactive without stale contribution.
- Active zero-volume sample playback and noise still advancing positions/LFSR;
  zero pitch, changing volume ramps, out-of-ROM and 24-bit address masking.
- 3000 deterministic random/masked register operations with variable buffers.

For actual game execution, the existing WAV dump is front stereo only and the
headless environment intentionally clears `RR_AUDIODUMP`. Rather than silently
bypassing that isolation, explicit `--audio-checksum-from FRAME` captures an
ordered FNV-1a64 of **all four little-endian s16 channels**, plus a separate
chained digest of every mutable voice/global field and accumulator after each
mixing interval, including chunk size. A final state digest and sample/interval
counts are reported too. The immutable ROM/table are unchanged; table contents
are checked directly in fixtures. This is a stream checksum comparison, not a
claim to retain full real-game raw PCM. The legacy desktop WAV mechanism is
unchanged.

`tools/test_c352_game.py` compares baseline/fast against actual headless state
at 600, 1700 and 3600, checking all PCM/state history **since reset**, equivalent
fixed-stride presentation pixels, board and video digests. All passed. Physical
frame 600 passed PCM/state/pixel equivalence; its PPM SHA256 also matches the
prior reference `32597188c5e15cde67fbbb8eee1db3db8bbdc8cf9032429044a7bfa0b934eb31`.
Benchmark activity phases separately check real audio for each measured range
and fail before performance runs if audio/state differs.

Measured-range native baseline references (physical baseline matched before
implementing the guard):

| Range | PCM FNV64 | Interval-state FNV64 | Final state FNV64 | Accumulator |
|---|---|---|---|---:|
| City | 99356441d396da2b | 984ffed62141c596 | 1140170e7e4791ec | 540384 |
| Race | 7c1541d9ca4fe871 | 1077d1458ef35d14 | c80bf3c03304cbeb | 862432 |

## Reproduction commands

From the repository, using existing Ninja trees and matching SDK:

```sh
cmake -G Ninja -S raverace -B /tmp/rr-memory-native-build \
 -DRR_GLES2=ON -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Release
raverace/tools/build_c352_variants.sh /tmp/rr-memory-native-build /tmp/rr-c352-native
cmake -G Ninja -S raverace -B /tmp/rr-memory-arm64-build \
 -DRR_GLES2=ON -DCMAKE_BUILD_TYPE=Release \
 -DCMAKE_TOOLCHAIN_FILE="$PWD/raverace/cmake/knulli-aarch64.cmake" \
 -DKNULLI_HOST="$HOME/dev/knulli-scarab/output/h700/host"
raverace/tools/build_c352_variants.sh /tmp/rr-memory-arm64-build /tmp/rr-c352-arm64
file /tmp/rr-c352-arm64/rr_gles2-c352-fast
readelf -h -l -d /tmp/rr-c352-arm64/rr_gles2-c352-fast
```

ELF is AArch64 for Linux 4.9, using `/lib/ld-linux-aarch64.so.1` and existing
SDL2/GLESv2/EGL/Mali/libc/libm. No desktop libGL dependency. No SDK installation,
package changes, ROM extraction or generation changes are needed.

Direct fixture tests:

```sh
for mode in baseline fast; do
 defs=
 [ "$mode" != fast ] || defs=-DC352_INACTIVE_FASTPATH
 cc -O3 -fno-strict-aliasing -fwrapv $defs -Iengine \
  raverace/tools/test_c352.c engine/c352.c -o "/tmp/test-c352-$mode"
 "/tmp/test-c352-$mode" "/tmp/test-c352-$mode.bin"
done
cmp /tmp/test-c352-baseline.bin /tmp/test-c352-fast.bin
```

Use `~/dev/knulli-scarab/output/h700/host/bin/aarch64-buildroot-linux-gnu-gcc`
in place of `cc` for ARM64, copy the two tests and execute/compare on the device.
Real native game checks:

```sh
SDL_VIDEODRIVER=offscreen MESA_GLES_VERSION_OVERRIDE=2.0 \
 python3 raverace/tools/test_c352_game.py \
 /tmp/rr-c352-native/rr_gles2-c352-baseline /tmp/rr-c352-native/rr_gles2-c352-fast \
 /tmp/rr-headless-native/rr_headless raverace/extracted /tmp/rr-c352-game-validation
```

Deploy experimental files alongside, preserving the working executable:

```sh
scp -F /dev/null /tmp/rr-c352-arm64/rr_gles2-c352-* \
 root@192.168.1.69:/userdata/roms/ports/rave-gles2/
scp -F /dev/null raverace/tools/RaveRacer-C352-Benchmark.sh \
 root@192.168.1.69:/userdata/roms/ports/
ssh -F /dev/null root@192.168.1.69 '
 chmod +x /userdata/roms/ports/rave-gles2/rr_gles2-c352-*
 trap "/etc/init.d/S31emulationstation start" EXIT
 /etc/init.d/S31emulationstation stop
 sh /userdata/roms/ports/RaveRacer-C352-Benchmark.sh
'
```

This session uses authenticated SSH ControlMaster
`/tmp/rr-simulation-knulli-ssh`; append `-S` for SSH or `-o ControlPath=...` for
SCP as appropriate. Script defaults to a new
`rave-gles2/c352-benchmark-DATE-TIME-PID/`; `RESULTS_DIR` can choose a new absolute
path and `RR_C352_APP_DIR` an alternate app directory. Existing paths are
rejected. No automatic recompilation or ROM extraction takes place.

All runs select independent timing, skipped geometry ON, textured rendering,
frame VBO and 64-MiB texture cache, and the same reset/scripted inputs:

```sh
./rr_gles2-c352-fast extracted --frames 3600 --coin 180 --gas 200 \
 --steer 300:2304 --steer 700:2048 --timing independent \
 --skip-geometry-on-dropped-frames on --texture-budget-mib 64 \
 --render-mode textured --vbo-upload frame --profile-from 1400 \
 --simulation-profile-from 1400 --profile-memory
```

City substitutes `--frames 1700`. Per workload: an activity/audio pair using
counter builds, three alternating uninstrumented baseline/fast pairs (pair 2
reverses order), then one separate stage/CPU-sampling pair using counter-OFF
builds. Activity adds `--c352-activity-from 1400 --audio-checksum-from 1400`;
profiling instead adds `--profile-stages --sample-simulation PATH`. Neither is
present in headline runs. `--profile-memory` uses existing aggregate process
memory sampling, not per-access or per-sample instrumentation. The profiler
reconciles exclusive stages with simulation total and records thread CPU time;
its measurements are not interpreted as whole paced-frame duration.

Retrieve and validate all state/audio/counter invariants before reporting:

```sh
scp -F /dev/null -r \
 root@192.168.1.69:/userdata/roms/ports/rave-gles2/c352-benchmark-DATE-TIME-PID /tmp/
python3 raverace/tools/summarise_c352_benchmark.py /tmp/c352-benchmark-DATE-TIME-PID
```

Optional second/third summary arguments supply native city/race JSON references.
Symbolise profiling samples with `tools/analyse_simulation_samples.py` and the
matching baseline/fast ELF plus SDK `nm`/`addr2line`. Normalise self samples to
simulation or mixer-stage samples; more presentations change total sample
proportions. No call stacks are recorded, so self samples are not inclusive
per-voice timings. Do not score activity builds: counters and PCM/state hashing
add work and change presentation opportunities.

## Physical performance results (2026-10-12)

All twenty benchmark runs completed with exit 0. Voice activity/audio phases
matched native references and each other; all phases matched the same native
full game state and board/video digests with no traps, unmapped accesses,
unexpected ROM writes, DSP or sound faults.

### Headline medians: three uninstrumented alternating pairs

| Metric | Baseline C352 | Optimised C352 | Change |
|---|---:|---:|---:|
| City simulation Hz | 58.574189 | 58.521439 | −0.090% |
| City render FPS | 22.744668 | 23.690032 | +4.156% |
| Race simulation Hz | 59.467273 | 59.504322 | +0.062% |
| Race render FPS | 42.851020 | 43.634701 | +1.829% |
| City rendered / skipped ticks | 117 / 184 | 122 / 179 | +5 presentations |
| Race rendered / skipped ticks | 1586 / 615 | 1614 / 587 | +28 presentations |
| City resynchronisations | 2 | 2 | unchanged median |
| Race resynchronisations | 6 | 5 | −1 |
| City discarded lag, ms | 102.599 | 109.615 | +7.016 |
| Race discarded lag, ms | 285.675 | 259.251 | −26.424 |

Each measured range executes exactly 301/2201 ticks. Individual pairs, preserving
all results including contrary simulation-rate changes:

| Workload / pair | Baseline Hz | Fast Hz | Baseline FPS | Fast FPS |
|---|---:|---:|---:|---:|
| City 1 | 58.574189 | 58.521439 | 22.768040 | 23.719653 |
| City 2 | 58.514060 | 58.651975 | 22.744668 | 23.577704 |
| City 3 | 58.781194 | 58.448359 | 22.457931 | 23.690032 |
| Race 1 | 59.467273 | 59.409342 | 42.851020 | 43.268139 |
| Race 2 | 59.488343 | 59.504322 | 42.893231 | 43.634701 |
| Race 3 | 59.432546 | 59.506734 | 42.744989 | 43.960904 |

FPS increases in every pair. City ranges 22.458–22.768 versus 23.578–23.720 and
race ranges 42.745–42.893 versus 43.268–43.961 do not overlap in this session.
Simulation Hz does **not** improve consistently: city falls in pairs 1/3 and
race in pair 1; ranges overlap (city 58.514–58.781 versus 58.448–58.652, race
59.433–59.488 versus 59.409–59.507). Resync/lag improvements are not universal
either. Do not present the tiny median Hz difference as a demonstrated
throughput improvement. Alternation reduces bias, but does not eliminate
thermal/background/driver variation; three pairs on one device are limited
repeatability evidence. Compare these contemporaneous builds, not previous
session's WRAM-refactor medians.

The race **does not reach 59.904059904 Hz**: the fast median remains 0.67% below
target and still resynchronises/discards lag. City remains 2.31% below target.
Saving guest CPU time does not guarantee more simulation Hz: the clock caps
simulation near the arcade rate, and extra presentations consume some freed
time and may block in the unchanged graphics driver. No scheduler target,
catch-up or render-selection policy was adjusted to improve the numbers.

### Separate timing pair (counters and audio checksums OFF)

One stage/CPU-sampling A/B pair per workload; these are average/p95 distributions
across its measured ticks, **not** three-run stage medians:

| Stage metric, ms/tick | Baseline | Fast | Change |
|---|---:|---:|---:|
| City C352 mean | 1.432449 | 1.137981 | −0.294468 (20.56%) |
| City C352 p95 | 1.604434 | 1.322028 | −0.282406 |
| Race C352 mean | 1.422543 | 1.117351 | −0.305192 (21.45%) |
| Race C352 p95 | 1.719444 | 1.466837 | −0.252607 |
| City simulation wall mean | 10.062290 | 9.812842 | −0.249448 (2.48%) |
| City simulation wall p95 | 11.071169 | 11.349034 | **+0.277865** |
| Race simulation wall mean | 10.273612 | 9.959503 | −0.314109 (3.06%) |
| Race simulation wall p95 | 12.188314 | 11.704743 | −0.483571 |
| City simulation thread CPU mean | 9.932972 | 9.682846 | −0.250126 (2.52%) |
| Race simulation thread CPU mean | 10.190173 | 9.873791 | −0.316382 (3.10%) |
| City native sound + peripherals mean | 2.066616 | 2.108037 | +0.041421 |
| Race native sound + peripherals mean | 2.126680 | 2.128230 | +0.001550 |
| City native 68K mean | 4.037064 | 4.016809 | −0.020255 |
| Race native 68K mean | 4.092191 | 4.090632 | −0.001559 |

The mixer reduction is clear; other stages are not claimed to be optimised.
City sound-stage variation partly offsets the mixer saving, and its overall
p95 does not improve. Race sound/68K are effectively unchanged. Overall mean
wall and thread CPU savings agree closely, rather than merely moving time into
waiting. Exclusive stage totals reconcile and sample/stat buffers did not
overflow. C352 stage includes the existing slice wrapper/output handling,
which is muted and has checksum/activity options disabled in these runs.

42–45% fewer accumulation bodies translate into 20–21% less mixer time, not
42–45% less mixer time: all 32 voices are still checked for every sample and
active fetches, looping, ramps, noise, interpolation and accumulation remain.
No active work was removed. All output sample counts remain identical.

### Normalised CPU samples and diagnostic overhead

The CPU-time PC sampler provides corroboration, not inclusive call-stack cost.
Exclude presentation/video samples when forming the simulation denominator:

| Workload / mode | Simulation samples | C352-stage samples | C352-stage fraction |
|---|---:|---:|---:|
| City baseline | 296 | 42 | 14.19% |
| City fast | 273 | 30 | 10.99% |
| Race baseline | 2175 | 291 | 13.38% |
| Race fast | 2086 | 228 | 10.93% |

All city mixer samples hit `c352_generate`; race 289/291 and 227/228 do.
Normalise distributions, not raw sample-count reductions: CPU duration and
presentation count differ. Small city cohorts are coarse evidence; measured
stage clocks and the longer race are more useful.

Compared to uninstrumented medians, the single timing+sampling runs had FPS
changes −2.38%/−3.66% city baseline/fast and −1.91%/−1.36% race. Activity plus
PCM/state-checking runs had −4.55%/−5.29% city and −4.20%/−2.44% race. These
figures combine overhead with run variability, not a precise repeated overhead
measurement. Consequently neither diagnostic phase supplies headline FPS.
There are no hot-loop voice counters in performance or stage-timing builds.

Maximum uninstrumented RSS high-water values were 198.58/198.92 MiB city and
229.77/231.55 MiB race (baseline/fast). The guard allocates nothing; counter
builds add a 600-byte diagnostic object outside chip state. Larger render
counts can change existing host/driver cache histories. Texture capacity and
policy remain 64 MiB; no allocation failures or rendering errors occurred.

## Validation and changed files

Passed:

- Native desktop build; default and C352-fast native headless determinism,
  environment-isolation, effective-input and CLI regressions.
- Actual ARM64 direct fixture: exact PCM/state transcript matching baseline,
  fast and native SHA256; all 4615 snapshots.
- Native real-game PCM/state history since reset, full simulation state and
  equivalent pixels at 600/1700/3600; physical frame 600 also matches native.
- All 20 physical benchmark runs: native full state, board/video digests, zero
  faults; ranged PCM, per-interval/final C352 state, accumulator, transition
  counters and full activity histograms agree baseline/fast.
- Native C352-fast coupled/independent timing, profiler off/stage/sampling and
  texture/four diagnostic-mode/eviction tests.
- Rebuilt SDK ARM64 headless regression with C352 fast path and both WRAM
  optimisations; separate actual race audio/state comparison.

No audio device/output was enabled, so this validates deterministic chip PCM
and state, not audible speaker playback. Full real-game PCM is represented by
checksums; targeted fixture PCM/voice/register transcripts are compared exactly.

Modified/added files:

- `engine/c352.c`, `engine/c352.h`: optional entry guard and aggregate diagnostic
  API/implementation outside emulated state, shared by the existing engine.
- `raverace/CMakeLists.txt`: two OFF-by-default options, definitions scoped to
  the existing C352 source only.
- `raverace/include/rr_sound.h`, `src/rr_audio.c`: explicit optional range
  selection, four-channel/voice-history checksums and exit-time reports.
- `raverace/src/rr_headless.inc`: explicit diagnostic CLI options/help; no
  scheduler loop or translated sound program changes.
- `tools/test_c352.c`, `tools/test_c352_game.py`: strict direct/actual-game audio,
  state and rendered-capture comparisons.
- `tools/build_c352_variants.sh`, `tools/RaveRacer-C352-Benchmark.sh`,
  `tools/summarise_c352_benchmark.py`: reproducible matrix, alternating experiment
  with audio fail-fast gate, validated results/table.
- This report and the link in `GLES2.md`.

Results are retained on device at
`/userdata/roms/ports/rave-gles2/c352-benchmark-20261012-151612-22418/`, with host
copy `/tmp/rr-c352-h700-results/c352-benchmark-20261012-151612-22418/`. Raw logs,
JSON, exit codes, binary SHA manifest, histograms/audio checks, PC samples/maps,
symbol analyses and generated `comparison.tsv` are included. Native actual-game
captures/checks: `/tmp/rr-c352-game-validation/`. Physical captures/checks:
`rave-gles2/c352-check-600-{baseline,fast}.{ppm,json,log}` and
`/tmp/rr-c352-h700-captures/`. Assembly: `/tmp/rr-c352-assembly/`.

The previous working handheld executable remained unchanged (SHA verified),
and EmulationStation was restored and its running process verified. No packages
or system files were changed. No changes were committed. Source defaults and
reusable GLES2 matrix trees remain OFF. Named experimental artifacts are distinct;
a cached headless output used for the final fast regression remains experimental
until rebuilt with its restored options.

## Recommendation

**Retain and adopt the entry-inactive guard after review**, leaving its default
OFF for this evaluation. The actual optimisation is a single guarded continue,
with strong PCM/state validation. The 20–21% mixer saving (~0.3 ms/tick), 2.5–3.1%
mean simulation CPU reduction and positive FPS changes in all six pairs justify
that small complexity. This is a modest improvement, not a full-speed milestone
or a consistent Hz/resynchronisation improvement.

Do not skip active silence, suppress all muted chip output, cache decoded audio,
reorder voices, alter interpolation or introduce interval-level shortcuts.
Remaining mixer work (~1.12 ms/tick) is active state/audio processing; no further
C352 optimisation is supported by this experiment alone.

Next, investigate **sound peripheral timer scanning** as a separate controlled
experiment. Fresh race samples put `m37710_service` at 25.95% of the sound stage
(123/474 self samples), consistent with the earlier investigation. That stage
still costs 2.128 ms/tick, giving roughly 0.55 ms/tick sampled exposure—not a
predicted saving. Audit which inactive timer slots can be omitted without
changing cycle accounting, ADC/peripheral service or interrupt recognition,
and retain exact event ordering and dedicated sound/device regressions.
Overall native 68K (~4.09 ms) and DSP (~2.30 ms) remain larger stages, but the
already-identified timer scan is a specific bounded candidate. None of those
optimisations was implemented here.
