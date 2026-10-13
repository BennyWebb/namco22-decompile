# Rave Racer RG40XX H handover

Start a new session with: **“Read `raverace/HANDOVER.md`, inspect the current
repository state, then [state the new task].”** This file is a signpost, not a
substitute for checking current source, Git status, device state and the cited
reports. Update it when a milestone changes the active build or benchmark.

## Current state (2026-10-13)

- Repository: `BennyWebb/namco22-decompile`, branch `feature/knulli-h700`.
  Latest checked commit: `6ebe29c` (`Frame pacing Latency experiment.`).
  The current working tree has uncommitted diagnostic and report changes from
  the ordered-submission investigation; check Git status before new work.
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

## Latest frame pacing experiment (2026-10-13)

See [frame pacing and image age experiment](FRAME_PACING_LATENCY_EXPERIMENT.md).
The new `RR_PREP_WAKE_ON_SNAPSHOT` CMake flag is OFF by default and requires
`RR_PARALLEL_FRAME_PREP=ON`. It lets the preparer wake when a fresh snapshot
arrives within the existing 6 ms grace period. Five alternating handheld
first-lap A/B pairs measured 41.262 versus 41.544 median rendered FPS, 20.411
versus 20.655 1% low FPS, 39.296 versus 39.364 ms p99 presentation interval,
and 49.126 versus 49.338 ms median image age (prior wait versus wake-up).
Median worst hitch worsened from 96.078 to 100.550 ms. The change moves about
0.98 ms from READY wait to PREPARED wait, without a meaningful latency or
pacing benefit. **Do not deploy this experiment based on these results.**

The installed playable executable and prior backup are unchanged. The active
binary hash above was reverified; EmulationStation was restored and the sole
Ports launcher remains `Rave Racer.sh`. Same-GPU replay frames 1965 and 3600,
all recorded checkpoints through tick 6006, native fixed-cadence texture event
digests and ThreadSanitizer checks passed. The five-pair data is under
`/userdata/system/rave-racer-multicore/ab-20261013-172218-26240`, and the
separate scheduler pair under `ab-20261013-174532-8107`. The GLES owner
remains the throughput limit; first-time texture bursts still dominate the
worst hitches. No repository commit was made.

## Ordered GLES2 submission investigation (2026-10-13)

See [ordered multi-texture experiment](GLES2_ORDERED_MULTITEXTURE_EXPERIMENT.md).
The first-lap diagnostic trace found 860.83 draws per presented picture;
contiguous four-texture groups could reduce this to 207.58 while retaining
scissor and mutation barriers. Mali-G31 reports 16 fragment texture units.
A bounded four-sampler prototype cut short-window submission time, but the
full-lap equivalent tick-2500 capture differed at 91 pixels. Shader-only
reproduction identified the fragment path; a straight-line sampler variant
still differed in batched mode. **The experiment failed the pixel-identity
gate, was removed from source, and was not deployed.** The alternating test
stopped at pair 2 rather than collecting five performance pairs for an invalid
candidate. Only the diagnostic trace behind `RR_GLES_SUBMISSION_PROFILE`
remains in the working tree. The active device executable and backup were
preserved; verify their hashes and EmulationStation before the next device
session. No commit was made.

### Shader-equivalence follow-up (2026-10-13)

The original deterministic tick-2500 pair is
`/tmp/multi4-ab-20261013-182049-28305/{baseline-2,candidate-2}.ppm`.
`candidate-2.ppm` is identical to `/tmp/shader-2500.ppm` (multi-sampler
shader, batching disabled). Pixel-by-pixel comparison finds 91 of 307,200
changed pixels, in two thin diagonal bands on the brown road-side surface
(x=299–526, y=218–254). Changed channel samples: R=90, G=78, B=27;
maximum absolute errors R=44, G=40, B=30; mean absolute error among changed
samples R=5.36, G=4.68, B=7.22. Whole-frame mean absolute errors are
R=0.001569, G=0.001188, B=0.000635. Use the untracked
`tools/compare_multitexture_capture.py` for the full signed histogram.

A temporary diagnostic build captured pre-compositor scene RGBA in
`/tmp/shader-split-20261013/{scene4_off,scene4_shader}.rgba`. Its 91 changed
pixels map exactly to the final capture after vertical flip. Scene differences
are RGB only (R=90, G=78, B=27, A=0); all changed pixels have alpha zero in
both variants. White-batch rendering is pixel-identical, so the mismatch
requires the real texture sample path. The original shaders use the same
`highp` UV precision, perspective division, texture multiplication, alpha
discard and priority-alpha handling; textures use `GL_NEAREST` and
`GL_CLAMP_TO_EDGE`. In shader-only mode all sampler uniforms address texture
unit 0. UV precision or nearest-texel selection remains a hypothesis, not an
identified cause.

The standalone probe in untracked `tools/mali_sampler_probe.c` ran on the
actual Mali-G31 (OpenGL ES 3.2, driver v1.r20p0). All 32 combinations of
conditional versus straight-line sampler selection, shared versus separate
texture units, slots 0–3 and two alpha priorities were RGBA pixel-identical
over 156,252 non-clear pixels. This synthetic case does not reproduce the
scene failure. A straight-line shader-only build with submission profiling
enabled also matched baseline, while another build without profiling differed;
same-binary `glFinish` toggles changed the mismatch pattern but never fixed
it. A fixed color adjustment would be unjustified. No precise shader cause or
correction has been established, and representative-frame equivalence has not
been demonstrated. Keep multi-texture batching disabled.

Temporary source/builds are under `/tmp/rr-shader-repro/`; local capture copies
are under `/tmp/shader-split-20261013/`, with device copies under
`/userdata/system/rave-racer-multicore/shader-split-20261013/`. They did not
replace the deployed playable. The installed executable hash verified before
testing was `77536f56ad17de9cf8772175c73442d9cb246d87aeb6fbf6c8d7f07c3d6ab53e`;
each completed device run restored EmulationStation. The final device-state
check was interrupted, so verify the hash and process state next session.
No credentials were stored in the repository, and no commit was made.

### Shader-equivalence resolution (2026-10-13, later follow-up)

See [tick-2500 resolution](SHADER_EQUIVALENCE_RESOLUTION.md). The earlier
fragment-shader attribution is superseded: polygons **978 and 1351** use a
history-dependent cached texture bake, either 594×80 / source-column step 1
or 298×80 / step 2, with different submitted U scales. Changing only these
resources using the same single-sampler shader reproduces every original
91-pixel RGBA pair exactly. The isolated four-triangle case changes 102 pixels;
11 are covered by later scene polygons. Applying captured coverage/order
reconstructs the original entire candidate scene byte for byte.

The replay's dropped presentations affect texture-cache residency. The sticky
higher-tier policy can therefore return different texture tiers at the same
simulation tick, despite an identical geometry digest. An off-mode diagnostic
run reproduced the original candidate, while another off-mode run reproduced
the original baseline. Do not treat a fixed replay tick as proof of identical
draw resources.

A frozen complete tick-2500 scene (**1,280 draws, 7,998 vertices**) passed exact
RGBA equivalence for original-program replay, conditional four-sampler and
straight-line four-sampler shaders, all samplers addressing texture unit 0.
Packed float32 UV/UVQ diagnostics also matched shader variants bit for bit for
fixed inputs. No Mali shader correction is supported or needed for this case.

The durable [standalone case](tools/shader_equivalence_case/README.md) contains
exact vertices, texture data, shaders and sample coordinates. Full captures and
the temporary renderer diagnostic patch are under `/tmp/rr-primitive-results/`.
Existing captures were preserved; prior temporary renderer sources were restored
and rebuilt. The installed playable hash above was reverified and
EmulationStation restored. No production change, deployment or commit occurred.

**Keep batching disabled.** Any next batching test must hold draw resources
fixed and validate its own ordering, transparency, HUD and texture mutation
behaviour. Do not alter the intentional sticky-tier policy as a shader fix.

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
