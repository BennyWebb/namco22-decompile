# Revised native frozen-scene renderer — M2 COMPLETE

See [acceptance report](../../NATIVE_FROZEN_SCENE_REPORT.md) and
[asset format/reproduction](../assets/CONSOLIDATED_FORMAT.md). No simulation,
production renderer integration or live palette update is included.

```sh
cc -O2 -Wall -Wextra raverace/native/render/native_scene.c -o /tmp/native-host $(pkg-config --cflags --libs sdl2 glesv2) -lm
/home/bwebb/dev/knulli-scarab/output/h700/host/bin/aarch64-buildroot-linux-gnu-gcc -O2 -Wall -Wextra raverace/native/render/native_scene.c -o /tmp/native-arm -lSDL2 -lmali -lm
mkdir /tmp/new-native-run
cd /tmp/new-native-run
SDL_VIDEODRIVER=offscreen /tmp/native-host /tmp/rr-m2rev/scene-canonical 30 0 > run.log 2>&1
```

On H700 use `SDL_VIDEODRIVER=mali XDG_RUNTIME_DIR=/var/run`, with no game running,
a temporary executable and results directory. Stop EmulationStation only for
the test and restore it with an EXIT trap. Never replace the production binary.
Arguments are scene directory, timed samples per mode, selection (0 complete;
1 terrain1280/car1945 small gate). Five warm-up passes per mode precede timing.
Mode0 uses independent source textures; mode1 native consolidated pages. Mode0
is not the incumbent; `frozen.c` supplies streamed/resident incumbent references.
All uploads and clipping occur before samples. Outputs are exclusive-created
`source.rgba`/`native.rgba`, bottom-up 640x480 raw RGBA. Preserve alpha priority;
PNG previews display RGB only.

```sh
python3 raverace/native/tools/compare_native.py reference.rgba native.rgba /tmp/new-compare
python3 raverace/native/tools/analyse_native.py --help
```

`compose_hud.c` is an isolated adapter around the existing production GPU HUD
probe. Its build requires a generated `hud_cpu_reference.inc`: extract
`raverace/src/rr_gl.c` from `static bool build_text(` to the following
`#ifndef RR_GLES2` / `/* *last:` marker into a temporary include directory.
Compile with `-DRR_GPU_HUD_DEFAULT -Iraverace/include -Iengine -I/tmp/include-dir`
and SDL2/GLES2 (ARM: SDL2/mali), `-lm`. Run with captured HUD state prefix,
candidate raw world and new output RGBA path. It compares the exact CPU/GPU HUD
composition and original opaque glyph/priority pixels. It does not measure live
HUD performance. Accepted outputs and logs are under `/tmp/rr-m2rev/hud-capture`.

---

## Historical failed projected-geometry probe

# Resident frozen-scene feasibility probe (Milestone 2)

**FAILED/RETHINK; not a completed native object renderer.** Read
[the report](../../NATIVE_FROZEN_SCENE_REPORT.md) before using these results.
The probe intentionally reuses captured projected geometry to measure a resident
ordinary-RGBA submission design without simulation or source transforms. Reusing
this data does not itself fulfil native model/instance integration.

Build from repository root:

```sh
cc -O2 -Wall raverace/native/render/frozen.c -o /tmp/rr-m2-frozen-host $(pkg-config --cflags --libs sdl2 glesv2) -lm
/home/bwebb/dev/knulli-scarab/output/h700/host/bin/aarch64-buildroot-linux-gnu-gcc -O2 raverace/native/render/frozen.c -o /tmp/rr-m2-frozen-arm -lSDL2 -lmali -lm
mkdir -p /tmp/rr-m2-new-host-results
cd /tmp/rr-m2-new-host-results
SDL_VIDEODRIVER=offscreen /tmp/rr-m2-frozen-host /tmp/rr-atlas/frozen 30 > run.log 2>&1
```

Input is the preserved **actual** tick-2500 capture directory: `scene.meta`,
`draws.bin`, `texture-N.rgba`, `reference.rgba`. Formats are the incumbent frozen
capture contract: little-endian six-uint32 draw header (texture ID, count, four
scissor components) then `count` tightly packed ten-float32 vertices (XYZ,
RGBA, UVQ). Texture files have ASCII `width height\n` then RGBA8. Scene metadata
is RGB background, opaque-priority flag, depth bits. This is not a new scene
command protocol. The loader rejects nonopaque scenes, inadequate synthetic
depth separation, oversized resources and invalid/truncated draw data.
The prototype's expected Rave vertex ABI is 40 bytes; it does not link the
production renderer's compile-time feature variants.

Use a fresh output directory: framebuffer files use exclusive creation.
Mode 0 streams original order; mode 1 is resident original order; mode 2 is
resident grouped indexed submission. All resources load before warm-up/timing.
Indices require actual GLES3 or OES unsigned-index support. No arrays or extra
shader texture lookups are used. Mode 0 has a separate streamed VBO and is not
included in the claim of zero steady-state uploads for resident modes.

Independent source/mesh metadata validation (repository root):

```sh
python3 raverace/native/assets/asset_tool.py inspect /tmp/rr-m1-visible-pack
python3 raverace/native/tests/validate_assets.py raverace/extracted /tmp/rr-m1-visible-pack /tmp/rr-atlas/frozen /tmp/rr-native-texture/palette.rgb
python3 raverace/native/tools/export_frozen_instances.py raverace/extracted /tmp/rr-m1-durable-trace/capture /tmp/rr-m1-visible-pack /tmp/rr-m2-new-instances --scene /tmp/rr-atlas/frozen
python3 raverace/native/tools/analyse_frozen.py /tmp/rr-m2/device-final /tmp/rr-atlas/frozen /tmp/rr-m2-new-analysis
```

The source exporter requires the actual M1 tick-2500 snapshot (`poly.i32`,
`pointram.i32`, `state.json`) and original ROM/converted asset inputs. Regenerate
those using M1's documented private source trace if `/tmp` was cleared. It checks
the frozen scene and snapshot have identical endpoint state, validates packet,
material and raw-coordinate association, and retains combined model/view Q15
matrices and lens parameters. It does not invent a camera or run the simulation.
The timed GPU probe does not yet consume these object-space records.

H700 command pattern (existing authenticated SSH connection; no passwords saved):

```sh
scp -F /dev/null -o ControlPath=/tmp/rr-m2-ssh /tmp/rr-m2-frozen-arm root@192.168.1.69:/tmp/rr-m2-frozen-check
ssh -F /dev/null -S /tmp/rr-m2-ssh root@192.168.1.69 '
if pidof rr_gles2 >/dev/null; then exit 4; fi
sha256sum /userdata/roms/ports/rave-gles2/rr_gles2
stopped=0
if pidof emulationstation >/dev/null; then
  /etc/init.d/S31emulationstation stop >/tmp/rr-m2-es-stop.log 2>&1
  stopped=1
fi
trap '\''if [ "$stopped" = 1 ]; then /etc/init.d/S31emulationstation start >/tmp/rr-m2-es-start.log 2>&1; fi'\'' EXIT
mkdir -p /tmp/rr-m2-new-results/run1
cd /tmp/rr-m2-new-results/run1
SDL_VIDEODRIVER=mali XDG_RUNTIME_DIR=/var/run /tmp/rr-m2-frozen-check /userdata/system/rave-racer-world-atlas-20261014/frozen-20261014-160318-25197 30 > run.log 2>&1
'
```

Use new `run2`/`run3` directories for independent process repeats. Finish all
transfers before benchmarks. Restore/check frontend and production hashes after
completion; do not install this executable or add a Ports launcher. All timing
samples include explicit diagnostic completion and are not live-game FPS.

Archive numeric logs only. Scene outputs, palettes, original meshes, shader
input captures and screenshots contain game assets and stay out of Git.
