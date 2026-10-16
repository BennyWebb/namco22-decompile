# C1d — host provenance gate PASS / ready for device go/no-go / STOPPED

[Report](../../NATIVE_C1D_REPORT.md),113 lines. Branch feature/knulli-h700;
clean starting HEAD `15bbc87599118e952c89e0eda1d7f4b78276e421`.
No device contact, transfer, run, clock/service change, commit or push.

All5875 residual image-pixels are proven class(b): same material/bank, adjacent
original source texels, both inside the original UV rectangle without relying on
gutters or wrapping. Class(a)=0, class(c)=0. Distances:2777 horizontal,3090vertical,
8diagonal (eight-connected adjacency). Largest colour difference162/255 is an
adjacent high-contrast choice. No residual is omitted or accepted by tolerance.

## Evidence

- `classification.json`: per-image a/b/c counts, maximum errors, source-coordinate
  examples, gate and all-world bank/material checks (zero mismatches).
- `pixels.csv`: every5875 residual coordinate, both GPU-observed bank/u/v/material
  identities, distances, original bounds check and actual colour error.
- `capture-proof.json`: all36 world diagnostic captures (18ticks ×2paths), original
  colour proof differences0, raw ROM/palette colour mismatches0, total11059200
  covered pixels. Raw alpha mismatches0. These are exact checks, not tolerances.
- `validation.json`: four6006-tick replays; each21sets/63domain records and4207
  adjusted command receipts, zero logger drops/errors. RGBA also retains4207
  original packet hashes. Normal36images match the preserved C1b/C1c baselines.
- `runs/{colour-rgba,colour-index,provenance-rgba,provenance-index}`: every raw
  timing/status/checkpoint/command receipt. All timings are diagnostic host data,
  never H700 performance evidence. No runs failed and no replay retry was needed.
- `raw-readback-manifest.json`: full paths/bytes/hashes for180 ignored readbacks:
  72normal images and108debug targets. No raw ROM/image/provenance payload is Git data.
- `identities.json`, `runs/build.json`: source/input build provenance and host binary.
- `telemetry-manifest.json`: published raw telemetry hashes and lossless source hashes.
- `gate.json`, `completion.json`: final PASS, scope, unchanged HEAD and STOP.

The withdrawn exact-layout control is not built or used. No alias-separated control
is created. Matching lowp palette precision is adopted in the C1d index derivation;
P3/C1b/C1c source remains historical and unchanged. Extra coordinate/mapping reads
and MRT attachments exist only with ENG_C1_DEBUG=provenance, not in the candidate
colour shader. This does not add tile-map indirection to normal rendering.

## Provenance format and validation

The integer target is640×480, tightly packed little-endian uint32 RGBA:
`bank,source_u,source_v,material_id+1`. Zero material denotes background. Coordinate
space in CSV/targets is bottom-origin. Source U/V are original virtual ROM-bank
texel coordinates0..4095, not oriented CG dictionary tile offsets.

A nearest-sampled coordinate-coded texture with the exact active page dimensions,
filter/wrap and interpolated normalized UV observes selected atlas x/y. Integer
per-material origin metadata resolves those coordinates back through the atlas
placement to original bank/U/V, including exact-content aliases. RGBA's grid uses
RGBA8/lowp, index's uses unsigned integer sampling. The actual colour texture is
sampled alongside it, not replaced with the grid. Independent ROM+palette decoding
matches every sampled raw colour. A second RGBA8 target reproduces every original
world colour byte, preserving the original16-bit depth and rasterization rules.

There are18unique sampled ticks and36world/composed images. Every composed residual
is a world residual at the same coordinate, so the underlying world-source map is
used for that contribution. HUD font texels are not labelled as world ROM data.
All ordinary world/composed captures still match the historical normal paths.

Colour-proof and raw sampled-colour targets accompany each18tick provenance file:
`tick-T.provenance.u32`, `tick-T.proof.rgba`, `tick-T.texel.rgba`. Raw files stay in
`raverace/native/generated/c1d/{colour-*,provenance-*}/images/`.
The prior C1c review PNGs remain valid because new normal colour images are exact:
`/home/bwebb/dev/namco22-decompile/raverace/native/generated/c1c/review/`.

[Reproduction/source](../../native/c1d/README.md). Gate PASS establishes reference
host fidelity readiness only. C1b Part2 remains unchanged: actual H700 pacing/audio,
fenced world+HUD p95≤14ms and measured GPU memory reduction≥3.5× still require
separate authorised hardware validation. Historical performance gates remain failed.
No device work was started. STOPPED.
