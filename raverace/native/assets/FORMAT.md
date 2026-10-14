# rr-native-assets v1

Offline asset format; no simulation, renderer or GPU API dependency. Run with
Python 3 and NumPy. Repository code remains under the root MIT licence. ROMs,
palette snapshots and outputs retain their owners' rights and must not be shared.

`manifest.json` is canonical JSON (sorted keys, compact separators, UTF-8,
newline). `format` must equal `rr-native-assets`; `version` must equal 1.
`files` lists every payload's byte length and SHA256. `sources` identifies each
exact-sized input ROM by basename, size and SHA256. `palette_sha256` identifies
the optional **actual snapshot** RGB palette. No timestamps or absolute paths
enter the format. The manifest itself is identified by an externally recorded
SHA256; this is integrity checking, not cryptographic authenticity.

`model-CODE.json` stores the display-list object code, point-ROM list address,
original quad/lighting packet addresses, raw 24-bit format/flags/colour/polyshift,
source UV words and material association. Preserve ordered quads: sorting and
synthetic priority are per-instance/per-picture concerns. A repeated list/chunk
reference is not implicitly deduplicated. Four local vertices per quad keep UV
and lighting seams. Indices use `(0,1,2),(0,2,3)` in original corner order;
degenerate polygons are retained, not repaired or silently discarded.

`model-CODE.vertices`: tightly packed little-endian signed int32, six components
per vertex, stride 24 bytes: `x,y,z,u,v,original_gouraud_brightness`. XYZ retain
signed 24-bit ROM units without scaling, axis conversion or instance transform.
Y increases upward; the current geometry stage projects X right, Y upward and
positive camera Z forward. Instance row-vector Q15 matrix and translation are
not baked. The raw brightness byte uses **UV word i** for vertex i, following
`quad_fixed`, not UV word 2*i. For flat packets use colour bits 16..23; normals
packets override brightness with instance-dependent lighting. Those raw packets
and their ordering/reset markers are retained in JSON for later evaluation;
this milestone has no native lighting evaluator.

`model-CODE.indices`: tightly packed little-endian uint32. Each quad JSON record
stores `first_vertex`. `lighting_packet_count` identifies the number of lighting
packets encountered before the quad. 0x10 resets the original lighting list;
0x0d appends. Raw normal components require low-16-bit sign extension. No normal
packets occurred in this ROM's inventoried models, but parser support is present.

`materials.json` stores sorted unique `[bank,palette_group,cmode]` keys, all source
rectangle recipes `[bank,group,cmode,min_u,min_v,width,height]`, and the ordered
`baked` subset. Recipes use full inclusive UV bounds; original 12-bit source UVs
remain in vertex buffers. These recipes describe textured instance mode only:
objectflags/cz_adjust can select palette-solid materials at runtime.

`material-N.pens`: width*height uint8 decoded **final** pen indices, row-major.
`material-N.rgba`: width*height*4 RGBA8, row-major, RGB from the supplied snapshot
palette, A=255 (Rave's opaque world profile). N indexes `baked`. No placeholder
palette is invented: without `--palette` only pen assets are emitted.
`palette.rgb`: 128 groups * 256 entries * RGB8, contiguous. The texture decoder
uses little-endian tile words, high nibble for even map entries, extended tile
bit, flips before transpose, zero raw pen for tile addresses beyond CG ROM, and
original packed 2/4/8-bit cmodes. Palettes/gamma/lighting/fog are not folded into
ROM provenance; the supplied palette already represents resolved RGB state.

The default object table interval is `[0x45,2490)`, matching the observed first
list pointer/table boundary of this Rave ROM set. It is explicit in the manifest
and overridable; it is not a general discovery algorithm for other games.
The converter checks chip sizes, addresses, list termination, chunk/packet bounds,
known packet types, topology and UV/material associations. Bad object recipes
are enumerated as exclusions rather than silently exported. Review `excluded`
before using a pack; the current set has zero. Existing output directories are
rejected. Repository outputs are restricted to ignored `native/generated/`;
prefer `/tmp`. Bake subsets are limited to 256 materials. Asset buffers are raw
CPU/load-time buffers, not a persistent GPU-resource implementation.

Reproduce from repository root (each output directory must be new):

```sh
python3 raverace/native/assets/asset_tool.py convert raverace/extracted /tmp/rr-m1-new --palette /tmp/rr-native-texture/palette.rgb --bake-models 1280,1765
python3 raverace/native/assets/asset_tool.py inspect /tmp/rr-m1-new
python3 raverace/native/tests/validate_assets.py raverace/extracted /tmp/rr-m1-new /tmp/rr-atlas/frozen /tmp/rr-native-texture/palette.rgb
```

The last command requires preserved actual renderer capture inputs; it checks
670 RGBA resources including original guard texels and decimation strides.
No capture utility or scene renderer is introduced by this milestone.
