# Texture pipeline investigation (before Phase 2 implementation)

The original CG ROMs load into `g_texture_data` (16 MiB), and CCR low/high
ROMs load into `g_texture_tilemap` (2.5 MiB), through `eng_load_texture_roms`.
Rave Racer also applies `eng_texture_tilemap_sys22_fixup`. These ROM-derived
pixels and tile addresses are immutable after startup. `rr_scene` supplies the
actual DSP geometry; texture bank, cmode, palette selector and UV bounds are
already decoded in the shared geometry pipeline.

`texture_pen_lookup` in `tex_bake.c` maps 12-bit U/V plus the V bank to a
16×16 tile. Little-endian tile addresses and high-first packed attributes
select the CG tile and apply X/Y flips followed by transpose. The bake's
existing cmode decoder supports packed 8-, 4- and 2-bit subpens, then indexes
`direct_palette[(color >> 8) & 127]`. The palette is reconstructed from game
RAM's planar RGB arrays each screen update. Per-group generation numbers
invalidate cached bakes on palette changes. Polygon pen zero is opaque on
System 22 (`g_tex_opaque=1`); legacy keyed paths preserve pen-zero alpha.
Object-flag solid polygons instead resolve a palette pen without a texture.

Desktop quads bake their original, unclipped UV bounding rectangle. Degenerate
ranges expand to 16 texels. The decoder supplies scales and origins for the
actual sampled size, decimation step and allocated storage. Vertices carry
`(origin + localUV * scale)/z` and `1/z`, interpolated through screen clipping
and divided per fragment. Rave Racer uses texel centres and brightness/64.
ROM address wrapping is implemented during decoding; baked textures use
nearest filtering, clamp-to-edge and guard texels, without mipmaps.

Desktop atlas mode preallocates roughly 128 MiB of GPU pages and equal CPU
shadows, can grow to roughly 448 MiB each, and uploads dirty rectangles using
desktop-only UNPACK_ROW_LENGTH. A page reset flushes buffered draws before
overwriting their texels. The existing alternative per-texture path already
uses GLES2-compatible RGBA/UNSIGNED_BYTE uploads and compact scratch decoding.
However its byte budget is enforced after upload and its recycled-object pool
retains uncounted GPU storage; those policies cannot bound handheld allocation.

Chosen smallest integration: reuse the entire existing per-texture baker and
cache, force atlas off only for GLES2, eliminate retained evicted storage,
enforce the budget before allocation, and flush pending geometry before any
texture mutation/deletion. This avoids a new decoder, atlas/shadow buffers and
new atlas-coordinate mapping. CPU memory comprises the immutable ROM buffers,
existing cache metadata and one shared ~4 MiB scratch buffer; no decoded-pixel
cache is retained. Draw-call count may be higher than atlas mode and is logged.
Palette changes rebake on demand. Geometry, simulation and desktop policies
remain intact. Device testing will determine whether the 64 MiB budget thrashes.
