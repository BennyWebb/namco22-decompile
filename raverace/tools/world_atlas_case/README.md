# World texture sharing feasibility case

Status: isolated probes pass for compatible-dimension GLES3 arrays. A live,
disabled-by-default implementation passes correctness. Five pairs show a 5.8%
FPS gain with worse 1% low/hitches and higher memory, so it is rejected for
production deployment. Production executable and launcher are unchanged.

## Why arrays instead of ordinary 2D atlas pages?

A complete tick-2500 capture contains 670 exact decoded resources, 1,280 draw
commands and 7,998 vertices. It includes both normally selected bake tiers for
polygons 978 and 1351. The capture preserves allocation dimensions, decoded
bytes, original shader inputs, scissor, priority and actual depth bits.

Moving resources into three 2048-square pages with affine coordinate remapping
changes 14 framebuffer pixels. Explicit `floor(uv * dimensions)` changes two.
The original sampler multiplies coordinates more accurately than a rounded
GLSL float product at texel boundaries. Minimal failures are preserved under
`minimal/`; the responsible commands are 1196 and 1200 (sorted polygon indices
1512 and 1527), rather than the previous road resource-history discrepancy.

- At (109,312), U = `0x1.8ba2e8p-1`; exact U * 594 =
  458.99998712539673, but float32 multiplication rounds to 459.
- At (8,352), U = `0x1.5fad40p-1`; exact U * 594 =
  407.9999885559082, but float32 multiplication rounds to 408.

A split-product correction passed this scene but failed 170 additional CPU
boundary cases. It is rejected. No colour adjustments or coordinate biases
are accepted.

GLES3 RGBA8 arrays group only resources with identical allocation dimensions.
They retain original UV division, allocation dimensions, edge guards, bake
scales, NEAREST sampling and CLAMP_TO_EDGE. The extra constant-per-triangle layer attribute
selects a resource using one sampler and does not remap UVs.

## Results

Each handheld table uses 20 rotated samples from one build and frozen scene.
CPU measures the submitting thread; finish elapsed includes CPU submission,
driver work and GPU completion. It is not exclusive GPU time or full-game FPS.

| Exact-size cohorts | Draws | Median CPU ms | Median finish elapsed ms |
|---|---:|---:|---:|
| Original | 1280 | 15.280 | 28.588 |
| Array, original boundaries | 1280 | 15.117 | 30.642 |
| Array, adjacent merge | 1166 | 14.015 | 29.402 |
| Array, opaque grouping | 266 | 4.709 | 15.699 |

Exact-size cohorts use 259 arrays, 670 layers and 24.89 MiB RGBA payload.

| Fixed two-layer cohorts | Draws | Median CPU ms | Median finish elapsed ms |
|---|---:|---:|---:|
| Original | 1280 | 12.977 | 27.108 |
| Array, original boundaries | 1280 | 13.697 | 29.292 |
| Array, adjacent merge | 1248 | 12.964 | 28.691 |
| Array, opaque grouping | 441 | 6.174 | 18.038 |

Fixed cohorts use 431 arrays, 36.49 MiB allocated, 68.21% occupancy. All Mali
baseline fidelity, ordered, adjacent and grouped comparisons are zero pixels.
Native llvmpipe array comparisons also pass. Cross-GPU baseline differs at 941
pixels; this is not an array equivalence failure and is not waived in same-GPU
comparisons.

Grouping is confined to opaque priority rendering with distinct synthetic
ranks, identical effective shader/scissor state and no resource mutation.
The capture reports 24 depth bits despite requesting a 16-bit renderbuffer.
Minimum distinct-rank separation is 35.894 conservative 16-bit depth units.
Live grouping must require at least two units and retain original ties/fans.
Blended paths retain original rendering.

## Reproduction

`build_capture.py` creates a private capture executable using unchanged
production objects except private instrumented decoder/submission copies.
It never edits the production executable or its build objects. Set
`ENG_ATLAS_CAPTURE` to an existing empty directory and capture tick 2500.
`capture.inc` documents the portable binary layout.

Compile `array_probe.c` against SDL2/GLESv2 (GLES3 headers and entry points),
then run with the capture directory as the sole argument. On the handheld,
stop EmulationStation completely, use `SDL_VIDEODRIVER=mali`, and always restore
the frontend with a shell trap. `RR_ARRAY_COHORT_LAYERS=2` tests fixed cohorts.
Run in an empty output directory: framebuffer files are written there.

Raw results are in `results/`. Complete exact-resource captures remain both
on the handheld under `/userdata/system/rave-racer-world-atlas-20261014/` and
locally under `/tmp/rr-atlas/frozen/`. Original shader-equivalence assets are
untouched.

## Completed gates and limits

Live normal-resource reference draws, native/ARM64 full replay correctness,
HUD RGBA, deterministic state/PCM/voices, ordered texture updates and native
ThreadSanitizer pass. Five alternating complete-pipeline A/B pairs and the
supplementary GPU-span pair are preserved. Adaptive cache-history differences
are audited separately from controlled resource equivalence. The pacing and
memory results reject deployment, so no physical atlas trial was performed.
The original production executable and its known-good configuration are active.

## Live native zero-area fan finding

The initial flat-layer shader passed tick 2500 and all 350 presented Mali
pictures through tick 2500, but failed natively at tick 435 (125 pixels).
The failure reproduces with one nine-vertex polygon and one 16×16 RGBA texture.
The uploaded array layer and original 2D resource are byte-identical. Removing
the final zero-area fan triangle, using a constant layer, or using an
interpolated layer makes the isolated comparison exact. A GLES3 2D-sampler
shader also passes. Thus this case depends on the flat layer/array shader and
zero-area fan; it is not resource history or draw reordering. No geometry
removal or pixel waiver is used in the candidate.

The corrected candidate interpolates a highp layer value that is constant on
each triangle. Integer layer selection remains exact in the two-layer cohorts.
The original zero-area fan, positions, colour and UV inputs are retained.
`minimal/array-native435-*` preserves the original failing input. These three
files can be copied as `draws.bin`, `texture-0.rgba`, and `scene.meta` into a case
directory; add a zero-filled 1,228,800-byte `reference.rgba`. Its baseline has
zero covered pixels in the flat-layer failing case on the recorded llvmpipe
version. The current interpolated-layer probe passes; use the preserved flat
shader qualifiers to reproduce the earlier failure.

Final sampling uses explicit `textureLod(..., 0.0)`, valid because world
resources have nearest filtering and no mipmaps. Interpolated-layer implicit
LOD passed the isolated tick-435 case but failed 33 pixels at native tick 1742.
The explicit-level candidate passes all 2,297 native presented pictures through
tick 2500, plus simultaneous GPU HUD verification. Native state matches both
the preserved Mali endpoint and headless reference byte for byte; PCM/voice
digests match the headless reference. Rebuilt ThreadSanitizer validates 566
pictures through tick 1700 with exact HUD/world pixels, no application race,
and identical state/PCM/voice digests to the previous production TSan run. The
sole suppression remains the existing `race:libgallium` driver suppression.
The final guarded hardware gate and five performance pairs are complete; see
the final decision below.

## Reordered merge / zero-area boundary finding

An expanded fixed-presentation run finds four pixels at tick 3462. Exact
resource reads match; ordered array sampling, adjacent merging and reordering
without merging all pass. Delta reduction isolates original commands 105 and
143 using the SAME 23×32 texture: command105 is one point triangle with all
positions (503.375,0); command143 contains two valid small triangles near
(487,11). Combining these reordered commands in one draw produces four
differences on llvmpipe. The case does not depend on changing texture layers.
`minimal/array-native3462/` preserves both commands and their exact RGBA data.
The probe's general safety rule preserves separate original command boundaries
when a command contains a zero-area triangle. It removes no vertices and uses
no screen-area tolerance or polygon-specific exception. The full frozen scene
passes with this rule. The final complete-recording and Mali gates subsequently
passed for this submission change.

Guarded complete native recording: 2,002 exact world/HUD pictures through
tick 6006. Endpoint JSON, framebuffer, geometry digest, ordered texture-miss
trace and PCM/voice values match the preserved production run byte for byte.
The final guarded TSan run through 1700 also passes566 pictures with identical
state/PCM/voices and no application race. Hardware fixed-history testing subsequently passed 1,880 pictures; all five
alternating first-lap pairs are complete.

## Final end-to-end decision

Five complete alternating first-lap pairs:52.93 ->56.02 FPS (+5.8%), but1% low
25.15 ->21.70 FPS (-13.7%) and median worst hitch91.9 ->106.6 ms (+16.0%).
Draws drop49%; mean upload/driver wall time rises0.472 ->0.865 ms/picture.
All runs have zero audio underruns/resyncs and identical simulation endpoints.
The candidate is not deployed; the original production launcher/executable
remain active. Keep the prototype OFF. Full results and section tables are in
../../benchmarks/world-atlas-20261014/RESULTS.md.

Final frozen scene: zero RGBA differences in all four modes; guarded opaque
grouping uses 499 draws. Raw final 20-sample timing is
`results/mali-final-guarded-frozen.txt`; prior unguarded variants remain labelled
historical evidence. Supplementary nonblocking GPU-span profiling uses
`build_gpu_profile.py`, with the multi-sampler feature still compiled OFF.
Both timer modes use one private binary; GPU spans are not active execution
and are not added to the separate headline CPU/FPS measurements.
