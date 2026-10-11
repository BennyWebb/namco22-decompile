# Rave Racer GLES2 Phase 1

`RR_GLES2=ON` builds `rr_gles2`, a 640×480 fullscreen scene viewer driven by
the existing deterministic scheduler. It runs the real translated 68K, DSP
and sound programs and board models. It uses scripted input; handheld controls
currently provide quit only. Sound emulation advances, but no audio device opens.
Keep the default desktop build and `RR_HEADLESS_ONLY=ON` in separate directories.

## Integration and limitations

`rr_main.c` calls the existing `rr_gl_prepare()` screen update. That retains
`rr_scene.c`'s DSP display handshake, direct-polygon decoding, the shared
`eng_walk_list()`/`geo_hw.c` object decoding and fixed-point projection, and
the original priority/depth ordering. CPU sorting, screen clipping and fan
indices were moved verbatim out of `engine/quad_gl.c` into
`engine/quad_geometry.inc`, included by both backends. The desktop renderer's
remaining implementation is unchanged.

`engine/quad_gles2.c` submits those projected vertices through a bounded VBO.
GLSL ES 1.00 shaders map screen coordinates to clip space using a matrix.
Per-viewport scissoring, priority ordering, depth testing and basic alpha
blending are enabled. Depth encodes sorted painter priority rather than eye
distance, retaining System 22's priority bands. Diagnostic colours distinguish
objects and palette selectors; they are deliberately not faithful game colours.
Existing perspective texture coordinates are carried through attributes and
clipping, but sample a white texture. All polygons are opaque in this phase.

The desktop `rr_host.c`, Nuklear, `tex_bake.c`, `render_target.c`, HUD/text layer,
fog, gamma/fade, framebuffer blits and pixel-map operations are excluded.
Texture ROM sampling, faithful palette/shading and translucent effects are
unsupported. This is a geometry milestone, not a complete playable renderer.
No generated game code was changed.

The original atlas is neither allocated nor referenced. Texture storage is
bounded by `RR_GLES2_TEXTURE_BUDGET_KB` (default 64 KiB); this phase allocates
one 1×1 RGBA texture, four texel bytes. Driver allocation overhead is additional.
CPU triangle storage and its GPU VBO are each 2,400,000 bytes. The actual quad
buffer allocation is logged when it grows. Allocation/GL errors terminate the
test rather than dropping geometry or fabricating texture references. There is
no texture cache or streaming policy yet; add one before enabling real textures.

## Build

Run from the repository root. GCC, CMake, make, host Python 3 and the existing
extracted ROMs are required. Native builds additionally need SDL2, GLES2 and EGL
development files. Nothing is downloaded or installed automatically.

```sh
cmake -S raverace -B /tmp/rr-gles2-native \
  -DRR_GLES2=ON -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/rr-gles2-native --target rr_gles2 -j2

cmake -S raverace -B /tmp/rr-gles2-arm64 \
  -DRR_GLES2=ON -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/knulli-aarch64.cmake \
  -DKNULLI_HOST="$HOME/dev/knulli-scarab/output/h700/host"
cmake --build /tmp/rr-gles2-arm64 --target rr_gles2 -j2

file /tmp/rr-gles2-arm64/rr_gles2
readelf -h -l -d --version-info /tmp/rr-gles2-arm64/rr_gles2
```

The existing toolchain file selects Buildroot GCC and its sysroot, and now
restricts pkg-config to that sysroot. It links target SDL2, GLESv2 and EGL;
libmali is linked explicitly when present (required by this SDK's wrapper DSOs). It does not link
desktop libGL or directly require X11, Wayland or DRM. Keep
`-fno-strict-aliasing -fwrapv` and the existing generated-code optimisation flags.
ROM extraction and desktop ZIP handling are not included: use the already
working `raverace/extracted/` directory from the headless milestone.

## Reproducible rendered-frame test

```sh
SDL_VIDEODRIVER=offscreen MESA_GLES_VERSION_OVERRIDE=2.0 \
  /tmp/rr-gles2-native/rr_gles2 raverace/extracted \
  --frames 600 --coin 180 --gas 200 --steer 300:2304 --unpaced \
  --capture-frame 600 --capture /tmp/rr-gles2-600.ppm \
  > /tmp/rr-gles2-600.json 2> /tmp/rr-gles2-600.log

/tmp/rr-headless-native/rr_headless raverace/extracted \
  --frames 600 --coin 180 --gas 200 --steer 300:2304 \
  > /tmp/rr-headless-600.json 2> /tmp/rr-headless-600.log
cmp /tmp/rr-gles2-600.json /tmp/rr-headless-600.json
```

Use the headless build commands in [HEADLESS.md](HEADLESS.md) first if needed.
Mesa offscreen is a desktop validation option, not a handheld driver. For a
visible desktop window omit the two environment assignments. Normal runs pace
to about 60 Hz; `--unpaced` disables pacing for validation.

The captured PPM is taken before buffer swap and vertically corrected.
Expect a flat-coloured car-selection layout at frame 600 with this input script.
Look for `captured frame 600: triangles=... draws=...` and inspect the actual
image, not just a nonzero count. Blank/fully occluded output remains a failure
even if triangles were submitted. Capture without geometry, GL/readback/write
failure, or failure to reach the requested capture frame returns status 2.
Simulation faults return status 3. No geometry totals or hashes are hardcoded.

For an actual 3D attract-mode object, run without coin/gas/steer:

```sh
SDL_VIDEODRIVER=offscreen MESA_GLES_VERSION_OVERRIDE=2.0 \
  /tmp/rr-gles2-native/rr_gles2 raverace/extracted --frames 3600 \
  --unpaced --capture-frame 3600 --capture /tmp/rr-gles2-attract3600.ppm \
  > /tmp/rr-gles2-attract3600.json 2> /tmp/rr-gles2-attract3600.log
```

## Deploy to Knulli Ports

Replace `KNULLI_IP` below with the device's SSH address. These are manual setup
commands; no system files are changed and no target libraries are replaced.

```sh
KNULLI_IP=192.168.1.100
ssh "root@$KNULLI_IP" 'mkdir -p /userdata/roms/ports/rave-gles2'
scp /tmp/rr-gles2-arm64/rr_gles2 \
  "root@$KNULLI_IP:/userdata/roms/ports/rave-gles2/"
scp -r raverace/extracted \
  "root@$KNULLI_IP:/userdata/roms/ports/rave-gles2/"
scp raverace/tools/RaveRacer-GLES2.sh \
  "root@$KNULLI_IP:/userdata/roms/ports/"
ssh "root@$KNULLI_IP" \
  'chmod +x /userdata/roms/ports/RaveRacer-GLES2.sh /userdata/roms/ports/rave-gles2/rr_gles2'
```

Refresh EmulationStation's game list, open **Ports**, and launch
**RaveRacer-GLES2**. The launcher selects `mali` unless `SDL_VIDEODRIVER` is
already set, changes into the executable's directory, and preserves its exit
code. It runs 3,600 scripted frames, captures frame 600, and appends stdout and
stderr to `/userdata/roms/ports/rave-gles2/rave-gles2.log`. The PPM is
`/userdata/roms/ports/rave-gles2/frame600.ppm` and is replaced each launch.
Hold **START + SELECT** on the same controller to quit (SDL START+BACK).
Escape, SDL quit and the fixed frame limit also exit.

```sh
ssh "root@$KNULLI_IP" 'tail -n 100 /userdata/roms/ports/rave-gles2/rave-gles2.log'
scp "root@$KNULLI_IP:/userdata/roms/ports/rave-gles2/frame600.ppm" /tmp/rg40-frame600.ppm
scp "root@$KNULLI_IP:/userdata/roms/ports/rave-gles2/rave-gles2.log" /tmp/rg40-gles2.log
```

For the identical 600-frame state test, run the launcher from SSH, with
EmulationStation's graphics ownership released using the same procedure as the
working triangle test. Do not run two framebuffer applications together:

```sh
ssh "root@$KNULLI_IP" '/userdata/roms/ports/RaveRacer-GLES2.sh --frames 600 --unpaced'
```

The launcher log contains the final headless JSON plus diagnostics. To compare
a separate JSON file directly, run the executable from its directory:

```sh
cd /userdata/roms/ports/rave-gles2
SDL_VIDEODRIVER=mali ./rr_gles2 extracted \
  --frames 600 --coin 180 --gas 200 --steer 300:2304 --unpaced \
  --capture-frame 600 --capture frame600.ppm > state600.json 2> test600.log
```

Retrieve `state600.json` using scp and compare it to `/tmp/rr-headless-600.json`.
For the 3D attract-mode test, use the executable directly with `--frames 3600
--capture-frame 3600 --capture attract3600.ppm`, without scripted input.

## Troubleshooting and validation status

Check the active SDL driver, vendor, renderer, ES version, drawable dimensions,
shader diagnostics, first-geometry frame and five-second FPS/draw counters in
the log. The drawable must be 640×480 with a depth buffer of at least 16 bits.
An unmapped joystick is reported explicitly: supply a known SDL controller
mapping through `SDL_GAMECONTROLLERCONFIG` if START+SELECT does not work.
This uses the same mapped-controller quit combination as the working triangle
test. The frame limit remains a fallback; handheld button mapping is unverified.

For startup errors, run `ldd ./rr_gles2` on the device. A `not found` entry
identifies a missing target library. An apparent missing executable may mean
the ELF interpreter `/lib/ld-linux-aarch64.so.1` is missing. Use host
`readelf --version-info` to inspect required glibc versions if the loader reports
`GLIBC_x.y not found`. Do not copy Ubuntu libraries onto Knulli. Rebuild using
the matching Buildroot sysroot instead. EGL/GLES driver or framebuffer errors
need target testing; retain the working triangle as the stack comparison.

Native validation used Mesa llvmpipe with ES 2.0 forced: GLSL ES 1.00 compiled
and linked, the scripted frame 600 submitted 180 triangles in one draw call,
and its 640×480 capture visibly contained the game's car-selection geometry.
The final JSON exactly matched the equivalent headless run. An unscripted
3,600-frame run also completed without traps/faults; frame 3600 contained a
recognisable 3D attract-mode character (1,524 triangles, one draw call).
The default desktop executable built and the existing headless regression
suite passed. The Ports launcher passed working-directory, argument forwarding,
driver override, log and exit-code checks; incomplete capture returned status 2.
Neither native rendering test establishes handheld GPU success.

The cross build completed with H700 Buildroot GCC 13.3.0. `file`/`readelf`
confirm a little-endian AArch64 ELF for Linux 4.9 with interpreter
`/lib/ld-linux-aarch64.so.1`. Direct dependencies are `libm.so.6`,
`libGLESv2.so.2`, `libEGL.so.1`, `libmali.so.0`, `libSDL2-2.0.so.0`,
`libc.so.6` and the loader; the highest executable glibc requirement is 2.34.
The inspected SDK SDL/EGL/GLES/Mali dependencies contain no desktop libGL,
X11, Wayland or DRM library. These checks establish build compatibility;
the installed device SONAMEs and transitive driver requirements still need
`ldd` and a real launch on the device.

The SDK's SDL pkg-config metadata reports 2.32.8, while the confirmed handheld
SDL was reported as 2.0.32. This backend uses older SDL2 APIs, logs both compiled
and runtime versions, and uses the target SDL ABI, but the version discrepancy
still requires device confirmation. Mali shader precision, fullscreen/depth
configuration, swap timing, controller mapping, readback and real memory usage
remain unverified on the RG40XX H. The next step is to launch through Ports,
inspect the display and frame600.ppm, and compare the final JSON before adding
texture support or optimising.
