# Tick-2500 road resource reproduction

See [the resolution report](../../SHADER_EQUIVALENCE_RESOLUTION.md).
This case contains polygons 978 and 1351: four triangles using one baked texture.
The two resource sets reproduce all 91 original differing scene pixel values.
Their isolated render differs at 102 pixels; later scene polygons cover 11.
For each fixed resource set the three shader variants are RGBA-identical on Mali-G31.

Files:

- `primitive_probe.c`: unchanged renderer shader sources and GLES2 scene state;
  compares single, conditional and straight-line samplers, all using texture unit 0.
- `vertices-high.h`, `vertices-low.h`: exact hexadecimal float inputs captured
  from the higher and lower bake runs.
- `texture-high.rgba`: FBO readback of the actual 594×80 baked texture.
- `texture-low.rgba`: equivalent step-2 298×80 bake reconstructed from the higher
  bake. Final guard column 297 is unused by the case; largest selected column is 293.
- `packed_probe.c`: companion UV/UVQ float32 bit diagnostic. Mode 0 checks ordinary
  colour output; modes 1–5 encode U, V, UVQ.x, UVQ.y and UVQ.z as little-endian
  float32 bits across RGBA bytes. Read those files as little-endian float32, with
  bottom-left framebuffer row order. All values used here are nonnegative.
- `sample_coordinates.json`: per-pixel coordinates, sampled texels and texture
  bytes for all 91 original coordinates. Screen coordinates use a top-left origin.

The texture file starts with one ASCII line `alloc_w alloc_h used_w used_h`, then
raw RGBA8 bytes in GL texture row order. Output `.rgba` files contain headerless
640×480 RGBA8, bottom-left framebuffer row order.

Build from this directory, choosing the matched Knulli toolchain:

```sh
case_cc=/home/bwebb/dev/knulli-scarab/output/h700/host/bin/aarch64-buildroot-linux-gnu-gcc
case_inc=/home/bwebb/dev/knulli-scarab/output/h700/host/aarch64-buildroot-linux-gnu/sysroot/usr/include/SDL2
"$case_cc" -O2 -I"$case_inc" primitive_probe.c -o /tmp/primitive-high -lSDL2 -lGLESv2 -lmali
"$case_cc" -O2 -I"$case_inc" '-DCASE_VERTICES_HEADER="vertices-low.h"' primitive_probe.c -o /tmp/primitive-low -lSDL2 -lGLESv2 -lmali
```

For packed coordinates, replace `primitive_probe.c` with `packed_probe.c` in
both commands. Use a separate output directory for each run. The first runtime
argument selects the texture file, e.g. `primitive-high /path/texture-high.rgba`.
On the handheld use `SDL_VIDEODRIVER=mali XDG_RUNTIME_DIR=/var/run` and restore
EmulationStation after diagnostics. These are temporary diagnostic executables;
do not replace the installed playable or its backup.

Detailed full-scene, isolated, packed-coordinate and historical capture outputs
remain under `/tmp/rr-primitive-results/`, with device counterparts under
`/tmp/rr-frozen-scene/`, `/tmp/rr-primitive-road/` and `/tmp/rr-tier-probe/`.
