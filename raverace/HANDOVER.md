# Rave Racer RG40XX H handover

Start a new session with: **“Read `raverace/HANDOVER.md`, inspect the current
repository state, then [state the new task].”** This file is a signpost, not a
substitute for checking current source, Git status, device state and the cited
reports. Update it when a milestone changes the active build or benchmark.

## Current state (2026-10-13)

- Repository: `BennyWebb/namco22-decompile`, branch `feature/knulli-h700`.
  Latest checked commit: `83aaf0d` (`Added multicore rendering pipeline`);
  working tree was clean when this handover was created. Check both again.
- Target: Anbernic RG40XX H, Knulli Scarab, AArch64 H700/Cortex-A53,
  Mali-G31, 640×480, SDL2 `mali` framebuffer driver, GLES2, glibc 2.40.
  Matching Buildroot host toolchain:
  `~/dev/knulli-scarab/output/h700/host/`.
- The normal playable build has original 3D geometry/textures, original
  2D/HUD, audible audio, controller input, an optional performance overlay,
  deterministic input recording/replay, and three-stage multicore presentation.
  The user tested HUD, audio and controls on the device and reported that
  controls felt responsive. Do not describe it as a 60-FPS renderer.
- CMake preset `playable-knulli` enables the validated WRAM reader, C352,
  ordered submission, geometry index/projection, threaded presentation and
  texture-row optimisations. The additional `RR_PARALLEL_FRAME_PREP` flag is
  **OFF by default** in source; explicitly set it `ON` to reproduce the active
  multicore binary. `RR_PARALLEL_FRAME_PREP` requires
  `RR_THREADED_PRESENTATION=ON` and `RR_GEOMETRY_INDEX_ORDER=ON`.

## Build and deployment references

From the repository root, build the current multicore ARM64 configuration in
a separate directory, leaving the installed device executable untouched:

```sh
cd raverace
cmake --preset playable-knulli -B /tmp/rr-multicore-arm64 \
  -DRR_PARALLEL_FRAME_PREP=ON
cmake --build /tmp/rr-multicore-arm64 -j4
file /tmp/rr-multicore-arm64/rr_gles2
readelf -d /tmp/rr-multicore-arm64/rr_gles2 | grep NEEDED
```

The preset uses `raverace/cmake/knulli-aarch64.cmake` and the matched
Buildroot sysroot. Do not substitute Ubuntu's generic AArch64 glibc. Native
development uses `cmake --preset playable-native` from `raverace/`; set the
parallel flag explicitly there too. See [multicore architecture and exact
validation](MULTICORE_RENDERING_PIPELINE.md) before deploying or rerunning
benchmarks.

On the handheld, the sole normal EmulationStation entry is
`/userdata/roms/ports/Rave Racer.sh`. It launches
`/userdata/roms/ports/rave-gles2/rr_gles2`; its persistent log is
`/userdata/roms/ports/rave-gles2/rave-racer.log`. The previous known-good
executable and launcher were preserved under
`/userdata/system/rave-racer-backups/20261013-pre-parallel-prep/`. The active
binary's recorded SHA-256 is
`77536f56ad17de9cf8772175c73442d9cb246d87aeb6fbf6c8d7f07c3d6ab53e`;
verify before relying on that identity. Do not overwrite the backup or
clean user replay/EEPROM data. Last known SSH address was `192.168.1.69`;
check reachability and that no game is running before device work. Restore
EmulationStation after testing.

## Reproducible workload and measured baseline

The primary representative workload is the player's complete first-lap
recording at
`/userdata/system/rave-racer-replays/race-20261013-122242-25136.rec`.
Measure ticks **1800–5639**; metadata, original EEPROM image, endpoint and
state checkpoints are sidecars beside the `.rec` file. The dense-opening
scripted window (ticks 1400–1700) remains a historical stress test. The
scripted 1400–3600 run has a wall-facing tail and is **not** representative
racing FPS. See [recording and replay](INPUT_RECORDING_REPLAY.md).

Five alternating physical replay A/B pairs, same traced mode and original
HUD/audio enabled, measured the bounded three-stage build against the
previous threaded presentation path:

| First-lap ticks 1800–5639 | Previous | Three-stage |
|---|---:|---:|
| Median rendered FPS | 33.431 | 41.279 |
| Median 1% low FPS | 17.890 | 20.624 |
| Median p99 presentation interval | 46.521 ms | 39.159 ms |
| Median largest hitch | 92.670 ms | 101.590 ms |
| Simulation rate | 59.904 Hz | 59.904 Hz |
| Presentation-thread CPU/picture | 29.364 ms | 22.532 ms |
| Preparation CPU/picture | — | 14.767 ms |
| Snapshot age to swap-return | 38.505 ms | 49.847 ms |
| Peak process RSS | 277.926 MiB | 293.172 MiB |

The split improves median FPS by 23.5% and the 1% low by 15.3%, while raising
software image age by about 11.3 ms. That image age is not measured
controller-to-photon latency. The rare texture burst near tick 1965 remains;
worst-frame latency did not improve. The GLES-owner thread is still the
largest serial rendering stage, about 22.5 ms CPU per picture in the matched
run, so this split alone cannot sustain 59.904 rendered FPS. No reliable Mali
GPU utilisation counter or `perf` executable was available on the device.

The user's later interactive session ran 23,604 emulated ticks, exited
normally, and reported correct HUD/audio and responsive controls. Its final
log reported zero audio underruns and emulation faults, 59.9049 simulation Hz
and 47.9843 rendered FPS across a different player-driven route. Do not compare
that overall FPS directly with the first-lap window.

The detailed A/B logs, timestamp CSVs and `/proc` samples remain under
`/userdata/system/rave-racer-multicore/`, especially
`ab-20261013-162808-17838` (final five pairs),
`replay-correctness-20261013-164429-22215` and
`diagnostic-20261013-165148-22678`. Use
`raverace/tools/RaveRacer-Multicore-Replay-Benchmark.sh` and
`raverace/tools/analyse_multicore_replay.py` as documented in the multicore
report. Preserve equivalent instrumentation when comparing FPS.

## Correctness and next-session guardrails

The multicore build passed native and ARM64 builds; native headless and
snapshot/serial/thread/drop regressions; same-GPU equivalent-frame pixels,
ordered geometry, texture events, board/simulation digests and PCM/voice
checks; and the full recorded replay's 63 state records. Native ThreadSanitizer
reported no application race in its frame-1700 test, with a documented Mesa
internal suppression; Mali code was not instrumented. Review
[multicore architecture](MULTICORE_RENDERING_PIPELINE.md) for exact scope and
limitations, and [performance audit](ARM64_GLES2_PERFORMANCE_AUDIT.md) for
older architectural findings. Historical reports predate later improvements;
do not treat their performance figures as the current baseline.

For any new change: inspect the actual build flags and Git diff first; keep
the known-good device executable until the candidate is validated; compare
simulation, board, audio and equivalent-frame visual state; use the human lap
for racing performance; and report simulation rate separately from actual
presentation timestamps. Do not infer GPU saturation from FPS or move CPU
texture decoding across threads without an immutable, versioned resource
handoff. The next optimisation has not been selected by this handover.
