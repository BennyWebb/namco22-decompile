# C1c host fidelity attribution — BLOCKED / STOPPED

[Report](../../NATIVE_C1C_REPORT.md),135 lines. Clean starting HEAD
`65028074a7d3393712d957c72725f36d56c26ec3`, feature/knulli-h700.
No device contact, production change, commit or push.

The normalised integer sampler reproduces all36 C1b highp/fetch images exactly.
Matching RGBA's lowp palette qualifier removes846527 original pixel differences;
5873 original pixels remain, plus2 new residuals. The literal RGBA-layout pen
control has22/36 exact images and426 differing pixels. All3 new replays pass all
21 authoritative sets and4207 adjusted command hashes; the layout control also
matches all4207 original raw packet hashes. Device Part2 is not recommended yet.

## Evidence index

- `gate.json`: unmet exact-control and residual-provenance gates; no waiver.
- `layout-alias-constraints.json`:5244 physical texels with no common allowed pen
  across shared original palettes (6690 material-texel failures in first-pen pack).
- `layout-witness.json`: concrete page4/(1927,3897) alias: RGB38/51/76 requires
  pen3 for material635/group1, pen4 for material4664/group2; empty intersection.
- `layout-interior.json`:3438 conflicts lie inside at least one footprint interior.
- `layout-control.json`: page dimensions, original material metadata equality,
  overlap counts, source ROM hashes. This is a conflicted diagnostic, not a valid
  replacement pack. Colour aliases must be separated/remapped for source exactness.
- `runs/layout-alias-locations.json.gz`: complete5244 physical conflict locations.
- `layout-images.json`, `layout-pixel-differences.csv`: all36 control comparisons,
  every426 changed coordinate and RGB pair; maximum error145.
- `sampler-analysis.json`: all36 compact images, each category's counts/maxima/
  example locations. These are counterfactual attributions, not proven neighbours.
- `pixel-attribution.csv.gz`: **every** original differing pixel and both new
  residual pixels, original/revised maximum channel errors and classification.
  Class b here conservatively means not proven adjacent; it does not assert
  every residual is a nonadjacent or wrong texel. Proven class a=0, incomplete.
- `validation.json`: three complete6006-tick replays, exact states and command
  receipts, normalised/highp identity to C1b, layout raw packet identity.
- `runs/{texture-highp,texture-lowp,layout-lowp}`: all game output, settings receipts,
  checkpoint sets, packet/semantic SHA files and thread telemetry.
- `runs/prepare*.txt`: initial pen-alias failures, exhaustive constraint diagnosis,
  and the explicitly conflicted first-pen control used to test actual visibility.
- `identities.json`, `runs/build.json`: source/input/binary/build identities.
- `telemetry-manifest.json`: lossless raw telemetry/source identities.
- `review-manifest.json`: full paths/hashes for78 PNGs (36 comparisons,36 overlays,
  six contact sheets), kept ignored locally. Raw images and location NPZs retained.

All coordinates in tables/CSV/NPZ are bottom-origin framebuffer coordinates.
PNG review images are top-origin. Comparison layout is RGBA left, revised compact
index/lowp centre, magenta difference mask right. Separate overlays blend magenta
onto the index image at differing pixels. The written assessment is in the report;
no user review approval or exhaustive source-neighbour proof is claimed.

## Reproduction / boundaries

See [private host tools](../../native/c1c/README.md). No C1b file or shared engine
is edited; C1c derives a private validation build. Existing C1b RGBA and fetch/highp
captures serve as baselines and remain intact. New texture/highp and texture/lowp
replays differ only by palette sampler precision. Timing data is retained solely
for completeness and is not a performance result. No outliers are removed.

The source-control representability failure is proven; its every rendered pixel
has not been mapped to its material. Compact residual adjacency likewise remains
unproven. Do not mistake image similarity or source texel equality for that proof.
No redesigned control, GPU provenance renderer or device experiment was begun.
Further work requires a decision on separating the incompatible control aliases.
