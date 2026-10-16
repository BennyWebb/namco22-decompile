# C1d — Residual pixel provenance and device readiness

| Gate / result | Evidence |
|---|---|
| **Host fidelity gate PASS** | **Class (c)=0; all5,875 residuals proven adjacent-source choices** |
| Classes (a) / (b) / (c) | **0 / 5,875 / 0** across36 sampled images |
| Source identities | Same material and bank in every residual; no bank/material mismatch anywhere in18 world frames |
| Sampling distances | 2,777 horizontal,3,090 vertical,8 diagonal; all inside original UV rectangles |
| Provenance fidelity | 36/36 world colour proofs exact;11,059,200 sampled ROM/palette colours verified |
| Replay determinism | Four6006-tick runs, each21 checkpoint sets/4207 adjusted command receipts exact |
| Readiness | **Ready for unchanged C1b Part2 device go/no-go; not started** |

## Implementation and proof method

Read HANDOVER/C1c report. Started clean on `feature/knulli-h700`, HEAD
`15bbc87599118e952c89e0eda1d7f4b78276e421`. C1d remains private under `native/c1d`.
The withdrawn exact-layout control is not built, used or required; no alias separation.
The active index derivation now defaults to the matching **lowp palette sampler**.
Normalised nearest `texture()` sampling is retained; fetch remains selectable.
Earlier C1b/C1c files remain as history; the C1d derivation carries the adoption forward.

`ENG_C1_DEBUG=provenance` enables both paths; unset leaves ordinary colour output.
Instead of world colour, GL_RGBA32UI records `(bank,source_u,source_v,material+1)`;
zero material marks background. Same geometry, synthetic depth,16-bit depth buffer,
scissors, near clipping, batching, priorities, snapshots and GL ownership are retained.
No CPU scheduling, simulation, physics, audio or normal rendering algorithm changes.

To observe actual nearest selection rather than approximate it with floor(), a
coordinate-coded texture of each page's exact dimensions/filter/wrap is sampled at
**the same interpolated normalised UV**. RGBA uses an RGBA8/lowp grid; index uses
an unsigned integer grid. Each returns the selected atlas x/y. Exact integer
per-material origins resolve these back to the original recipe's bank and u/v,
modulo4096. Logical recipe identity is preserved even for colour/content aliases.
No screen-neighbour colour search, UV bias, tolerance or guessed sampling coordinate.

Two additional debug MRT attachments record the ordinary shaded colour and raw
sampled colour. Every one of36 world colour-proof readbacks equals the corresponding
normal render byte-for-byte. Independently decode each covered source address from
ROM, apply its exact material palette, and compare to GPU raw-sample readback:
**11,059,200/11,059,200 exact**, with no wrong bank, pen, palette colour or shade proof.
This validates the provenance diagnostic against actual colour sampling/rasterisation.
Extra debug targets, textures, copies and shutdown exports are host diagnostics only.

Four full replays: colour RGBA/index and provenance RGBA/index. All match21
checkpoint sets/63 domain records and4207 pre-batching adjusted command receipts.
Both RGBA runs also retain4207 original raw packet hashes. Normal36-image outputs
match C1b RGBA and C1c lowp-index exactly, preserving the same5875 residual population.
No class-(c) fix or replay retry was needed. All logger drops/errors are zero.

## Per-image classification

The36 samples are world/composed pairs at18 ticks, not36 distinct ticks. World
provenance is captured once per tick/path; each composed residual is also a world
residual at that same framebuffer coordinate. The unchanged HUD composition depends
on that world pixel; its font texels are not falsely labelled as world ROM texels.
For both sample types use the actual world-source identity at the residual coordinate.
All classes below are `(a)/(b)/(c)`; coordinates/examples use bottom-origin pixels.

| Tick | World a/b/c | Composed a/b/c | Max world/composed channel error |
|---:|---:|---:|---:|
| 1800 | 0/95/0 | 0/88/0 | 92/137 |
| 1965 | 0/238/0 | 0/231/0 | 88/108 |
| 2500 | 0/140/0 | 0/135/0 | 48/88 |
| 2643 | 0/199/0 | 0/195/0 | 80/138 |
| 3000 | 0/176/0 | 0/169/0 | 49/87 |
| 3546 | 0/180/0 | 0/173/0 | 83/94 |
| 3600 | 0/204/0 | 0/197/0 | 57/63 |
| 4200 | 0/194/0 | 0/182/0 | 87/104 |
| 4800 | 0/188/0 | 0/175/0 | 139/148 |
| 5400 | 0/221/0 | 0/209/0 | 104/104 |
| 5639 | 0/198/0 | 0/186/0 | 98/100 |
| 5655 | 0/160/0 | 0/154/0 | 54/112 |
| 5867 | 0/143/0 | 0/136/0 | 89/135 |
| 5874 | 0/169/0 | 0/158/0 | 51/87 |
| 5878 | 0/122/0 | 0/115/0 | 87/133 |
| 5889 | 0/132/0 | 0/123/0 | 60/91 |
| 5921 | 0/152/0 | 0/141/0 | 60/79 |
| 6006 | 0/100/0 | 0/97/0 | 101/162 |

Class(b) is eight-connected adjacency: |du|≤1 and |dv|≤1, not both zero, identical
material/bank. All5875 satisfy the stronger **original UV rectangle bounds** test;
none needs gutter inclusion or wraparound to justify adjacency. Classes(a)/(c) have
no examples because both are empty. The8 diagonal image-pixels are explicit corner
boundary choices, not rounded into a looser distance criterion.

| Example (image; x,y) | Material/bank | RGBA u,v → index u,v | Class / colour error |
|---|---|---|---|
| 1800 composed;163,49 | 758/12 | 2984,2967 → 2983,2967 | b /14 |
| 3546 composed;225,130 | 2808/12 | 2490,3049 → 2491,3048 | b diagonal /2 |
| 6006 composed;221,404 | 933/3 | 392,2136 → 392,2137 | b /162 |

Large RGB deltas are therefore high-contrast adjacent texel choices, not missing
geometry or wrong palettes/texels. Different fresh atlas affines select opposite
sides of nearest-sampling boundaries; integer provenance proves the choice in each
case. This meets the authorised visual-fidelity rule without relaxing pixel checks
for unexplained cases. No residual is silently dropped or accepted by error magnitude.

## Readiness, scope and final state

**The reference compact index atlas is ready for the unchanged C1b Part2 device
go/no-go. Do not start it without authorisation.** Stock-GPU pacing/audio, fenced
world+HUD p95≤14ms and measured GPU-memory reduction≥3.5× remain to be established
on H700. Conditional420MHz testing remains only the separately authorised fallback.
This is host fidelity readiness, not a performance pass, production readiness or
full-game coverage claim. Existing M3/P3 performance gates are not retrospectively passed.

The prior36 C1c side-by-sides/overlays remain valid: new colour captures match them
exactly. [Evidence](benchmarks/native-c1d-20261016/README.md) contains full pixel/source
coordinates, every count/example, readback hashes, proof results and replay receipts.
Raw debug targets stay in ignored `native/generated/c1d`, not Git. No alias-control
redesign, other-course/dynamic-format work or unrelated optimisation is performed.
Host only: no SSH/device transfer/run, clocks/services, production/launcher/settings/
saves/replays/controller changes, commit or push. Branch HEAD unchanged. **STOPPED.**
