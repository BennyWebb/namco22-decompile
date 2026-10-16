# C1b compact palette-free atlas — experimental / STOPPED

Sizing PASS; current host renderer fidelity FAIL. See
[report](../../NATIVE_C1B_REPORT.md) and
[evidence](../../benchmarks/native-c1b-20261016/README.md).
Do not use this prototype as the selected playable or launch further tests
without new authorisation. P3 selected build and shared engine remain untouched.

Host sizing, from repository root:

```
python3 raverace/native/c1b/size_atlas.py roms/raverace.zip \
  raverace/native/generated/milestone4a-coverage/race-live-v2 \
  raverace/native/generated/milestone4a-coverage/lookup/live.lookup \
  raverace/native/generated/c1b raverace/benchmarks/native-c1b-20261016
```

It decodes all static point-ROM model footprints, exact-deduplicates final pens
including source-aware gutters, greedily merges overlapping area-saving unions
within bank/cmode, exact-deduplicates again and calls the existing atlas packer.
Every original footprint is read back through its placement and compared with
ROM decode. Reference RGBA parity uses the existing pack and its stored exact
palettes. No recording is opened by this tool. Outputs stay ignored; do not
publish ROM-derived mappings, pages or extracted ROMs.

`prepare_reference.py` emits reference material affines/group metadata and
opaque/cutout shader sources into the ignored atlas. `build.py BUILD host
--validation` privately derives from P3 b2 through the existing P1b/P2 chain.
Example BUILD: `/tmp/rr-m3-reference-gl`. No ARM build was made for C1b.
`ENG_C1_TEXTURES` is unset/rgba by default; index requires `ENG_C1_ATLAS`.

The index prototype uses three fenced palette resources, revisions copied into
immutable queue slots, slot-only palette reads on presentation, and palette-free
material resolution. The current reference is fully opaque; the separate discard
shader is not exercised. Warmup packet conversion only changes page-group runs.

`validate_host.py` runs separate full RGBA/index validations into new
`host-final-rgba`/`host-final-index` directories. Use a fresh private workspace
for reproduction; it refuses existing run directories. `ENG_C1_IMAGES` keeps
36 raw images in bounded memory and exports them only at shutdown, outside
active gameplay. `ENG_C1_SEMANTIC` hashes the pre-painter command arrays with
page zeroed, while original packet hashes are retained separately. Neither
image capture nor extra semantic SHA is inserted into a play build.

`compare_images.py` writes statistics, side-by-side PNGs and per-pixel difference
arrays. It uses NumPy and standard-library PNG encoding, no Pillow dependency.
All36 current host images differ; no driver/sampling/precision explanation is
proven for every pixel. No correctness tolerance or shader bias was added.
No H700 timing or GPU-memory claim can be made from these validation runs.
