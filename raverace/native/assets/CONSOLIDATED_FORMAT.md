# Native consolidated frozen assets v1

This is the revised M2 format. Original M1 packs remain unchanged. Generated
copyrighted outputs must stay outside Git. Python 3/NumPy are required.

`rr-native-material-pages`, version 1: canonical JSON manifest binds every file
by size/SHA256 and binds source ROM hashes, M1 manifest, instance snapshot,
original direct records and RGB palette. `materials.json` preserves each original
(bank, palette group, cmode, u, v, width, height) recipe, source packet binding,
content hash, placement and native UV affine. Identical complete RGBA rectangles
with identical dimensions can share content. Pen zero is opaque in this format.
Only the measured opaque scene contract is supported.

RGBA8 pages are raw row-major bytes with dimensions in the manifest's material
data. Original UVs are retained. The fresh native mapping is
`u/pageWidth + (x+gutter+0.5-uMin)/pageWidth` (likewise v). The two-texel gutters
sample **original source coordinates**, wrapping modulo 4096 before original
tile addressing, flips/transpose and cmode/palette decode. They are not clamped
copies of the rectangle edge. Texture sampling is nearest/clamp, with no mips.
Mip bytes are budgeted only. Future mips require independently filtered material
levels and regenerated gutters; automatic whole-page mip filtering is forbidden.

`rr-native-local-scene`, version 1: manifest checksums bind all binary files,
source material/M1 manifests and original model identities. Little endian:

| File | Record |
|---|---|
| `models.bin` | 6 signed int32: original x,y,z,u,v,brightness (24 bytes) |
| `instances.bin` | 12 signed int32: Q15 matrix9, translation3; float32 camera4 (64 bytes) |
| `quads.bin` | uint32 vertex,instance,page,flags; int32 scissor4; float32 native UV affine4, source-control affine4, depth,alpha; uint32 code,packet,material (84 bytes) |
| `textures.bin` | uint32 page,x,y,width,height (20 bytes), diagnostic source-texture rectangles |
| `source-models.json` | Original source model/packet/instance bindings |
| `scene.meta` | ASCII `RRNATIVE1 pages vertices instances quads materials bgR bgG bgB`, followed by page width/height lines |

Quad flags: bit0 original direct geometry, bit1 protected degenerate boundary.
Direct source words are decoded from uint32 count + count×28 uint16 snapshot
records. Model local-space records remain reusable; direct corners retain their
original screen/depth/UV/brightness data. A bounded load-time step applies source
Q15 transforms/translation, near clipping and indexed grouping; GPU camera
projection remains native. Synthetic priority and tie order are retained, as are
mirror cameras/scissors. Unsupported objectflags/point-RAM code5 are rejected;
neither occurs in the two accepted frozen scenes. This is not a race command
protocol or live palette update format.

## Reproduction

From repository root, output directories must be new. `ROMS`, `PACK`, `INST`,
`CAPTURE`, `PAL` and `OUT` below denote actual paths. Original M1 reproduction
is in [FORMAT.md](FORMAT.md). Capture one selected scene using a private existing
host build, without modifying production sources or its normal build objects:

```sh
python3 raverace/native/tools/capture_native_snapshot.py /tmp/rr-playable-native-build /tmp/new-capture --tick 3600 --capture-world
```

Run its emitted private executable with the original recorded replay and the
matching `--frames 3600`/capture-frame arguments, as documented by the existing
playable/replay instructions. The helper prints the private output path. Compare
its endpoint state JSON to the original headless replay before conversion. For
tick2500 the original `/tmp/rr-atlas/frozen` reference is retained. Resolve RGB
from the actual captured palette planes, not a default ROM palette:

```python
import numpy as np
from pathlib import Path
p = Path('/tmp/new-capture')
(p/'palette.rgb').write_bytes(np.frombuffer((p/'palette.planar').read_bytes(), np.uint8).reshape(3,32768).T.copy().tobytes())
```

```sh
python3 raverace/native/tools/export_frozen_instances.py "$ROMS" "$CAPTURE" "$PACK" "$INST" --scene "$CAPTURE"
python3 raverace/native/assets/consolidate_materials.py "$ROMS" "$PACK" "$INST" "$CAPTURE" "$PAL" "$OUT/materials" --direct "$CAPTURE/direct.bin"
python3 raverace/native/tests/validate_consolidated.py "$ROMS" "$OUT/materials"
python3 raverace/native/assets/build_native_scene.py "$PACK" "$INST" "$OUT/materials" "$OUT/scene" --reference "$CAPTURE"
python3 raverace/native/tests/validate_native_scene.py "$OUT/scene" "$PACK" "$OUT/materials"
```

Actual accepted artifacts: `/tmp/rr-m2rev/materials-canonical`, `scene-canonical`,
`materials3600`, `scene3600`; original instances in `instances`, `instances3600`.
The numeric manifests/hashes and benchmark evidence are in
[benchmarks](../../benchmarks/native-frozen-revised-20261015/).
See [renderer commands](../render/README.md) and
[acceptance report](../../NATIVE_FROZEN_SCENE_REPORT.md).
