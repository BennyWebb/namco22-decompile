# C1b — sizing PASS / host fidelity NO-GO / STOPPED

[Report](../../NATIVE_C1B_REPORT.md) (176 lines) records the unchanged gate and stop.
Branch feature/knulli-h700, clean starting HEAD
`1482183fffd0e3ac2b17e8ff50111050f5c607c3`. No commit, push or deployment.

- `sizing.json`: ROM provenance, inventory, exact dedup/merge counts, page sizes,
  per-page hashes, timing and sizing gates. Whole ROM is seven 4096² R8UI pages,
  112 MiB. Reference is 58.6875 MiB (four pages; 64 MiB if all full extent).
- `reference-parity.json`, `reference-parity-materials.json`: all 5,407 material
  rectangles/120,048,149 texels exact, including source-aware two-texel gutters.
- `unresolved-reference-footprints.json`: three keys absent from static enumeration;
  these are footprint counts, not frame/object counts. Runtime point-RAM/direct/
  dynamic UV occurrence counts cannot be established from ROM alone.
- `host-validation.json`: both 6006-tick/21-set replays, RGBA 4207 original packet
  hashes, matching 4207 adjusted pre-batch command hashes, prior P3 RGBA images.
- `image-comparison.json`: every image's hashes, RGB/alpha differences and complete
  changed-channel error histogram. All 36 images differ; 852,400 RGB pixels,
  alpha exact, maximum channel error162. Not accepted or fully explained.
- `image-review-manifest.json`: full paths and SHA256 for every side-by-side PNG.
  Layout: RGBA left, index centre, magenta difference mask right; top-origin.
  Raw images and difference arrays stay ignored under native/generated/c1b.
- `identities.json`: source, binary and audit input identities.
- `setup-history.txt`: setup failures retained and corrected; no outliers removed.
- `runs/host-final-{rgba,index}`: clean validation game output, all checkpoints,
  image receipts, adjusted command receipts and thread telemetry. Validation is
  unpaced/offscreen/audio-output-off; timings are not H700 performance results.
- `runs/host-{rgba,index}`: first setup attempt, retained separately. RGBA used a
  prior successful build; index reached6006 but failed diagnostic shutdown export.
- `runs/build-host*.txt`, `runs/sizing*.txt`, validation logs: build/setup history.
- `telemetry-manifest.json`: hashes and lossless source hashes for published logs.
- `production-check.txt`: read-only final H700 hash/clock/EmulationStation proof.

ROM-only tool / private renderer sources: [native/c1b](../../native/c1b/README.md).
The whole-ROM enumeration/index construction does not read recordings. Reference
pack recipes and exact stored palettes are used only for reference sizing/parity.
No ROM bytes, index page, shader output image or executable is published here.

All page texels are verified after packing (684,129,770 original footprint texels
across the two populations, including duplicates). Hash matches are confirmed by
byte comparison. The greedy merger stays within bank/cmode, keeps exact source
coordinates and only accepts area-saving overlapping unions; packing uses the
existing nonrotating MaxRects implementation and four-texel dimension alignment.

Host correctness fails despite exact source texels and source command parity.
No device smoke, validation, timing, fence, 420MHz run or soak was authorised to
proceed past that failure. GPU performance and measured residency are unmeasured;
this is not a claim that design B cannot meet the budget. Production remains
unchanged, governors stock and EmulationStation running. Further fidelity
attribution requires separate authorisation. STOPPED.
