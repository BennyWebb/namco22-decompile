# C1c host-only sampler/alias attribution — BLOCKED / STOPPED

Read [report](../../NATIVE_C1C_REPORT.md) and
[evidence](../../benchmarks/native-c1c-20261016/README.md) before reproduction.
No device capability or ARM build is used. No shared engine/P3/C1b source changes.

`prepare.py` builds original-layout pen pages using the existing ROM decoder and
RGBA page dimensions/placements/material metadata. It exhaustively checks palette
constraints at shared pen conflicts, writes a concrete witness and deliberately
labels the first-pen control conflicted. It is **not a valid replacement pack**:
5244 shared texels cannot satisfy all original palettes. The first versions
asserted at these conflicts; logs and complete coordinates are retained.

`prepare_sampler.py` independently copies the valid C1b compact atlas and writes
four opaque shader variants: normalised texture() / integer texelFetch(), each
with highp / lowp palette sampler. The existing UV computation is preserved.
`build.py` derives a private C1b host validation binary and adds ENG_C1_SAMPLER
(texture default, fetch retained) and ENG_C1_PALETTE_PRECISION (highp default,
lowp diagnostic). It changes no gameplay, snapshot or compiler algorithm.

`run_sampler.py` runs complete texture/highp and texture/lowp host replays, recording
actual settings,21checkpoint sets,4207adjusted command receipts and36rawimages.
`run_layout.py` tests the explicitly conflicted layout with texture/lowp;22/36
images are exact. Original4207rawpacket hashes match because original grouping,
materials, UV affine and geometry remain. All run directories must be new.
These scripts are reproduction tools, not authority to continue past the stop.

`analyse_sampler.py` partitions every original differing pixel into counterfactual
removals and residuals, without declaring any unproven adjacent source texels.
`review_sheets.py` makes six contact sheets covering all36 world/composed images.
`export_evidence.py` exports complete coordinate/error CSV and safe telemetry,
retaining generated ROM/index/image payloads only in ignored native/generated/c1c.
`native/p2/publish.py` preserves eligible telemetry losslessly in the evidence dir.

No claim of exhaustive source-texel adjacency, exact valid shared-layout control,
Mali fidelity or performance is made. Separating incompatible colour aliases and
proving residual GPU sampling provenance require further authorisation.
