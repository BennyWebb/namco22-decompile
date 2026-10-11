# Rave Racer GLES2 Phase 2 — original textures

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
distance, retaining System 22's priority bands. Original CG/CCR ROMs, cmode decoding, palette resolution and baking are
reused directly from `engine/tex_bake.c`. Perspective texture coordinates use
the desktop UV bounding rectangle, degenerate expansion, texel centres,
returned bake scale/origin and per-fragment division. Per-vertex brightness
multiplies RGB by brightness/64, including overbright values. Object-flag solid
polygons use the original palette-pen rules. See
[TEXTURE_PIPELINE.md](TEXTURE_PIPELINE.md) for the pre-implementation investigation.

Nearest filtering, clamp-to-edge and the existing guard texels are retained.
ROM coordinate wrapping happens in the decoder, not by repeating a baked
rectangle. RGBA/UNSIGNED_BYTE uploads preserve the decoder's alpha; System 22
polygon pen zero is intentionally opaque, as on desktop. Palette generation
changes invalidate bakes on demand. `--diagnostic-colours` explicitly restores
the Phase 1 debug colours instead of sampling original textures.

The GLES2 path forces the existing per-texture cache rather than an atlas.
It allocates on demand up to `RR_GLES2_TEXTURE_BUDGET_KB` (default 65536 KiB,
64 MiB), including the four-byte white solid/debug texture. Runtime
`--texture-budget-mib N` selects 1..64 MiB, no larger than the build limit.
Eviction occurs
before allocating, deletes storage instead of retaining a recycled-object
pool, and flushes pending draws before referenced storage changes. Cold bakes
are full-quality under the existing desktop sampling/cap rules; coarse
placeholders and deferred refinement are disabled. A texture larger than the
configured budget or a GL allocation/upload error terminates with a diagnostic
and status 2 rather than silently dropping textures or raising the budget.
Driver overhead and storage retained for already-issued GPU commands are not
measurable via GLES2; the log accounts for application-owned texel storage.

No atlas shadows or CPU decoded-texture cache are allocated. CPU usage includes
18.5 MiB of ROM data/tile maps, the logged cache metadata, a single ~4 MiB shared
bake scratch buffer, and the existing geometry storage (2,400,000 bytes each
for CPU triangle staging and the maximum current GPU VBO store, plus 114,688
bytes of ordered draw descriptors and logged quad allocations). Per-texture
binding produces more draw calls than an atlas; performance tuning is deferred.
Counters report cumulative decoded bakes/uploads/hits/misses/evictions,
resident/peak GPU bytes and CPU metadata/scratch usage every five seconds,
at capture and at shutdown. ROM allocations and supported cmodes are logged
at startup. All cmode variants supported by the desktop decoder are reused.

The backend requires GL_MAX_TEXTURE_SIZE >= 1024 and fragment highp precision
for faithful perspective UVs; both are checked and logged before texture
allocation. No GL_UNPACK_ROW_LENGTH operation is compiled into this path.
The desktop `rr_host.c`, Nuklear, render targets, HUD/text, fog, gamma/fade,
framebuffer blits and pixel-map operations remain excluded. Textures and
palette shading are supported; faithful final composition, translucency
effects, audio output and playable controller integration remain outside scope.
No generated game code was changed.

## Build

Run from the repository root. GCC, CMake, make, host Python 3 and the existing
extracted ROMs are required. Native builds additionally need SDL2, GLES2 and EGL
development files. Nothing is downloaded or installed automatically.

```sh
cmake -S raverace -B /tmp/rr-gles2-native \
  -DRR_GLES2=ON -DRR_GLES2_TEXTURE_BUDGET_KB=65536 -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/rr-gles2-native --target rr_gles2 -j2

cmake -S raverace -B /tmp/rr-gles2-arm64 \
  -DRR_GLES2=ON -DRR_GLES2_TEXTURE_BUDGET_KB=65536 -DCMAKE_BUILD_TYPE=Release \
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

SDL_VIDEODRIVER=offscreen MESA_GLES_VERSION_OVERRIDE=2.0 \
  python3 raverace/tools/test_gles2_textures.py \
  /tmp/rr-gles2-native/rr_gles2 /tmp/rr-headless-native/rr_headless raverace/extracted
```

Use the headless build commands in [HEADLESS.md](HEADLESS.md) first if needed.
Mesa offscreen is a desktop validation option, not a handheld driver. For a
visible desktop window omit the two environment assignments. Normal runs pace
to about 60 Hz; `--unpaced` disables pacing for validation.

The captured PPM is taken before buffer swap and vertically corrected.
Expect the original track-selection cards at frame 600 with this input script.
Repeat with `--diagnostic-colours` to compare the same geometry without textures.
The budget is a CMake cache setting: explicitly update an existing Phase 1 build
directory, whose cached 64 KiB value is too small for authentic textures.
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

For city track/traffic geometry with the same initial menu selection and
steering centred before the race:

```sh
SDL_VIDEODRIVER=offscreen MESA_GLES_VERSION_OVERRIDE=2.0 \
  /tmp/rr-gles2-native/rr_gles2 raverace/extracted --frames 1400 \
  --coin 180 --gas 200 --steer 300:2304 --steer 700:2048 --unpaced \
  --capture-frame 1400 --capture /tmp/rr-gles2-city1400.ppm \
  > /tmp/rr-gles2-city1400.json 2> /tmp/rr-gles2-city1400.log
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
**RaveRacer-GLES2**. Original textures are now the default. The launcher selects `mali` unless `SDL_VIDEODRIVER` is
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

Phase 1 native validation established matching rendered/headless simulation
state and visible diagnostic geometry. Phase 2 validation results are recorded
below after rebuilding and capturing original textures. No Phase 2 handheld
validation has been performed; the previously committed Phase 1 renderer was
confirmed on the RG40XX H by the user.

Phase 2 native validation used Mesa llvmpipe with ES 2.0 forced:

- Frame 600 visibly contains the original track-selection cards/background:
  180 triangles, 90 draws, 660 decoded/uploaded bakes, 15,665,192 GPU bytes,
  no evictions. Final JSON exactly matches headless and diagnostic-colour runs.
- A 4 MiB run produced identical frame-600 pixels and JSON while exercising
  20,676 evictions; measured peak 4,194,300 bytes stayed below 4,194,304.
- A 1 MiB run failed safely because a single RGBA bake needs 1,228,800 bytes.
  The configured budget was not raised and no fallback texture was substituted.
- An unscripted 3,600-frame run captured an original-textured 3D attract-mode
  character (1,524 triangles, 760 draws). Its final JSON matched headless,
  with no traps/DSP/sound faults. Peak 67,108,860 bytes stayed below
  67,108,864; 13,394 evictions indicate cache pressure in the longer run.
- Logged CPU ROM storage is 19,398,656 bytes, cache metadata 4,456,448 bytes,
  auxiliary static bookkeeping 310,816 bytes, shared scratch 4,202,500 bytes;
  atlas shadows and retained decoded cache are
  zero. These figures exclude the game/sound/geometry memory and driver overhead.
- Scripted frame 1400 with steering centred at frame 700 visibly contains
  textured city buildings, track, traffic vehicles and the rear-view viewport;
  peak texture storage was 52,685,796 bytes with no evictions.
- The existing desktop target built, the headless regression suite passed,
  and the new GLES2 regression checked textures/debug mode, eviction pixels,
  memory bounds, simulation equality and undersized-budget failure.

The Phase 2 cross build completed with H700 Buildroot GCC 13.3.0. `file`/`readelf`
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
is logged at startup; the Phase 1 executable already worked on the device.
Phase 2 Mali shader precision, texture uploads, eviction behaviour, performance
and real memory usage remain unverified on the RG40XX H. The next step is to launch through Ports,
inspect the display and frame600.ppm, and compare the final JSON before broadening
texture coverage or optimising.

## Draw-fragmentation diagnostic build

The following modes preserve polygon order, priority depth, clipping and scissor
changes. No texture sorting or geometry reordering is performed.

| `--render-mode` | Geometry/batch boundaries | Sampled texture |
| --- | --- | --- |
| `textured` (default) | Existing ordered per-texture batches | Original ROM textures |
| `diagnostic` | Existing diagnostic-colour batching; no texture lookups | White, with diagnostic vertex colours |
| `white-batches` | Exactly the textured path's boundaries, lookups, cache and uploads | One white texture, with normal vertex shading/solid colours |
| `textured-no-rebind` | Same as textured | Original textures; shared binding tracker skips redundant binds |

`--diagnostic-colours` remains an alias for diagnostic mode. White-batches
still builds/updates the original cache to keep its workload and mutation
boundaries comparable; it isolates sampling/texture switching from the many
small draw submissions in warmed intervals, not cold texture decoding. It
intentionally retains per-draw bind requests even though they name white.
The no-rebind tracker is shared with the baker, is invalidated on deletion and
at every frame boundary, and does not query GL state per quad. It remains
opt-in for comparison rather than changing the default renderer.

`[PROFILE]` reports aggregate averages in milliseconds per measured frame,
every five seconds, a final partial window and a cumulative `total` at exit.
`--profile-from F` excludes earlier frames from these aggregates (default 1).
It does not skip their simulation/rendering or populate a saved state.

- `simulation`: elapsed time outside the rendering host, including the actual
  68K/DSP/sound scheduler, frame-tail device/input work and scheduler logs.
- `scene`: existing scene generation, geometry decoding and sorting.
- `lookup_decode`: baker/cache calls, including eviction housekeeping, with
  nested texture-upload and mutation-triggered draw time subtracted.
- `uploads`: texture binding/setup, allocation and RGBA upload calls on misses.
- `submission`: texture bind, vertex-buffer update, glDrawArrays and error check.
  `vertex_upload` (glBufferSubData) and `draw_call` (glDrawArrays) are **subsets**
  of submission; do not add them again to the total.
- `swap`: SDL_GL_SwapWindow. `render_other` covers rendering state setup, CPU
  vertex preparation and clipping. Capture and software pacing are separate.
- Triangle/draw/bind averages and skipped bindings are reported alongside
  nonempty flush counts by texture change, scissor, capacity, mutation and end.
  `same_texture` counts quads retaining the current logical batch texture.
  `empty_texture_flush` counts texture breaks caused by a quad subsequently
  clipped away; it identifies avoidable boundaries without changing them.

These are CPU-side **wall-clock** intervals: driver waits/backpressure may
appear in upload, vertex update, draw, other rendering or swap. They are not
GPU timings or CPU-thread execution-time measurements. No glFinish is inserted
and no GPU timing extensions are required. Timing probes add some overhead;
compare all modes using this same executable. Avoid screenshots during timing
runs, as readback forces synchronization and affects subsequent work.

### Build and run 120–300-frame handheld tests

Use the ARM64 build commands above, explicitly keeping the 65536 KiB budget
when configuring an old build directory. Copy the rebuilt executable as usual.
For a separate short Ports entry:

```sh
scp raverace/tools/RaveRacer-GLES2-Profile.sh \
  "root@$KNULLI_IP:/userdata/roms/ports/"
ssh "root@$KNULLI_IP" 'chmod +x /userdata/roms/ports/RaveRacer-GLES2-Profile.sh'
```

**Ports → RaveRacer-GLES2-Profile** runs 300 scripted frames with original
textures, no screenshot, unpaced, and profiles frames 100–300. Its persistent
log is `/userdata/roms/ports/rave-gles2/textured-profile.log`. To test another
mode from Ports, change the launcher's `mode=${1:-textured}` default to one of
the table's names. This changes only the copied user launcher.

With framebuffer ownership released as for the triangle test, SSH commands
for all four independent runs are:

```sh
ssh "root@$KNULLI_IP" '/userdata/roms/ports/RaveRacer-GLES2-Profile.sh textured'
ssh "root@$KNULLI_IP" '/userdata/roms/ports/RaveRacer-GLES2-Profile.sh diagnostic'
ssh "root@$KNULLI_IP" '/userdata/roms/ports/RaveRacer-GLES2-Profile.sh white-batches'
ssh "root@$KNULLI_IP" '/userdata/roms/ports/RaveRacer-GLES2-Profile.sh textured-no-rebind'
```

Each produces `<mode>-profile.log` in the application directory and preserves
its exit code. Override the length with, for example,
`RaveRacer-GLES2-Profile.sh textured --frames 120 --profile-from 100`.
At 1 FPS a 300-frame test can still take minutes. These fresh-start tests
include boot/menu scenes; 120 frames barely reaches initial geometry. Prefer
300 and compare the same frame intervals, inputs, budget and swap setting.
They may not reproduce the much denser race scene in the supplied log.

For a comparable 300-frame interval of city/traffic geometry after warm-up,
use each mode with `--frames 1500 --profile-from 1201 --steer 700:2048`.
This measures 300 frames but must still execute/render the first 1200; there
is no save-state shortcut. Compare the five-second windows as well as the
cumulative line, and use texture-counter differences between windows to
separate cold-cache work from sustained slowdown. Pacing is disabled and swap
interval 0 is requested; the log reports if the driver rejects that request.

Retrieve logs:

```sh
scp "root@$KNULLI_IP:/userdata/roms/ports/rave-gles2/*-profile.log" /tmp/
```

### Findings and next optimisation decision

Source inspection confirms consecutive quads with the same baked texture and
scissor already combine. Mutations flush only when pending vertices reference
the affected texture; empty flush calls do not issue draws. The main source of
fragmentation is distinct per-bbox/palette/cmode texture IDs in painter order.
Scissor changes, capacity and mutation counts establish other causes. The
current implementation can also break a batch before discovering that the
next quad clips away; the new counter measures that opportunity.

Every nonempty flush writes the same VBO offset zero before drawing. A driver
may wait for preceding GPU reads at each write. This is an additional suspect,
not proof that draw-call count alone causes the handheld's sustained 1 FPS.
Rare texture uploads do not rule out sampling, binding, buffer hazards or GPU
backpressure. Cumulative cache-hit counts alone do not measure lookup cost.

Native Mesa ES2 validation at scripted frame 600 confirmed identical final
JSON across the controls and headless, identical original/no-rebind pixels,
and matching original/white-batches/no-rebind flush boundaries. In an initial
native profile of frames 100–600, all three fragmented modes had 38,722 texture
flushes plus 495 end flushes and no scissor/capacity/mutation flushes; only
87 quads kept the same logical texture. Bind elimination skipped about 0.1
bind/frame against roughly 80.6 requested/frame, suggesting little opportunity
in that menu workload. These are native structural checks, not handheld
performance measurements. The diagnostic regression also checks timing output,
all modes, memory bounds and unchanged pixels under cache eviction.

A second native check measured city/traffic frames 1201–1400 (200 frames).
Original, white-batches and no-rebind modes had identical flush counts and
averaged 896.5 draws and 2,136.2 triangles/frame; diagnostic mode retained the
same triangles at 3.5 draws/frame. All final simulation states matched. The
fragmented modes counted 31,889 texture-triggered flushes before a quad was
clipped away (about 159/frame). These are candidate boundaries to defer, not
guaranteed saved draw calls: merging also requires compatible following visible
geometry. No-rebind skipped 18.6 binds/frame in that scene, about 2% of the
original bind requests. These native figures establish useful controls and
identify opportunities; they do not establish the cause of the handheld stall.

Smallest safe next steps depend on the device results:

- The smallest candidate supported by the native structural counters is to
  defer texture/scissor batch changes until after the existing screen clip
  accepts a polygon. The current UV math and draw order can be preserved;
  texture-mutation flushes must still happen before overwriting referenced data.
  This would avoid premature breaks for invisible quads, but not necessarily
  reduce every counted draw. Compare pixels and draw counts before enabling it.
- If no-rebind is materially faster with identical output, retain the shared
  bind tracker; it does not reduce draw count and may have little effect.
- If white-batches remains slow and `vertex_upload` dominates, first test
  nonoverlapping VBO write offsets within a frame (and bounded buffer renewal)
  while preserving the exact batches/order. This is smaller than a texture
  architecture change; the implementation and A/B procedure below now cover it.
- If white-batches is slow while VBO updates are cheap and draw submission
  dominates, reducing batches is indicated. The next focused test would be a
  small bounded atlas page using the same decoder, without reordering quads.
- If original is slow but white-batches is fast, investigate texture switching,
  sampling and memory traffic rather than attributing everything to draw count.
- If `empty_texture_flush` is substantial, moving visibility clipping ahead of
  lookup/batch selection is a small order-preserving candidate.

No diagnostic-build performance improvement is claimed on the RG40XX H.
The cross-compiled build still has no desktop libGL dependency. Device timing
results are needed to assess the VBO change below on the Mali driver.


## Contiguous VBO streaming and handheld A/B test

The measured Mali runs averaged ~103 ms/frame in vertex uploads for all three
fragmented modes, versus 0.072 ms for diagnostic mode. The old implementation
calls `glBufferSubData` at offset zero after every draw. Each write overlaps
vertices still potentially consumed by the GPU. These measurements and the
code indicate a synchronization hazard; proving the driver's internal stall
mechanism would require a GPU trace.

Default `--vbo-upload frame` retains the existing 60,000-vertex CPU arena and
records ordered texture/scissor commands. It uploads the contiguous prefix with
one GLES2 `glBufferData(..., data, GL_STREAM_DRAW)` and draws each batch using
its original count and an offset (`glDrawArrays` first vertex). Re-specifying
storage lets the driver retain old in-flight stores without an application
overwrite. Driver renaming/allocation performance is not guaranteed. No
`glFinish`, mapped buffers, fences or new texture architecture are introduced.
Depth, blend, projection and shader state stay constant during the frame.

A full vertex arena or 4,096-command queue drains in order. Texture cache
updates/deletions drain all queued draws referencing the affected texture
before its contents change, retaining a pending batch for a different texture.
Consequently cold-cache or eviction-heavy frames can have multiple uploads.
This preserves texture lifetimes and polygon ordering without pinning textures
or raising the texture budget. Only 114,688 bytes of fixed command storage are
added. Current application-owned GPU vertex storage is at most 2,400,000 bytes;
the driver may retain previous stores for in-flight work, which GLES2 cannot
measure. Allocation errors remain fatal and logged.

`--vbo-upload legacy` retains the original per-batch offset-zero update for
controlled comparison. All four render modes remain independent of this option.
`[PROFILE]` reports `vbo`, `avg_vertex_uploads`, `avg_vertex_bytes` (40 bytes per
submitted vertex) and total `texture_barriers`. Existing vertex-upload,
submission, draw-call, simulation, scene, cache/upload and swap timings remain.

Build with the commands above, then deploy from the host (replace `knulli` with
its SSH address; assumes the existing ROM deployment):

```sh
scp /tmp/rr-gles2-arm64/rr_gles2 root@knulli:/userdata/roms/ports/rave-gles2/rr_gles2
scp raverace/tools/RaveRacer-GLES2-Profile.sh root@knulli:/userdata/roms/ports/
ssh root@knulli 'chmod +x /userdata/roms/ports/rave-gles2/rr_gles2 /userdata/roms/ports/RaveRacer-GLES2-Profile.sh'
```

Launch the profile entry in Ports for a default 300-frame textured/frame run.
For reproducible A/B runs execute in SSH on the device while EmulationStation
has released the display (use the same display-control procedure as prior tests):

```sh
cd /userdata/roms/ports
for mode in diagnostic white-batches textured textured-no-rebind; do
    ./RaveRacer-GLES2-Profile.sh "$mode" --vbo-upload legacy
    ./RaveRacer-GLES2-Profile.sh "$mode" --vbo-upload frame
done
```

Each run uses identical input, 300 frames, unpaced, profiling frames 100–300.
Logs append to `/userdata/roms/ports/rave-gles2/<mode>-profile.log`; compare
`[PROFILE] total` lines marked `vbo=legacy` and `vbo=frame`. Compare JSON state,
triangles, draws, bytes, cache activity and overall timings, including swap:
waiting can migrate between driver calls. Repeat runs to control thermal and
cache variability. Optional `--capture-frame 300 --capture /userdata/roms/ports/rave-gles2/vbo-frame.ppm`
adds a screenshot (use a separate legacy filename). Screenshot cost is reported
separately. No handheld speedup is claimed until these runs are measured.

Native validation uses Mesa offscreen GLES 2.0. Before/after captures at frame
600 (menu) and frame 1400 (city/traffic, with `--steer 700:2048`) are compared
byte for byte in all four modes, together with full final simulation JSON.
The real-ROM regression additionally compares legacy/frame paths with a 4 MiB
cache to exercise eviction barriers. Headless regressions and desktop/ARM64
builds are checked independently. ARM64 ELF uses the Buildroot compiler,
AArch64 interpreter and GLESv2/EGL/Mali/SDL2 libraries, with no desktop libGL.
Hardware performance, driver storage renaming and in-flight memory overhead
remain unverified on the RG40XX H.
