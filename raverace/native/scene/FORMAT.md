# Native race scene API and offline playback

This is Milestone 3 only. Simulation and audio are not linked into playback.
Source is MIT under the repository licence. All ROM-derived packets, pages,
meshes and imagery belong in ignored `native/generated/`, never Git.

## Source command capture v1

`build_race_capture.py` privately relinks the unchanged headless executable,
inserting capture immediately after the original `rr_scene_frame()` eligibility
call and before the original direct-FIFO consume. It invokes the original
geometry/list stage only as a diagnostic source oracle. No capture code enters
production or benchmark binaries. Use `ENG_NATIVE_CAPTURE` because deterministic
replay intentionally removes `RR_*` debug environment variables.

Each `tick-T.bin` begins with eight little-endian uint32 fields: magic
`0x33435352`, version 1, tick, walk, mesh-quad count, direct count, record stride352,
state byte count. Counts are bounded at 32768/4096. Every mesh record is88 int32:
36 metadata, 24 original local source components, 28 original projected oracle
components. Metadata: code, original UV-packet address, zsort, colour, flags,
bank, cmode, objectflags, cz-adjust, cz-type, cz-value, emission order; Q15
matrix9; translation3; zoom mantissa/shift; camera centre2; scissor l/r/t/b;
near-clipped count, guard-clipped count, behind flag, display-list record address.
Local corners are x/y/z/u/v/resolved-brightness. Oracle corners are
sx16/sy16/viewZ/u/v/brightness/valid. Original UVs and brightness are preserved
before the old oracle leaves behind-eye corners unset. Direct records retain
28 uint16 words each. State then holds planar palette 0x18000, mixer 0x8000,
czram0x8000, CG0x1e000, text 0x2000, attributes 16. Every exact tick is mandatory.

Instance IDs are **frame-local display-list record addresses**, not invented
persistent traffic IDs. They bind to the actual per-picture code/transform;
reusing a list slot does not imply the same traffic entity across pictures.
Changing model/packet identity encodes the original animation selections.

## Compiled playback v2

The manifest format is `rr-native-race`, version 2. All immutable files and the
frame index are sized/SHA256 bound; the index binds every compiled packet and
its source packet hash. ROM hashes and palette identities are recorded. Static
meshes are deduplicated by original model/packet ID, and their local x/y/z/UVs
are checked against the ROM. Material keys bind original recipe + actual palette
group hash, then deduplicate complete RGBA content. Fresh source-derived UVs,
two source-aware gutter texels, nearest/clamp/no-mips follow M2's contract.
Static mesh texels are two RGBA32F per corner: xyz0, uv00, four original corners
per packet. These are geometry attributes, not fragment map/pen/palette lookup.

`race.meta`: ASCII `RRRACE2 pages meshWidth meshHeight maxCommands`, then page
width/height lines. `frames.rsc`: uint32 magic0x33525352, version 2, count 3840,
reserved0, then3841 uint64 offsets, followed by the original compiled packets.
Offsets delimit bounded complete packets. This sequential container avoids
per-picture filesystem opens; it does not preload all commands into RAM.

A compiled packet has uint32 magic0x33504e52, version 2, tick, primitive count,
group count, dynamic-mesh texel count, HUD byte count, palette revision; then
float32 background RGB. Header44 bytes. Followed by one144-byte command per
primitive, one32-byte group per draw, dynamic RGBA32F texels and original HUD state.
All fields are little endian/IEEE float32. Nine vec4 command columns:

| Column | Meaning |
|---|---|
| 0–2 | Q15 matrix columns xyz + respective translation in w |
| 3 | Camera centre in NDC xy, projection scale xy |
| 4 | Native UV scale xy, offset xy |
| 5 | Four resolved brightness values /64 |
| 6 | Mesh texel index, synthetic depth, HUD priority, near-crossing flag |
| 7 | Original fog RGB, fog weight |
| 8 | Frame-local instance ID, original emission order, model code, packet ID |

A negative mesh index means original direct geometry in the bounded dynamic
mesh texture (`-index-1`). Original direct emission order remains ahead of list
emission before sorting. Synthetic depth binds original descending zsort/tie
order. Groups are int32 first,count,page,scissor x/y/w/h,protected. Reordering
only coalesces opaque compatible groups; zero-area source boundaries remain
individual draws. Near clipping happens in the native vertex shader; immutable
local meshes are not reconstructed or uploaded per picture. Fog state is retained but polygon fog rendering is disabled to match production
GLES2 policy. Per-corner
lighting, cameras, animated model selection, direct records and HUD are dynamic.
GPU Q15 dot products use float32/floor, so subpixel sampling/projection differs
from the integer oracle while original command state is exact.

HUD bytes use the existing production captured-HUD contract (HUD1): header16,
CG/text 0x20000, RGBA palette 1024, multipliers9, LUT1024. The production GPU glyph
compositor reuses glyph resources and only uploads changed data. Live point-RAM
code5 and palette-solid cases are explicitly rejected by this compiler; none
occur in the selected 3840 ticks. This is a declared coverage limit, not fallback
or silent omission. Palette revisions are content identities and baked before
playback; changing palettes create new materials subject to residency limits.
The actual recording uses one palette revision. Unseen/uncompiled revisions
must not fall back to another palette. Runtime streaming is not implemented.

## Bounds and tests

For this selected race, static GPU resources are482,471,936 bytes (460.12 MiB),
below an explicit512 MiB preload ceiling. This is the full lap; M2's128 MiB target
was a frozen-scene target and is not relabelled as passing for the full lap.
Native commands have4096 primitive capacity, dynamic geometry4096 texels, and
one fixed packet buffer. Actual maxima are2157 primitives,454453 packet bytes,
311760 dynamic world-upload bytes; buffer storage is bounded independently of
lap duration. No per-tick static mesh/material allocations/uploads/ROM decode
occur. First-run file reads and shader warm-up are reported separately. Palette
variants for other recordings can exceed the cap and must stop with a new budget.

Reproduce from repository root (all capture/build output directories new):

```sh
cmake -S raverace -B /tmp/m3-headless -DRR_HEADLESS_ONLY=ON -DRR_FAST_MEMORY_READS=ON -DRR_SPLIT_MEMORY_READER=ON -DRR_C352_INACTIVE_FASTPATH=ON -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/m3-headless -j4
python3 raverace/native/tools/build_race_capture.py /tmp/m3-headless /tmp/m3-capture-build
ENG_NATIVE_CAPTURE="$CAPTURE" /tmp/m3-capture-build/race_capture raverace/extracted --replay-input "$REPLAY" --frames 5639 --input-checkpoints "$CHECKPOINTS"
python3 raverace/native/tools/audit_race_capture.py "$CAPTURE" "$AUDIT"
python3 raverace/native/tools/compile_race.py raverace/extracted "$CAPTURE" "$AUDIT" "$PACK"
python3 raverace/native/tools/pack_race_stream.py "$PACK"
python3 raverace/native/tests/validate_race.py "$CAPTURE" "$PACK" "$VALIDATION_JSON"
python3 raverace/native/tests/validate_race_materials.py raverace/extracted "$PACK" "$AUDIT" "$MATERIAL_JSON"
cc -O2 -Wall -Wextra -DRR_GPU_HUD -Iraverace/include -Iengine raverace/native/render/race.c -o /tmp/m3-race $(pkg-config --cflags --libs sdl2 glesv2) -lm
SDL_VIDEODRIVER=offscreen /tmp/m3-race "$PACK" raverace/native/render 1800 5639 1 - > "$RUN_LOG"
```

Actual durable artifacts: `raverace/native/generated/milestone3/`. Read the race
report for exact accepted hashes, device paths, benchmark gate and result.
Reference sequences use a private `build_race_reference.py` relink of the original
GLES build, `--snapshot-reference`, `ENG_GPU_HUD_VERIFY=1` and
`ENG_GPU_HUD_CAPTURE`. Captures and gameplay checkpoints must agree with the
untouched headless/original replay. Native capture sets `ENG_NATIVE_SEQUENCE=1`
and an output directory in place of `-`; these runs are diagnostics, not timing.
Normal timing uses `-`, five startup warm-ups and every required tick, no transfers,
no capture and no display swap/vsync. Completion includes packet reads, dynamic
uploads, world and HUD through glFinish; it is not exclusive GPU time or live FPS.

Cross build with the repository's matching Buildroot toolchain, `-lSDL2 -lmali
-lm` instead of pkg-config libraries. Transfer only diagnostic files to the named
isolated directory; use tar `--no-same-owner` on userdata's filesystem. The
`upgrade_race_pack.c` utility is a transfer adapter from the provisional eight-
column pack plus captured identity metadata to final v2; output must match the
host container SHA256 exactly. Normal fresh compilation needs no adapter.
`run_race_h700.sh BASE NEW_RUN_DIR` checks the production hash, refuses concurrent
game execution, stops/restores EmulationStation and leaves all results isolated.
