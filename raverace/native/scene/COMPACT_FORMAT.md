# Revised Milestone 3 compact scene protocol (v3)

This is isolated offline playback, not the live scene bridge. The source v2
capture and protocol remain documented in [FORMAT.md](FORMAT.md). Build with
`native/tools/compact_race.py V2_PACK SOURCE_CAPTURE OUTPUT`, then validate with
`native/tests/validate_compact_race.py V2_PACK V3_PACK REPORT_JSON` (see each
command's `--help` for named options). Generated packs stay ignored.

All wire integers/floats are little endian. `frames.rsc` starts with four u32:
magic 0x33525352, version 3, count 3840, codec 1 (zlib); 3841 u64 offsets include
the end sentinel. Each independent compressed frame is at most 512 KiB; inflate
is bounded to 512 KiB, with checksum, EOF and trailing-data checks. The manifest
binds every static file and compressed/raw packet hash to the v2 source manifest
and original source-capture hashes. Unknown versions or malformed lengths fail.

The frame header is `12I3f` (60 bytes): magic 0x33504e52, version, original tick,
source quad count, visible count, group count, instance count, dynamic texel count,
font revision, HUD style revision, palette revision, reserved zero, background RGB.
The payload, in order, contains:

- Instances: 80 bytes each, original 64-byte matrix/camera plus four i32 scissors.
  Instances are shared only within the picture and preserve source identities.
- All original quads: 32 bytes each, `<iHHf4H4HI`: mesh texel, instance, material,
  depth, four exact brightness values, source instance, emission order, flags,
  reserved zero, original zsort. Direct mesh texels use `-1 - direct_index*8`.
- Visible u16 permutation, padded to four bytes; no source record is deleted.
- Groups: `<4I4i` (32 bytes): first visible index, count, material page, mode,
  original scissor. Mode bit 0 is near clipping, bit 1 protected ordering.
- Dynamic RGBA32F geometry, followed by the exact 8192-byte HUD text RAM.

Quad flags: bit 0 HUD priority, bit 1 near clipping, bit 2 protected ordering;
the high byte records cull reason: 0 visible, 1 left, 2 right, 3 top, 4 bottom,
5 empty scissor. Conservative whole-quad rejection encloses float projection and
original integer Q15 transform error, requires positive near bounds, and uses a
four-pixel margin outside the original scissor. Near crossings are not culled.
The independent validator reconstructs immutable source positions and verifies
these bounds with a two-pixel margin, every source ID/order, depth, material,
brightness, transform, direct vertex and HUD byte. Protected quads never merge.

`materials.bin` stores `<4f4I` per material: UV affine, page, three reserved zero.
The GPU retains only immutable UV affine data. Font revisions are 122880 bytes;
HUD style revisions are 2073 bytes: 16-byte original header, 1024-byte palette,
9-byte mix state, 1024-byte LUT. Revision lookup reconstructs the exact v2 HUD.
This actual window has one font and 166 styles; no synthetic palette event is
invented. Geometry/material pages and mesh files are unchanged v2 assets.

The renderer uses a three-slot FIFO, each 512 KiB, plus one bounded compressed
scratch buffer. Slot ownership is FREE → WRITING → READY → READING → FREE under
mutex/condition variables. Only the consumer calls GL. Slots are released after
whole-picture completion and any diagnostic capture; no overwriting, dropping
or extrapolation is allowed. The producer reuses one inflater and reports read,
inflate and thread CPU independently. First-three-packet prefill and static load
are outside picture timing and explicitly logged; readiness waits are inside.

Ordinary quads draw six instanced vertices; near-clipped quads use the separate
12-vertex shader. Per-instance matrix/camera uploads and per-quad attributes
replace full command textures. World VAO divisors are isolated from the unchanged
production GPU HUD. Static GPU bytes are 482570240 (460.21 MiB), below the stated
512 MiB full-lap ceiling; CPU shadows, loading and process RSS are separate.

Normal timing uses one final `glFinish` after world and HUD. `ENG_NATIVE_STAGE_SYNC`
adds a diagnostic intermediate sync and is excluded from headline results.
Readback runs are also excluded. One must match fence policy for v2 controls.
Main-thread CPU excludes the separately reported producer CPU. Submit spans are
not exclusive GPU completion times. This protocol proves neither live gameplay
throughput nor display cadence; both remain later milestones.
