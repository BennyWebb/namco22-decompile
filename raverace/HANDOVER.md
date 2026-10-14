# Rave Racer RG40XX H handover

Start a new session with: **“Read `raverace/HANDOVER.md`, inspect the current
repository state, then [state the new task].”** This file is a signpost, not a
substitute for checking current source, Git status, device state and the cited
reports. Update it when a milestone changes the active build or benchmark.

## H700 production GPU HUD integrated and deployed (2026-10-14)

Read [GPU_HUD_PRODUCTION.md](GPU_HUD_PRODUCTION.md) for final configuration,
regressions, [confirmation benchmark](benchmarks/gpu-hud-production-20261014-summary.json)
and rollback instructions. The user confirmed original HUD/gameplay, steering,
controller operation and audible synchronised sound.

- `playable-knulli`: GPU HUD/default glyph-quads ON, CPU fallback selectable;
  all validated three-stage/geometry/memory/C352/tile-run flags preserved;
  batching, pre-decoding and wake experiment OFF.
- Installed executable SHA256
  `6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`.
  Existing launcher uses `gpu-hud-mode=default`, recording OFF, exact controller
  mapping and audio ON. Source changes remain uncommitted.
- Native full replay: 2,002 RGBA pictures exact against CPU; 46 frozen/synthetic
  cases pass. ThreadSanitizer through 1700: no application race, exact pixels,
  state/PCM/voices and checkpoint prefix (existing Mesa-only suppression).
- Final ARM64: 2,002 fixed pictures plus 913 adaptive/live-audio pictures exact;
  all 63 recording state records match; final pixels, geometry, misses, PCM and
  voices match pre-GPU references; CPU/serial fallback checks pass. Verification
  readbacks cause one diagnostic resync, excluded from performance claims.
- Two alternating final confirmation pairs: accepted GPU prototype 52.728 FPS,
  consolidated 52.939; 1% low 25.214 / 25.297; p99 32.802 / 32.657 ms;
  image age median 38.706 / 37.370 ms. Every normal benchmark run has zero audio
  underruns and resyncs, simulation ~59.904 Hz. Prior five-pair 52.714 FPS benefit
  retained; tiny consolidation difference is not a new optimisation claim.
- Fresh previous GPU playable/config backup:
  `/userdata/system/rave-racer-backups/20261014-pre-consolidated-gpu-hud/`.
  `restore.sh` restores the manually accepted GPU trial. Older CPU and
  pre-parallel-prep backups/hashes untouched. Mode `off` selects original CPU
  HUD in the new binary without changing controller configuration.
- Device artifacts `/userdata/system/rave-racer-gpu-hud-consolidated-20261014/`;
  validation `validation-20261014-152511-6403`, benchmark
  `ab-20261014-153427-7324`, startup `deployed-startup.log`; host
  `/tmp/rr-hud/consolidated/`. All preserved.
- Last final check: EmulationStation PID 23050, no game running, one active
  `/userdata/roms/ports/Rave Racer.sh`. No commits.

## Earlier GPU HUD interactive trial (superseded by production integration above)

Read [GPU_HUD_PROTOTYPE.md](GPU_HUD_PROTOTYPE.md) and
[the five-pair summary](benchmarks/gpu-hud-20261014-summary.json).

- Default-OFF `RR_GPU_HUD` adds compact snapshot metadata, a lazy glyph atlas,
  one tile-quad batch into an indexed HUD target, and palette/shadow/priority/LUT
  composition. No world geometry/cache changes. Runtime selectors are
  `ENG_GPU_HUD=1 ENG_GPU_HUD_GEOMETRY=1`; unset/off retains the original CPU path.
- Five alternating pairs against the installed multicore binary: median FPS
  40.420 → 52.714 (+30.4%); 1% low 19.134 → 25.151; p99 41.035 → 33.027 ms;
  median worst 85.466 → 79.755 ms; image age median/p95
  50.585/67.119 → 37.945/55.108 ms. Simulation remains ~59.904 Hz.
- HUD preparation 11.070 → 0.048 ms; HUD upload/update CPU 5.538 → 1.003 ms;
  GLES-owner CPU 23.265 → 17.013 ms. Peak RSS 293.836 → 288.047 MiB.
  All ten runs have zero audio underruns and match endpoint JSON.
- Fixed-through-6006 verification: 2,002 pictures byte-exact, same final capture,
  geometry, texture miss sequence, board, PCM and voices. Adaptive live-audio
  verification: 2,078 selected pictures exact through 5639; normal CPU fallback
  matches PCM `a18439c518e08e3d`, voices `c12e04d613b280fc`, endpoint state.
- Both architectures passed 14 captures + 32 synthetic cases. Direct packed
  tilemap shader had ~9.18 ms warmed span versus ~3.30 ms reference; retained
  glyph-quad final pass is ~2.775 versus ~3.276 ms at racing tick 2502. Dirty
  elapsed queries were disjoint and rejected; no exclusive GPU time claim.
- A fake early mismatch was captured as the EmulationStation Ports screen: its
  asynchronous restart raced the next test. Intermediate pens were correct.
  The durable benchmark waits for wrapper + child shutdown and rejects a
  competing frontend. Do not reintroduce the old conditional stop/start race.
- Device artifacts `/userdata/system/rave-racer-gpu-hud-20261014/`; host
  `/tmp/rr-hud/`. Headline run `ab-20261014-135149-14295`; guarded fixed run
  `geometry-validation-20261014-134641-14211`; live
  `live-verify-20261014-141046-19437`; fallback/audio
  `final-checks-20261014-141928-20309`. Preserve captures and diagnostics.
- Candidate `bin/rr_hud_geometry_capture`, SHA256
  `fef8f66dc7e87c18668a32b5c43f473d14c98b9323cb9236d47380700ce32b52`.
  Deployed to the existing Ports → Rave Racer launcher at the user's explicit
  request for physical controller/audio validation. GPU glyph-quads and the
  validated three-stage production optimisations are enabled; batching and
  texture pre-decoding are compiled OFF. Verification/capture diagnostics are
  disabled. `gpu-hud-mode=on`, `replay-mode=off`.
- Prior executable (SHA256 `77536f56ad17de9cf8772175c73442d9cb246d87aeb6fbf6c8d7f07c3d6ab53e`),
  exact launcher and mode configuration are preserved under
  `/userdata/system/rave-racer-backups/20261014-pre-gpu-hud-interactive/`.
  Exit the game with START+SELECT and run that directory's `restore.sh` over SSH
  to restore the previous playable executable and configuration. The earlier
  pre-parallel-prep backup remains untouched.
- Launcher explicitly loads the exact built-in RG40XX-H controller mapping from
  `controller-input.cfg`; startup confirms recognition, three-stage mode,
  glyph-quads with verification OFF and zero audio underruns. ES restored.
- Physical controller, HUD/menu, audio and full-race validation remain pending
  user confirmation. This is an authorised interactive trial, not acceptance
  as the permanent default. One Ports launcher; no commits or optimisation work.


## Texture preparation investigation completed (2026-10-14)

Read [TEXTURE_PREDECODE_INVESTIGATION.md](TEXTURE_PREDECODE_INVESTIGATION.md)
and [the complete measurements](benchmarks/texture-predecode-20261014-summary.json)
before further texture-cache work. Batching remains OFF; no more batching work.

- Actual HEAD at start: `309e053` (`Multitexture followup`). No commits made in
  this investigation. The working tree adds default-OFF CPU pen-bake/complete-ROM
  preparation and bounded private GPU preload diagnostics, plus tools/reports.
- Fresh five-pair deployed/newer-unbatched reconciliation: 41.090 / 40.953 FPS.
  Historical 34.48 FPS does not reproduce as a build regression. Long historical
  swap scopes and simulation pauses are documented; simple 256 MiB `/tmp`
  occupancy was tested and did not explain it. Initiating historical cause is
  unresolved. Audio anomalies correlate with simulation gaps, often before the
  racing window; whole-process counters must not be labelled racing-only.
- Final five pairs compare the **installed multicore executable directly** with
  startup-prepared pen bakes: FPS 41.278 / 41.122, 1% low 19.853 / 23.142,
  p99 40.313 / 37.287 ms, median worst 104.110 / 71.758 ms. All five improve
  low/worst pacing; average FPS is unchanged. Additional RSS ~110 MiB, startup
  ~3.55 s preparation; active GPU budget remains 64 MiB. Every endpoint matches.
- Complete replay through 6006: 14 selected HUD frames + final capture identical,
  texture semantics `2a6578ef363d13f3`, PCM `0c24e8fca4b6400e`, voices
  `b76f90445ba87e18`, board `f9154a83`. 136,188,011 live RGBA texels checked
  byte-exactly. Corrected code-generation candidate validated separately.
- Initial same-binary A/B had a compiler artefact: verification's second call
  stopped inlining the ordinary tile-run decoder. It was corrected with a flag-
  gated always-inline attribute; final deployed-baseline pairs supersede it.
- Current Rave game table does **not** enable conditional Sys22 map fixup.
  Complete-ROM representation needs 65,747 combinations, 32-bit map indices,
  20.05 MiB resident. A 16-bit attempt safely did not activate and is excluded.
  Corrected raw/row/frame verification passed; one probe saved ~6 ms of first-
  burst decode, with weaker hitch improvement. No five-pair claim for that mode.
- Independent 8 MiB GPU preload: exact live-RGBA equality required before adopting
  unused private objects. 80 valid objects used; deliberately corrupted data
  rejected and normal uploads/pixels/state preserved. One GPU-only probe reduced
  upload scope 23.548 -> 1.348 ms but did not improve mean FPS. Not production.
- Persistent-cache feasibility sample: source SHA-256 + decoder/version/mode,
  canonical keys and CRCs, ~110 MiB raw -> 33.5 MiB gzip. Handheld cold/warm read
  + inflate medians 2.227 / 2.115 s, excluding validation/allocation/import.
  No learner/loader implemented. Production must learn/derive generic keys,
  retain missing-entry fallback and preserve actual GPU tier/palette selection.
- Stop with `RR_TEXTURE_PREDECODE_EXPERIMENT=OFF` by default. No deployment,
  extra Ports launcher or commits. Known executable and backup hashes unchanged;
  EmulationStation restored (last verified PID 27340), no test game running,
  `/tmp` 16 KiB, pressure diagnostic removed. Existing captures/tools preserved.
- Device: `/userdata/system/rave-racer-predecode-20261014/`; host:
  `/tmp/rr-predecode/`. Final headline run `final-ab-20261014-092916-29756`.
  Report names all exact validation/probe directories and reproducible flags.

## Current state (2026-10-13)

- Repository: `BennyWebb/namco22-decompile`, branch `feature/knulli-h700`.
  Latest checked commit: `31a8a67` (`Multitexture issue investigation`).
  The current working tree contains the default-off four-texture prototype,
  validation tools and follow-up report; check Git status before new work.
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

### Ordered batching follow-up (2026-10-13, latest)

See [bounded batching follow-up](GLES2_MULTITEXTURE_FOLLOWUP.md). The user chose
to preserve the intentional cache-history policy and validate exact draws while
auditing independently selected resources. A trace identified the road key's
cap-1024 bake at tick 2499 and subsequent budget eviction. Another normal-runtime
pair differed at 187 pixels, all covered by polygons whose U/V bake resources
differed; source signatures, positions, colours and depth matched.

The four-explicit-sampler prototype is restored behind
`RR_GLES2_MULTITEXTURE_BATCH=OFF` and runtime `--gles-multitexture off|shader|batch`.
It requires ordered submission; batch mode requires frame VBO upload. No cache
policy, polygon ordering, shader colour correction, extra thread or queue change
was introduced. Existing texture mutation barriers remain effective.

Correctness passed: the controlled four-unit case; 2,296 exact live-reference
scenes through replay tick 6006; twelve original-HUD captures and deterministic
simulation/board/PCM/voice/ordered texture events in off/shader/batch and the actual
feature-disabled build; and 650 exact constrained-cache scenes with 262 mutation
barriers, 41,618 uploads and 41,489 evictions. No pixel tolerance was used.

Five alternating first-lap pairs (ticks 1800–5639) measured median **34.476 versus
35.276 rendered FPS (+2.32%)**, 18.189 versus 19.064 1% low FPS, 43.491 versus
40.704 ms p99 interval, 105.257 versus 97.376 ms median worst hitch, 59.665 versus
57.897 ms median image age, and 73.968 versus 71.454 ms p95 age (baseline/batched).
Draws fell 878.938 → 213.337; submission wall time 11.219 → 7.414 ms. Peak RSS rose
295.031 → 296.430 MiB. Median simulation Hz was 59.898/59.887; median underruns
zero in both. Pair 1 had 80,229/3,243 underrun samples and a baseline 1.504-second
hitch; the other eight runs had zero underruns. No run was discarded.

The separate diagnostic measured complete submission CPU 11.302 → 7.117 ms, but
the shader alone added 1.645 ms to the median 3D GPU command span. Swap wall time
in the unprofiled paired test rose 4.081 → 6.834 ms. Three dense-opening pairs
improved FPS 48.407 → 52.964 (+9.41%) while 1% lows/p99 worsened slightly. These
results do **not** establish a substantial representative racing FPS increase.
They are contemporaneous comparisons; do not mix their absolute FPS with the
older handover benchmark. Instrumented GPU runs are not headline FPS evidence.

**Retain the validated prototype disabled by default; it was not deployed.**
The improvement is repeatable but modest. The installed playable and backup
hashes above were verified unchanged, EmulationStation restored, and the single
active Ports launcher preserved. No commit was created. Artifacts are on the
handheld under `/userdata/system/rave-racer-multitexture-resume-20261013/` and
locally under `/tmp/rr-multi-resume/`; summaries and reproduction commands are
linked from the follow-up report. Existing captures/tools remain preserved.

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

## World RGBA atlas investigation — 2026-10-14

The committed GPU HUD production executable remains deployed and unchanged
(SHA256 `6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`).
The world-atlas prototype is rejected for production deployment and stays OFF.
See [architecture, correctness and decision](GPU_WORLD_ATLAS_INVESTIGATION.md)
and [five-pair results](benchmarks/world-atlas-20261014/RESULTS.md). No commits
or additional permanent launchers were created.

Compatible-dimension two-layer RGBA arrays preserve original UVs, bake tiers,
cache policy and ordered updates. Native complete recording, Mali controlled
first lap and TSan pass exact pixels/state/PCM/voices. Ordinary 2D atlas UV
remapping is not exact at nearest-sampling boundaries. Native zero-area fan
cases require original draw boundaries when merged after reordering; the
smallest reproductions are preserved in `tools/world_atlas_case/minimal/`.

Five alternating complete first-lap pairs: median FPS 52.931 -> 56.019,
world draws 860.7 -> 439.0, but 1% low 25.151 -> 21.699 FPS and worst hitch
91.9 -> 106.6 ms. Tick-1965 upload/driver time rises about 19 -> 53 ms with
similar decode time and miss count. RSS adds 44.2 MiB. Simulation remains
59.904 Hz; all ten runs have zero audio underruns/resyncs and identical state.

The optional source is behind `RR_GLES_WORLD_ATLAS=OFF` in the H700 preset
and runtime mode `off`. Preserve the captures and tools; these experiments
are complete evidence, not a request to restart atlas micro-optimisation.
The existing world renderer, GPU HUD, multicore/geometry optimisations and
controller configuration remain the playable reference.
