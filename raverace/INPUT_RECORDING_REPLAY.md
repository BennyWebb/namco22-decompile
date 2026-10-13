# Handheld input recording and deterministic replay

This feature reuses `rr_input.c`'s existing change-only input format. It does not
create a driving script, AI, save-state system, or a second replay format.
Representative race inputs must be supplied by the player. The dense-opening
1400–1700 fixture and the extended drive/wall-tail fixture remain unchanged;
the latter is a regression workload, not representative racing performance.

## Input and reset architecture

The translated program advances the real scheduler in 16 slices per video tick.
CPU, DSP, sound/C352 execution complete those slices before the video boundary.
The simulation owner performs essential video bookkeeping and publishes an
immutable snapshot; the presentation worker may replace/drop snapshots.
Controller polling also occurs during pacing waits. Once per emulated boundary,
the controller bridge applies active-low cabinet bits and final 12-bit ADC
values. Replay/record then executes **before** `rr_dsp_vblank()`,
`rr_hw_vblank()` and IRQ delivery. This is the existing desktop recorder's
boundary, now also used by GLES2 and headless. Presentation success is irrelevant
to whether a tick's input is recorded.

The recorder includes the entire cabinet input word plus steering, accelerator
and brake. This includes coin, view and gear bits. Rave Racer's START/menu action
is the accelerator input (including the host's short START pulse), not a separate
emulated START register. Capturing final values preserves pedal ramping and
controller mapping without needing those host mappings on replay.

The SDL-free stream implementation is shared by desktop bindings and the
headless/GLES2 executable. Desktop `--record`/`--replay` still use the same `F`
records. The strict deterministic-session CLI adds mandatory sidecars; it does
not accept a legacy file without reset metadata. Metadata-free legacy recordings
can still use the existing desktop interface.

Recording starts at reset, including coin, menus, track/car selection and the
countdown. Mid-race recording is deliberately unsupported: it would need a
complete CPU/DSP/board/sound save state, not just an input file. The current
handheld runtime starts from the extracted factory EEPROM seed and does not load
host-persisted operator/high-score settings. A separate initial EEPROM copy is
saved before executing the reset entry and never modified by gameplay.

## Recording on the RG40XX H

The single Ports entry remains **Rave Racer**. Experimental recording uses
`/userdata/system/rave-racer-replay/rr_replay`; normal mode retains the installed
known-good `/userdata/roms/ports/rave-gles2/rr_gles2`.

Enable recording before launching:

```sh
ssh root@192.168.1.69 'printf "record-once\n" > /userdata/roms/ports/rave-gles2/replay-mode'
```

1. Open **Ports → Rave Racer**. Recording starts from boot automatically.
2. Insert coins/select track and car normally, then drive your race.
3. Optionally press physical **X** (SDL Y), or **F3**, at race start and finish.
   This retains the existing overlay cycling and adds numbered `# MARK` comments
   at the next exact input boundary. Do not treat an arbitrary overlay toggle as
   a race-state detector. Identify which markers you intended as start/finish.
4. Use **START+SELECT** to exit and save. Recording completes the current tick
   before shutdown, so the final state and audio checkpoint have a reproducible
   endpoint. Escape/SDL quit behaves the same while recording.
5. Disable recording for subsequent normal launches:

```sh
ssh root@192.168.1.69 'printf "off\n" > /userdata/roms/ports/rave-gles2/replay-mode'
ssh root@192.168.1.69 'ls -lt /userdata/system/rave-racer-replays/'
```

The setting is read at launch, not mid-session. `record-once` resets the mode
to `off` as that launch starts; `record` enables recording for every launch until
explicitly disabled. No second launcher is needed.
Recording does not yet have a pause/resume mechanism; finishing and restarting
creates a new deterministic session. All normal HUD/audio/controller features
remain enabled. Overlay markings are annotations and do not affect cabinet state.

Each recording has a unique timestamp/PID basename:

```text
/userdata/system/rave-racer-replays/race-YYYYMMDD-HHMMSS-PID.rec
```

Files are opened exclusively; existing recordings and sidecars are not replaced.
The normal persistent log remains
`/userdata/roms/ports/rave-gles2/rave-racer.log`. Look for `[REPLAY] ... status=ok`
after exiting. A power loss or forced kill is not a successfully closed recording.

## Format and determinism evidence

```text
# Rave Racer input recording: F frame inputs steer gas brake (hex), on change
F 1 FEFF 800 000 000
...
# MARK 2400 1
```

`F <tick> <digital hex> <steer hex> <gas hex> <brake hex>` means “apply these
values before this tick's vblank”, then hold them until the next change. All
input values are captured on the simulation thread. Markers are ignored by
playback and by legacy desktop readers. No per-tick file flush or hot-loop logging
is introduced. Standard buffered writes record at most one input-change line per
tick. Buffers/files are owned solely by the simulation thread.

Sidecars:

| File | Contents |
|---|---|
| `.rec.meta` | Replay version 1, game, 25600000/427350 Hz (59.904059904 Hz) timing, input boundary, reset policy, scheduler poll count, initial EEPROM FNV1a64, every relevant ROM filename/size/FNV1a64 |
| `.rec.eeprom` | Separate 8192-byte initial EEPROM image, never the final gameplay EEPROM |
| `.rec.end` | Last fully completed tick, written on orderly shutdown |
| `.rec.states.jsonl` | Every 300 ticks and at the final tick: CPU/register/RAM state, board/video-memory hashes, cumulative raw four-channel PCM and C352 voice hashes/sample counts |
| `.rec.build.sha256` | SHA256 of the recording candidate and preserved playable executable (launcher-generated) |

ROM fingerprints include program, textures/tilemap, points, sound program,
wave ROMs, gamma PROMs and factory EEPROM. FNV hashes are portable identification
and regression checks, **not cryptographic authentication**. Metadata and the
initial EEPROM are checked before playback begins. Mismatches, malformed events,
invalid widths/values and non-increasing input ticks fail rather than silently
continuing. The baseline readable/debug/network environment remains isolated.

The default `--replay-checks on` enables the existing raw audio checksum machinery.
Checkpoints perform no emulated memory reads with side effects. Board/device
fields are serialized byte-defined; video RAM and CPU/RAM hashes are compared.
Vehicle coordinates/speed/heading are not separately decoded here: complete WRAM
and register digests cover their stored state, rather than judging visual routes.
Headless and rendered playback must match the **original recording**, not only
one another.

## Build and regressions

Use the validated preset (all existing CPU/geometry/submission/threading flags):

```sh
cd ~/dev/namco22-decompile/raverace
cmake --preset playable-native
cmake --build --preset playable-native
cmake --preset playable-knulli
cmake --build --preset playable-knulli
```

A headless validation binary needs the same CPU optimisations, without SDL/GL:

```sh
cd ~/dev/namco22-decompile
cmake -S raverace -B /tmp/rr-replay-headless -DRR_HEADLESS_ONLY=ON \
 -DRR_FAST_MEMORY_READS=ON -DRR_SPLIT_MEMORY_READER=ON \
 -DRR_C352_INACTIVE_FASTPATH=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build /tmp/rr-replay-headless -j4
python3 raverace/tools/test_input_replay.py /tmp/rr-replay-headless/rr_headless raverace/extracted
```

For ARM64, additionally pass
`-DCMAKE_TOOLCHAIN_FILE="$PWD/raverace/cmake/knulli-aarch64.cmake"` and
`-DKNULLI_HOST="$HOME/dev/knulli-scarab/output/h700/host"` to a fresh headless
build directory. Do not use Ubuntu's generic AArch64 toolchain. Inspect `file`
and the SDK's `aarch64-buildroot-linux-gnu-readelf -d` output.

## Playback, validation and benchmarking

Copy a completed recording and all sidecars when moving between machines. Paths
in the ROM manifest are filenames, not host absolute paths.

```sh
python3 raverace/tools/inspect_input_replay.py /path/race.rec
rr_headless extracted --replay-input /path/race.rec \
 --input-checkpoints /path/new-playback.states.jsonl
python3 raverace/tools/validate_input_replay.py /path/race.rec /path/new-playback.states.jsonl
```

Playback stops at `.end` by default; `--frames N` can choose an earlier endpoint.
Do not combine playback with `--interactive` or scripted coin/gas/steering.
Validation includes the final completed tick. Files for new playback checkpoints
and timing output must not already exist.

Only after validation, run representative benchmark playback. Select racing
bounds from your start/finish markers; exclude boot, menus and countdown, and
optionally the first two racing seconds. The runner verifies the entire original
recording using headless execution before benchmarking. It then disables detailed
PCM hashing (`--replay-checks off`) in headline runs; emulation and sample counts
are unchanged. Three alternating untraced/traced pairs are the default, to quantify trace
overhead instead of attributing file-I/O stutters to gameplay.

```sh
ssh root@192.168.1.69 \
 'sh /userdata/system/rave-racer-replay/RaveRacer-Replay-Benchmark-v3.sh \
 /userdata/system/rave-racer-replays/race-YOUR-FILE.rec RACE_FIRST RACE_LAST 3'
```

Use literal numeric racing ticks in place of `RACE_FIRST`/`RACE_LAST`. The runner
refuses to interrupt a running game, stops EmulationStation only for framebuffer
ownership, restores it on exit, uses isolated experimental binaries and preserves
the playable executable and original recordings. Unique results directories are
created under `/userdata/system/rave-racer-replay/results-*`.

Retrieve and analyse each run:

```sh
scp -r root@192.168.1.69:/userdata/system/rave-racer-replay/results-YOUR-RUN ./race-results
python3 raverace/tools/analyse_race_replay.py \
 race-results/run-1.presentations.csv race-results/run-1.ticks.csv RACE_FIRST RACE_LAST race-results/run-1.log \
 > race-results/run-1.sections.csv
```

The optional presentation CSV is now streamed through a bounded 64 KiB buffer,
so long races are not truncated after 8192 presentations. The existing bounded
in-memory aggregate record array is unchanged; the complete CSV is authoritative
for full-race percentiles. Optional per-tick timing uses a bounded 16 KiB buffer.
These are benchmark instrumentation, not enabled in normal interactive recording.

Sections contain about 300 ticks (5.008 emulated seconds); a tiny tail is merged
into the previous section. `simulation_hz` uses monotonic completed-tick timestamps.
Presented FPS counts **swap completions in that same wall-clock interval**.
Mean/median/p99 intervals and 1% lows use actual monotonic swap-return timestamps,
not simulation ticks. 1% low is reciprocal mean of the slowest ceil(1%) intervals;
a five-second section contains relatively few tail samples, so also inspect max
intervals and repeatability. An interval crossing the selected race startup is
excluded. Short-windows are not interchangeable with the overlay's rolling
300-presentation statistics.

The report includes presentation thread CPU time, scene wall time, inclusive 3D
draw wall time, existing submission wall time, swap wall time, draws/triangles,
texture activity, logical GPU cache allocation, queue-ready depth, cumulative
snapshot drops and snapshot age. These stages are nested: **do not sum inclusive
3D draw time and submission time**. HUD/overlay work is included in total
presentation CPU. Submission/swap wall time can include driver waits; neither is
GPU execution time. Swap return is not an independently measured physical scanout
or input-to-photon timestamp. Missing simulation ticks in the presentation stream
are reported as unpresented ticks, separately from discarded READY snapshots.

Inspect `[THREAD-RATE]` for whole-range resyncs/discarded lag and compare repeated
per-section reports. No GPU utilisation or fabricated display timing is inferred.
Use worst p99/max intervals, 1% lows and CPU/scene/submission costs together to
locate repeated stutters; no automatic claim about causation is made from FPS alone.

## Validation and measured results

Native record/replay and existing headless regressions pass. Existing native
threaded/serial/snapshot/drop tests pass at 600, 1700 and 3600 ticks. A separately
recorded existing 1700-tick fixture reproduced exact pixels, ordered geometry,
texture events, board/video bookkeeping, PCM/voice hashes and checkpoints on
native playback. This fixture is **not** human driving or representative race FPS.

Physical handheld tests and recording-disabled/recording-enabled overhead results
are documented below once complete. A real representative performance report
requires the player's completed recording; no synthetic race or invented numbers
will stand in for it.

### Handheld pre-recording validation (2026-10-13)

The isolated ARM64 run at
`/userdata/system/rave-racer-replay/validation-20261013-114918-22049`
passed existing headless regressions and strict replay rejection tests. Recording
and playback of the historical 1700-tick fixture matched the preserved playable
binary's pixels and simulation state. Six completed checkpoints matched native
headless and ARM64 headless/GLES2 CPU/RAM, board/video and PCM/voice digests.
No traps, unmapped accesses, ROM writes, DSP faults or sound faults occurred.

Three alternating triplets, original HUD/audio enabled, dense ticks 1400–1700:

| Condition | Median simulation Hz | Median rendered FPS | Simulation CPU ms/tick | Presentation CPU ms |
|---|---:|---:|---:|---:|
| Preserved playable | 59.868 | 40.981 | 11.130 | 22.431 |
| Candidate, recording off | 59.871 | 41.171 | 11.160 | 22.287 |
| Candidate, recording on | 59.907 | 41.004 | 11.302 | 22.376 |

FPS ranges were 40.774–41.167, 40.778–41.970 and 41.000–41.392 respectively.
These do not establish an improvement; there is no measurable presentation
regression in this short experiment. Recording plus raw PCM/voice verification
added about 0.17 ms/tick against the playable median (about 1.55% of simulation
CPU), while maintaining arcade-rate simulation. All nine final states matched,
all runs had zero resyncs/discarded lag, queue peak two, and no host-audio
underruns/overruns/latency resets. Detailed per-tick/CSV tracing was disabled in
these headline overhead runs.

Raw aggregate results are retained in
[benchmarks/input-recording-20261013.json](benchmarks/input-recording-20261013.json).
The optional native SDL-quit injection test also passed: input recording ended
on a completed tick and headless replay matched its final state, board/video and
audio checkpoints. Native ThreadSanitizer recording/playback with dummy audio
and expanded overlay reported no races; dummy-audio scheduling is not handheld
audio evidence.

This validates the recording machinery, **not a human-driven route**. The
representative racing report remains pending the player's recording. The later
exit-only fix freezes the existing cabinet inputs when a recording quit is
latched, preventing the exit chord from injecting an extra coin/START action.
Its normal recording-off path and game/rendering semantics are unchanged.

The final quit-boundary candidate also passed the physical frame-3600 regression
against the preserved fully playable build (pixels, geometry, PCM/voices and
simulation/board digests):
`/userdata/system/rave-racer-replay/final-check-20261013-120419-23343`.
The final timing-metadata-corrected candidate SHA256 is:
`a49dd7312d6110fa0be107ab87ded722bcf6c2dd07b89bcc2e51c33ffda33072`.
The frame-3600 check above used its preceding exit-only revision; the subsequent
change corrected metadata/duration labelling, not the scheduler or renderer.
Both native and physical ARM64 strict replay tests passed again after that fix.
Earlier internal fixtures retain their original metadata and are deliberately
rejected by the corrected metadata comparison; no user race was recorded with
the incorrect label.

The installed Ports launcher has optional recording support but **normal mode is
still the default**. The current playable binary retains SHA256
`80d36fc2c536c4931da41491aa097be60e5cbcb24efbb036d940cd91bc438e7f`.
Its previous launcher is preserved at
`/userdata/system/rave-racer-replay/RaveRacer-known-good.sh`. EmulationStation was
restored and only one Rave Racer launcher remains in the active Ports directory.
The newly staged `RaveRacer-Replay-Benchmark-v3.sh` compares every
headline run's final state to a headless reference at the same endpoint. Untraced logs provide headline throughput and tracing-overhead comparisons;
complete per-section statistics come from the traced pairs. Older
experimental helper files remain preserved outside Ports, not user recordings.

## Completed human-driven lap

The player supplied `race-20261013-122242-25136.rec` and confirmed working HUD,
audio and controls. All 21 original checkpoints through tick 6006 matched native
and ARM64 playback. Human-input equivalent-frame pixel/geometry/audio/state
checks also passed on native and Mali.

The first marker was in the menu before coin insertion; the second was after the
first lap. Verified moving-race ticks **1800–5639** are the new representative
window, not an assertion that the entire recording is racing. Three alternating
untraced/traced pairs measured **33.914 FPS** and **59.903819 simulation Hz**
(untraced medians), with zero timing resyncs, discarded lag or audio underruns.

The full per-section example, 1% lows/p99 intervals, repeatable stutter locations,
validation paths and exact repeat commands are in
[RECORDED_RACE_BENCHMARK.md](RECORDED_RACE_BENCHMARK.md). This recording is a
representative first lap, not a complete multi-lap race. No optimisation was
implemented to improve its measured numbers.
