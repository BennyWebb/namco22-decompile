# H700 GPU HUD production integration — 2026-10-14

## Status

Integrated and deployed to the existing **Ports → Rave Racer** launcher.
The user accepted the HUD/gameplay trial and independently confirmed steering,
controller operation and audible synchronised sound. Native and ARM64 correctness
regressions and the final representative benchmark passed before replacement.
EmulationStation restored (last checked PID 23050), no game running, one launcher.
No commits. Existing captures, diagnostics and older backups are preserved.

## Configuration

Build from `raverace/`:

```sh
cmake --preset playable-knulli
cmake --build --preset playable-knulli --target rr_gles2
```

`playable-knulli` enables `RR_GPU_HUD` and `RR_GPU_HUD_DEFAULT`, retaining the
original CPU compositor. With no environment overrides the validated glyph-quad
implementation runs. `ENG_GPU_HUD=0` selects CPU HUD. Explicit
`ENG_GPU_HUD_GEOMETRY=0` remains a diagnostic packed-tilemap selector and is not
used in production.

Validated optimisations remain enabled: threaded presentation, parallel frame
preparation, ordered submission, geometry index ordering and projection reuse,
fast split memory reads, inactive C352 fast path and texture tile runs.
Multi-texture batching, texture pre-decoding, preparation wake experiment and
profiling/counter options remain OFF. No world/cache/emulation changes.

The consolidated Ports script is `tools/RaveRacer.sh`. Missing mode file or
`gpu-hud-mode=default` uses the build default; `off` selects CPU HUD and `on`
explicitly selects GPU glyph-quads. It clears capture/verification and texture
experiment environment settings and loads the previously installed
`controller-input.cfg` when present. Replay recording remains OFF for deployment.
Interactive flags: independent timing, dropped-picture geometry skipping,
64 MiB texture cache, frame VBO, audio ON, original HUD, overlay OFF.

## Completed local validation

- Clean H700 preset ARM64 build: `build/playable-knulli/rr_gles2`, SHA-256
  `6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`.
- Native GLES build with equivalent flags, SHA-256
  `605e8455137ff747358b8a2f4ed56d8eba0f2ca2751bab2ebbe000b4722236a9`.
- Rebuilt frozen HUD probe: 14 preserved captures + 32 synthetic cases, all
  14,131,200 RGBA pixels exact. Menus, palette/scroll changes, shadows,
  transparency, tile flips, character aliasing and priority included.
- Full first-lap recording through 6006, fixed one-in-three presentation:
  CPU fallback/default GPU final pixels, simulation JSON, board, geometry,
  PCM/voice digests and ordered texture misses identical. All 2,002 GPU
  pictures independently verified against the original CPU compositor.
  PCM `0c24e8fca4b6400e`, voices `b76f90445ba87e18`, board `f9154a83`.
- Rebuilt ThreadSanitizer three-stage run through 1700: 567 verified GPU
  pictures; pixels, state, geometry and PCM/voices match the ordinary build.
  First five simulation/board/audio checkpoints match the human recording.
  No application race reports; only the existing Mesa `libgallium` suppression
  was used. Uninstrumented Mali driver internals are outside this check.
- SDL virtual controller regression passes mapping, coin release, START,
  brake, analog/deadzone, D-pad, script isolation and quit.
- Retained palette snapshot/change/revert/wrap regression passes.

## Final ARM64 correctness

Device directory: `/userdata/system/rave-racer-gpu-hud-consolidated-20261014/`.
Run `validation-20261014-152511-6403` exited zero:

- Fixed replay through 6006: all 2,002 selected RGBA pictures match the original
  CPU compositor, final capture/simulation/geometry/digests and ordered misses
  match the preserved pre-GPU reference. All 63 checkpoint records (21 sets of
  simulation, board and PCM/voice state) match the human recording byte for byte.
- Adaptive replay with ALSA audio through 5639: all 913 selected pictures exact
  under normal cache/resource selection. PCM `a18439c518e08e3d`, voices
  `c12e04d613b280fc`, final sound `a588e1f884d1c5f8`, same endpoint state.
  Zero audio underruns/overruns/resets. Verification readbacks reduced throughput
  and caused one 22.611 ms clock resync; this diagnostic run is not a gameplay
  performance measurement. Verification is OFF in all performance/deployed runs.
- CPU HUD fallback and serial GPU HUD at tick 600: identical pixels/state.
- Production launcher startup: controller recognised with the exact saved
  RG40XX-H mapping, ALSA 48 kHz stereo/512 samples/gain x6, three-stage pipeline,
  glyph-quads `verify=0`, fixed CPU HUD image allocation zero, startup underruns
  zero. The user's physical controller/audio confirmation applies to the accepted
  GPU build; consolidated state/PCM/voice equivalence and startup are rechecked.

## Final representative performance confirmation

Two alternating A/B pairs, accepted experimental GPU build (`fef8f66d…`) versus
consolidated build (`6e1cac00…`), ticks **1800–5639**, original first-lap recording.
Order: experimental/consolidated, consolidated/experimental. Same adaptive
three-stage pipeline, normal cache policy, 64 MiB budget, frame VBO, live audio,
profiling and monitor method. No cache forcing, GPU HUD verification or clock
policy changes. Raw run `ab-20261014-153427-7324`. These two confirmation pairs
supplement the preserved original five-pair architecture experiment.

| Metric (median of two runs) | Accepted GPU prototype | Consolidated playable |
|---|---:|---:|
| Rendered FPS | 52.728 | **52.939** |
| 1% low FPS | 25.214 | 25.297 |
| p99 presentation interval | 32.802 ms | 32.657 ms |
| Worst interval per run | 82.582 ms | 80.886 ms |
| Image age median / p95 | 38.706 / 54.726 ms | 37.370 / 54.748 ms |
| Simulation Hz | 59.904 | 59.905 |
| Preparation CPU / picture | 3.784 ms | 3.779 ms |
| HUD preparation CPU / picture | 0.048 ms | 0.045 ms |
| HUD upload/update CPU / picture | 0.994 ms | 1.010 ms |
| GLES owner CPU / picture | 16.991 ms | 16.910 ms |
| Peak RSS | 286.924 MiB | 288.547 MiB |
| Audio underruns / clock resyncs | 0 / 0 | 0 / 0 |

All four endpoint states match; whole-process audio underruns, overruns and
latency resets are zero. Consolidated runs: 52.963 and 52.915 FPS; experimental:
52.744 and 52.712 FPS. Worst individual intervals: 83.582 ms consolidated,
86.722 ms experimental. Simulation rounding reflects elapsed measurement,
not a changed target: target remains 59.904059904 Hz.

The earlier five-pair GPU result was 52.714 FPS, 25.151 FPS 1% low and 33.027 ms
p99. Consolidation retains that performance. The +0.4% versus the fresh prototype
comparison is small and is not a new optimisation claim. Median RSS is 1.62 MiB
higher in this confirmation; both paths share the same buffer layout and GPU
allocation limits, while presentation/cache histories differ. No new CPU HUD
image allocation is enabled. The legacy `[PREP-RATE] extra_hud_bytes` field
reports unconditional buffer capacity; use startup `fixed_hud_bytes=0` and
measured RSS for allocation evidence.

Temperatures during measured windows: 54.1–64.9°C experimental, 56.3–64.4°C
consolidated. CPU frequencies range 1008–1512 and 1416–1512 MHz respectively;
most samples are 1512 MHz. `schedutil` CPU and `simple_ondemand` GPU policies
unchanged. Do not infer GPU-exclusive timing from these CPU/submission scopes.
Sustained 59.904 rendered FPS is still not achieved.

Summary: [benchmark JSON](benchmarks/gpu-hud-production-20261014-summary.json).
Host raw data/scripts `/tmp/rr-hud/consolidated/`, device directory as above.

## Deployment, CPU fallback and rollback

Installed executable `/userdata/roms/ports/rave-gles2/rr_gles2`, SHA-256
`6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`.
Existing launcher `/userdata/roms/ports/Rave Racer.sh`, mode file `default`,
replay-mode `off`. Exact device-specific controller configuration retained.
No additional permanent launcher.

Exit with START+SELECT before changing configuration or restoring.
Select original CPU HUD without replacing the production executable:

```sh
ssh root@192.168.1.69 'printf "off\n" > /userdata/roms/ports/rave-gles2/gpu-hud-mode'
```

Return to production GPU default:

```sh
ssh root@192.168.1.69 'printf "default\n" > /userdata/roms/ports/rave-gles2/gpu-hud-mode'
```

Fresh backup of the **previous manually accepted GPU playable executable**, exact
launcher, both mode files and controller mapping:
`/userdata/system/rave-racer-backups/20261014-pre-consolidated-gpu-hud/`.
Manifest verified before and after deployment. Restore it with:

```sh
ssh root@192.168.1.69 \
  /userdata/system/rave-racer-backups/20261014-pre-consolidated-gpu-hud/restore.sh
```

The pre-GPU CPU executable/configuration backup is also preserved:

```sh
ssh root@192.168.1.69 \
  /userdata/system/rave-racer-backups/20261014-pre-gpu-hud-interactive/restore.sh
```

Its SHA-256 remains `77536f56ad17de9cf8772175c73442d9cb246d87aeb6fbf6c8d7f07c3d6ab53e`.
The earlier pre-parallel-prep backup remains
`ed27bc04b41c402e2181a1f36c4b3876becffea6224a9499bd9a181120dbedb1`.
Restore scripts check saved hashes and refuse while the game is running.
No commits or unrelated renderer changes.
