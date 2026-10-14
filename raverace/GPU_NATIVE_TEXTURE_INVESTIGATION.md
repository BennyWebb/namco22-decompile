# GPU-native System 22 texture investigation — 2026-10-14

## Decision

**Retain production. Stop this prototype at the isolated shader performance gate.** Both tested GPU-native layouts reproduce the preserved Mali tick-2500 world framebuffer with zero differing RGBA pixels. With all textures already resident, the original draw boundaries and unchanged geometry, their median submission-through-completion times are 81.00 ms and 77.94 ms, versus matched RGBA medians of 37.53 ms and 37.37 ms. Pre-orientation helps only modestly. These measurements do not justify live integration or speculative batching.

This rejects the measured three-dependent-lookup shader, not every conceivable GPU representation. It does not establish exclusive GPU shader execution time, nor extrapolate an integrated FPS. No production source, executable or launcher was modified. No commit was made. EmulationStation was restored (PID 6271); the production executable SHA256 remained `6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`.

## Context and scope

Reviewed `RENDERING_ARCHITECTURE_REVIEW.md`, `GPU_WORLD_ATLAS_INVESTIGATION.md`, `MALI_TEXTURE_ARRAY_DRIVER_INVESTIGATION.md`, `TEXTURE_PREDECODE_INVESTIGATION.md`, `GPU_HUD_PRODUCTION.md`, the decoder in **`engine/tex_bake.c`** (rather than `raverace/engine/tex_bake.c`), tile-run/predecode helpers and frozen-scene GLES infrastructure. Starting source HEAD: `1c40c2f`. The rejected RGBA array retention/cohort work was not repeated.

The standalone probe replaces world sampling only. It preserves captured vertex bytes, draw order, draw boundaries, scissor rectangles, synthetic depths, depth format, blending/priority semantics and zero-area triangles. There are 670 RGBA resources, 1,280 draws and 7,998 vertices. This is the preserved world FBO used by the earlier investigation, not an integrated gameplay/HUD benchmark. HUD code is untouched; final HUD composition was not separately revalidated because the performance gate failed.

## Existing texture model

Eight 2 MiB CG ROMs contain 65,536 raw 16×16 tiles, one byte per source position. The tile-address ROM contains 1,048,576 little-endian 16-bit entries; the attribute ROM contains two four-bit attributes per byte, **high nibble first**. Rave Racer uses the 17-bit addressing interpretation, not the conditional System 22 16-bit map fixup.

For each baked texel, production computes source coordinates using the selected bake tier:

```
step_u = max(1, ceil(range_u / cap))
step_v = max(1, ceil(range_v / cap))
source_u = min_u + baked_x * step_u
source_v = min_v + baked_y * step_v
u = source_u & 4095
v = (source_v & 4095) | (texbank * 4096)
map_index = ((v & 0xfff0) << 4) | ((u & 0xff0) >> 4)
```

The map supplies the tile; attribute bit 0 extends its index to 17 bits. Local x/y are the low four coordinate bits. Attribute 4 flips x, 2 flips y, then 8 transposes them. **Flips precede the coordinate transpose.** References beyond the 16 MiB CG data supply raw pen zero. The optimized tile-run fallback for an out-of-range map address uses tile 0/attribute 0; the captured normal banks never require that fallback. The prototype does not implement arbitrary out-of-range bank requests.

With `c = cmode & 15`, interpretation is:

| Condition | Pen offset | Right shift | Mask |
|---|---:|---:|---:|
| `c & 4` | `0xec + ((c & 8) << 1)` | `2 * (~c & 3)` | 3 |
| otherwise `c & 2` | `0xe0 + ((c & 8) << 1)` | `4 * (~c & 1)` | 15 |
| otherwise | 0 | 0 | 255 |

Final pen is `(offset + ((raw >> shift) & mask)) & 255`. Palette selection uses the resource's palette group, with 128 groups of 256 RGB colours. Palette generations are live state: immutable ROM pens do **not** imply immutable colours. Pen zero is transparent only in the nonopaque model; captured Rave world textures are opaque, including pen zero. The shader preserves the existing alpha discard and vertex colour multiplication; priority output alpha comes from vertex alpha.

Caps are 256/512/1024, with existing sticky resident higher-tier selection. Used dimensions are ceiling-divided by step. Production's exact-size allocation can add one guard texel relative to used dimensions; that guard samples the **next actual source coordinate**, not a duplicated edge. Clamp-to-edge applies to allocated dimensions. Original normalized U/V scales already encode decimation and allocation dimensions. Captured perspective UVQ and its division are reused directly: the new shader must not substitute full-resolution source sampling or recompute vertex UVs.

The legacy power-of-two bake mode uses a different footprint and is not supported by this diagnostic. No integrated feature flag or production path was added.

## Persistent representations and verified memory

Two layouts were tested, with GL-owned texture objects allocated once before measurement:

| Resource | Raw layout | Oriented layout |
|---|---:|---:|
| R8UI pens | 4096×4096: 16,777,216 B | 4096×4112: 16,842,752 B |
| R32UI map | 256×4096: 4,194,304 B | same |
| RGBA8 palette | 256×128: 131,072 B | same |
| Total texel storage | 21,102,592 B (20.125 MiB) | 21,168,128 B (20.1875 MiB) |

Deduplication verifies **65,747 oriented tiles**, including a shared zero tile for invalid raw tile references. Unpadded oriented pens occupy 16,831,232 B; row padding adds 11,520 B. Pens plus map therefore occupy 20.0625 MiB after padding, consistent with the review's approximately 20.05 MiB payload estimate. These are API-level storage estimates; undocumented driver padding, allocation metadata and physical GPU residency were not measured.

The raw map packs the extended tile index and orientation bits. The oriented map stores a deduplicated tile ID and removes orientation handling from the fragment shader. Both use three dependent reads: tile map → pen → palette. No per-polygon texture allocation occurs. Twelve uint32 metadata values per frozen logical resource describe allocated dimensions, steps, origins, bank, palette group, cmode decoding and white/opaque flags: 32,160 bytes for 670 resources, plus existing geometry. The first gate uses three uvec4 uniforms per original draw. A production vertex metadata design was intentionally not implemented.

## CPU semantic verification and shader correctness

`tools/gpu_native_texture/capture_palette.py` privately relinks the existing native replay executable with a decoder destructor that exports the real replay palette. It does not replace normal build objects. The recorded first-lap replay was run through tick 2500; the export was compared against every captured RGBA resource before constructing GPU data.

`build_sources.py` reproduces **all 6,524,539 allocated texels in all 670 resources with zero differences**, including guard storage. This validates the captured palette colours, source orientation, cmode interpretation and bake steps; it is stronger than validating only visible pixels for that scene.

A plain floating `floor(uv * allocated_size)` was avoided because the previous atlas investigation established rounding failures. The prototype decodes the interpolated normalized float's exponent/mantissa, multiplies its mantissa by the allocation dimension using a split 64-bit integer product, and shifts to obtain the nearest texel index without a rounded floating product. It clamps before resolving the baked source coordinate. This matches the tested Mali sampler, but is not a proven portable emulation of every driver's sampling precision.

| Backend | RGBA vs preserved Mali reference | Raw vs same-backend RGBA | Oriented vs same-backend RGBA |
|---|---:|---:|---:|
| Mali-G31 GLES 3.2 r20p0 | 0 | 0 | 0 |
| llvmpipe GLES 3.2 Mesa 26.0.8 | 941 | 3 | 3 |

Native differences occur at framebuffer coordinates (425,27), (123,31), (261,243), bottom-origin readback. Both source layouts produce the same three discrepancies, while every CPU-generated resource texel agrees; the unresolved issue is sampling/interpolation fidelity rather than their source representation. No bias or tolerance was introduced. These differences were not resolved because the actual-target performance gate already failed. Native pixel validation therefore remains incomplete.

The frozen scene includes the previously troublesome original bake tiers and U scales, including polygons 978 and 1351, without changing their captured UVQ. However, no claim is made that one scene covers all requested boundary, flip, transpose, cmode, palette-change, transparent, invalid-reference, decimation or degenerate cases. A separate synthetic exhaustive suite and dynamic palette test were not run after the performance stop. The helper's oriented height is explicitly specialized to this ROM set; it is not a general renderer allocator.

## Controlled Mali performance

Device: actual RG40XX H/H700, `Mali-G31`, OpenGL ES 3.2 `v1.r20p0-01rel0.8ec337de4ddab3365a32cff58063a9da`. SDL's Mali backend and the game's EGL/GLES environment were used. ES was stopped during testing and restored by a shell trap. All data transfers completed before testing.

All RGBA textures and persistent sources were created/uploaded before measurements and completed once. Five warm-up pairs precede **30 alternating RGBA/native pairs per layout**. Each submission clears and renders the same 640×480 world framebuffer, streams the original 40-byte vertices, then calls `glFinish` in this standalone controlled diagnostic. Neither normal benchmarks nor production acquired a blocking synchronization call.

| Layout / shader | Median thread CPU ms | Median through completion ms | p95 through completion ms |
|---|---:|---:|---:|
| Raw-layout session, RGBA | 23.665 | 37.530 | 38.161 |
| Raw-layout session, GPU-native | 34.808 | 81.002 | 82.030 |
| Oriented-layout session, RGBA | 23.598 | 37.371 | 37.829 |
| Oriented-layout session, GPU-native | 34.902 | 77.942 | 78.737 |

Completion ratios are approximately **2.16× raw / 2.09× oriented**. Even the minimum native samples (67.105 / 63.798 ms) exceed the matched RGBA p95. This is not a comparison against the earlier isolated atlas timing or the live 52.9 FPS benchmark.

These times include command feeding, VBO streaming, draw submission, driver work, scheduling and completion. CPU submission can itself encounter driver backpressure. Extra per-draw uniforms contribute CPU cost. No exclusive shader execution, utilization or hypothetical CPU/GPU savings are claimed; no timer-query subtraction was performed. Clock/thermal telemetry and peak RSS were not captured for this small gate, so absolute values should not be treated as production timings. Alternating matched workloads and the large repeated separation support the decision despite that limitation.

There is no texture creation, deletion, palette upload or source mutation inside the measured render loop. Consequently the regression does not require the rejected atlas's per-resource retirement/allocation churn. The experiment establishes that this fully resident sampling implementation is expensive; it does **not** isolate integer-address ALU versus dependent-read latency, cache locality, compiler behavior or uniform submission as the individual cause. Removing orientation alone reduces the measured completion cost only modestly. No increasingly complex shader or allocator workaround was pursued.

## Batching and dynamic updates: design constraints, not implemented results

Shared source textures make draw consolidation conceptually possible, but the phase-3 cost gate did not pass. Draw-call consolidation was not attempted. Original clipping, fan ordering, equal-depth ties, zero-area boundaries and scissor requirements remain mandatory; the earlier guarded opaque-grouping lessons still apply. There is no measured batching benefit for this representation and no extrapolated FPS estimate.

For future work, ROM pens/maps can remain persistent after initial GLES-owner upload. Palette RGB must come from the **immutable presentation snapshot** selected for submission, not the newest simulation palette. Per-bank generation tracking could upload changed groups before that snapshot's draws; skipped presentations must compare generations against the last actually uploaded state. Changes must invalidate the appropriate logical sampling metadata and maintain HUD/world palette ordering. A bounded versioned palette ring with nonblocking retirement would need explicit lifetime validation if updates to in-flight storage prove problematic; it is not justified or implemented here. Generations alone do not make old palette contents available after a buffer has been overwritten.

Initial source readiness must precede sampling; no GLES object may be mutated on simulation/preparation threads. Runtime source mutation, if introduced beyond this immutable Rave ROM model, would require snapshot/version handling as well. No explicit stalls should be added to a live path as a correctness shortcut.

## Validation scope and retained artifacts

Native and ARM64 standalone probes compiled successfully. CPU resource equality and target world-frame equality passed. Native pixel equality did not pass. There is no integrated renderer, disabled-default feature flag, five-pair live-game benchmark, tick-1965/3386/3820 burst result, board/PCM/voice equivalence result or ThreadSanitizer result for this prototype. Those phases were deliberately not entered after the required stop gate; no game simulation threading was changed.

Reproducible source: `tools/gpu_native_texture/{capture_palette.py,build_sources.py,probe.c}`. The probe reuses the preserved frozen-scene loader/state setup from `tools/world_atlas_case/array_probe.c` without altering that file. Measurements: `benchmarks/gpu-native-20261014/{mali.log,native.log,summary.json,sources.json}`. Generated ROM-derived planes, palette and framebuffer readbacks remain temporary, not copied into the repository.

Preserved scene on handheld:
`/userdata/system/rave-racer-world-atlas-20261014/frozen-20261014-160318-25197`.
Device result directory: `/tmp/native-probe-results`; inputs: `/tmp/sources`; private probe: `/tmp/probe-arm`. Host inputs/output: `/tmp/rr-native-texture`, frozen resources `/tmp/rr-atlas/frozen`. Replay: `/userdata/system/rave-racer-replays/race-20261013-122242-25136.rec`.

Build the host diagnostic with `cc -O2 tools/gpu_native_texture/probe.c -o probe -lSDL2 -lGLESv2 -lEGL -lm` from the raverace directory. ARM64 uses the existing H700 cross compiler and `-lSDL2 -lmali -lm`. Run `probe FROZEN_SCENE GENERATED_SOURCE_DIRECTORY` from a writable results directory. Source generation runs from repository root: `python3 raverace/tools/gpu_native_texture/build_sources.py SCENE PALETTE_RGB OUTPUT`. The source builder requires NumPy and fails immediately on any CPU texel mismatch.

Further investment would require a materially different, independently justified sampling approach and an isolated correctness/performance gate, including resolution of native boundary behavior. The present evidence supports keeping the committed production renderer and ending this implementation rather than moving it into gameplay.
