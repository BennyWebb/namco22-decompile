# Tick-2500 shader-equivalence resolution (2026-10-13)

The original 91-pixel difference is reproduced exactly by changing the baked
texture tier and its submitted UV scale, with the **same single-sampler shader**.
The two captures did not have identical draw resources. The previous attribution
to the fragment shader is superseded by this investigation.

On Mali-G31, the single-sampler, conditional four-sampler and straight-line
four-sampler shaders are RGBA-identical for the captured primitives and for the
complete tick-2500 scene when commands, vertices and texture objects are frozen.
This establishes equivalence for this scene, not for batching or every scene.
Batching remains disabled. No production shader or cache changes were made.

## Specific primitives and resources

Coordinates below use the top-left origin of the existing PPM captures. The
original difference mask contains 91 pixels, x=299–526, y=218–254. Accounting for
triangle coverage, scissor and sorted depth identifies:

| Sorted polygon | Pick code | Original emission order | Changed pixels | Triangles |
|---|---:|---:|---:|---:|
| 978 | 1282 | 1091 | 16 | 2 |
| 1351 | 1281 | 938 | 75 | 2 |

Both use texture bank 12, palette group 103, cmode 0, UV rectangle
`min_u=1871, min_v=576, range_u=593, range_v=79`. Their colour input is
`(51/64,51/64,51/64,0)`. Their scissor is `(0,0,640,480)`.
The scene uses RGBA8 with depth16, `GL_LEQUAL`, depth writes enabled,
blending and culling disabled, priority enabled, a 640×480 viewport,
`GL_NEAREST` min/mag filters and `GL_CLAMP_TO_EDGE` wrapping.
The screen-to-clip matrix and triangle order are retained exactly.

The cache gives these polygons one of two resource sets:

| Resource | Higher tier | Lower tier |
|---|---:|---:|
| Bake cap | 1024 | 512 |
| Source-column step | 1 | 2 |
| Allocated texture | 594×80 | 298×80 |
| Used texture | 593×79 | 297×79 |
| Submitted U scale, exact float | `0x1.ff2358p-1` | `0x1.fd6c3ep-1` |
| Submitted V scale | `0x1.f9999ap-1` | `0x1.f9999ap-1` |

Consequently the submitted `uvq.x` values differ. Position, colour, `uvq.y`
and `uvq.z` are identical. These are different inputs before the shader runs.
The GL texture object number varies with allocation history and is not a content
identifier. In the higher-tier resource capture both polygons used object 2126.

Exact submitted vertices for both resource sets, shader source, textures and
representative pixel coordinates are preserved in
[the standalone case](tools/shader_equivalence_case/README.md).
The original fixed-point source vertices and draw metadata are in
`/tmp/rr-primitive-results/rr-primitive-road/shader.log`.

## Why resource history matters

`engine/tex_bake.c`, `bake_quad_texture()`, deliberately retains a sharper
cached tier when the current projected size asks for a smaller tier. Its lookup
probes cap 1024 before returning a requested cap 512. `bake_impl()` includes the
cap in the cache key and computes both source step and UV scale from that cap.
If the sharper entry is absent, the lower tier is baked instead.

The independent replay drops presentations, and cache residency depends on the
frames that were rendered. A fixed simulation tick and an identical geometry
digest therefore do not guarantee identical baked textures or submitted UVs.
This explains why profiling and timing changes could appear to fix the shader.
The exact earlier cache eviction tick was not traced; it is unnecessary to the
identified pixel mechanism and was not inferred from a speculative GPU bug.

A new **off-mode** run with the lower U scale reproduced the original candidate
PPM byte for byte. Another off-mode run with the higher U scale reproduced the
original baseline PPM byte for byte. Both used the original single-sampler
fragment shader. Thus the nominal shader A/B can also occur with no shader
change.

## Controlled reproductions

1. Captured all submitted tick-2500 triangle inputs and polygon scissors.
   An initial coverage calculation incorrectly selected mirror polygon 1634;
   its scissor excludes every affected pixel. That diagnostic was discarded
   as an attribution and preserved with the other captures.
2. Captured the actual higher-tier texture through an FBO readback: 594×80,
   used 593×79. The two road polygons reference the same texture bytes.
3. Rendered each polygon separately with this resource. All three shader forms
   produced identical RGBA output. Repeated the comparison for the combined
   four-triangle case using both captured UV input sets and the corresponding
   higher/lower texture layouts: again zero shader differences.
4. Reconstructed the lower-tier image by taking every second source column of
   the higher-tier capture, as the decoder's step-2 bake does. The unused final
   guard column is duplicated for storage completeness. Diagnostics show the
   largest sampled lower-tier column is 293; guard column 297 is never sampled.
   This reconstruction introduces no approximation in any sampled texel.
5. Changing only the resource set produces 102 differing pixels in the isolated
   four-triangle render. At the original 91 coordinates, **both endpoints are
   exactly the original pre-compositor RGBA values**. The other 11 pixels are
   covered by later polygons in the complete scene. Applying the captured
   coverage/scissor/order to those 102 pixels reconstructs the entire original
   candidate scene with **zero differing bytes** relative to `scene4_shader.rgba`.
6. Froze the complete original draw stream and replayed it without texture
   lookups, rebaking, simulation or batching: **1,280 draws, 7,998 vertices**.
   Original-program replay is byte-identical to the original scene, validating
   the replay's state and resource fidelity. Conditional and straight-line
   four-sampler replay are also byte-identical, all sampler uniforms using unit 0.

The frozen off-mode scene itself matches the original lower-tier candidate
`scene4_shader.rgba`; shader changes still produce zero differences when those
resources are retained. The five full-scene RGBA files in
`/tmp/rr-primitive-results/rr-frozen-scene/` share SHA-256:

```
0da45be3e49bcf22afaf6f404ef32efd7ba3af68bab1fb645e052d7d7254872c
```

The original baseline/candidate captures under
`/tmp/multi4-ab-20261013-182049-28305/` and
`/tmp/shader-split-20261013/` were preserved.

## Interpolation and texel selection

A targeted companion shader packs positive float32 UV and UVQ values into
RGBA8, preserving their bits. Its normal-colour mode is byte-identical to the
uninstrumented primitive output. For each resource set, the single, conditional
and straight-line forms produce byte-identical packed values across the entire
primitive coverage, for U, V and all three UVQ components. Instrumentation can
change compiler allocation; the normal-colour gate and independent unchanged
shader tests bound this diagnostic's interpretation.

An earlier RGBA32F output diagnostic rounded through the GLES2 fragment output
precision. Its numbers are **not** used as exact coordinate evidence.

Representative GPU readbacks after perspective division:

| Pixel | Higher U | Lower U | V in both | Higher sampled texel | Lower sampled texel |
|---|---|---|---|---|---|
| (398,234) | `0x1.5c38b0p-2` | `0x1.5b0d8ap-2` | `0x1.10f50cp-1` | (201,42) | (100,42) |
| (463,228) | `0x1.77cea4p-1` | `0x1.768bccp-1` | `0x1.877b16p-2` | (435,30) | (217,30) |
| (349,242) | `0x1.b7b7bep-8` | `0x1.b63ec0p-8` | `0x1.08b0c6p-4` | (3,5) | (1,5) |

At (398,234), these texels correspond to source coordinates (2072,618) and
(2071,618), with RGBA `(56,35,4,255)` and `(55,35,4,255)`. Multiplication by
51/64 yields scene RGBA `(45,28,3,0)` and `(44,28,3,0)`, exactly the original
pair at that pixel. The full sample table includes float bits, UVQ, selected
texels and source coordinates for all 91 original pixels.

The mismatch originates in **different baked sample spacing and submitted UV
scale**, leading to different nearest-selected source texels. Interpolation and
perspective division are equivalent between the tested shader forms for fixed
inputs. Rasterisation, alpha, colour multiplication and draw ordering do not
need changes to reproduce or explain this mismatch. There is no captured
reason to investigate Mali machine code further for this case.

## Decision and scope

Correct the validation method by freezing draw resources, or by checking baked
texture versions/dimensions/content and submitted vertices before treating a
replay-frame comparison as a shader-equivalence test. The shader-only full-scene
validation above passes that requirement.

Do not disable sticky tiers or force lower quality as a purported shader fix.
The existing tier policy intentionally avoids texture popping; changing it is a
separate renderer decision. No shader correction is required by this evidence.
Batching remains disabled pending its own fixed-resource equivalence tests,
including HUD, transparency and dynamic textures.

Temporary capture helpers are preserved as
`/tmp/rr-primitive-results/renderer-diagnostic.patch`; the prior temporary
renderer sources were restored and rebuilt after testing. EmulationStation was
restored, no diagnostic/game process remained, and the installed playable hash
was reverified as
`77536f56ad17de9cf8772175c73442d9cb246d87aeb6fbf6c8d7f07c3d6ab53e`.
No executable or launcher in the installed game or backup was modified.
No commit was created.
