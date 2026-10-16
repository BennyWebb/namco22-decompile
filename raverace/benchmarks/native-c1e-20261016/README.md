# C1e — compact index atlas, isolated H700 evidence

Complete; formal **NO-GO**, stopped. Branch `feature/knulli-h700`, starting HEAD
`eb9a4ac5910dab65217573c660b4c42e27bc8c4a`.
Private build/runner/analysis sources: `native/c1e/`.
Raw binary packs, ROM-derived readbacks and local build outputs remain ignored in
`native/generated/c1e/`; device work is confined to
`/userdata/system/rave-racer-native-c1e-20261016`.

Method: full 6,006-tick reference replay; native 1800–6006; queue 2, strict FIFO,
CPU userspace 1512MHz, GPU stock, default priorities. Each runner saves and restores
CPU/GPU governors and bounds via EXIT trap, verifies 47 protected files and the
production executable, and restores EmulationStation. No timing-run transfers.
Cooldown is ≤55°C or 180s idle. Timing play builds contain no packet/image SHA work.
One-fence diagnostics and image/provenance captures are separate from play timings.
Nearest-rank p95/p99; retain every sample, including failures.

Memory observer: single long-lived SCHED_IDLE/nice19 process at 100ms, buffered
writer, no PSI, thread stacks or vmstat reads. Records per-core frequency, GPU
frequency/governor/transitions, thermal/cooling, RSS, MemAvailable, CmaFree and Mali
allocated pages. Native play and debug-memory results are kept separate.
Capture-only transport uses four preallocated 7,372,800-byte slots (29,491,200 bytes)
and a low-priority writer; no capture-file writes on rr-present. Identical-size
provenance coordinate grids share storage. Neither changes source/shader math.

Setup corrections: initial extraction requested unsupported uid/gid ownership on
userdata; rerun with `--no-same-owner`. Initial protected-file input was an old
`: OK` status listing, not a checksum manifest; reconstructed a current manifest
for all 47 paths before any game test. These are not performance samples.

Debug-memory refinement: the normal RGBA smoke's minimum MemAvailable was
115,040KiB. The full 2D coordinate grids would additionally require ~75MiB GPU,
a temporary 64MiB CPU grid, and touched capture slots: unsafe at that peak.
The final debug build instead samples independent X/Y axis textures with exactly
the source page's width/height, nearest/clamp and matching sampler precision.
Nearest sampling is separable; colour sampling, depth, geometry and materials are
unchanged. Host controls prove all 108 readbacks (18×3×2 paths) byte-identical to
full-grid provenance; both new replays pass authoritative/command checks. This
change only affects debug resources. The play binary used for both paths remains
the original C1e play relink. See `host/axis-control.json` and `host-axis/`.

The fresh Mali RGBA colour captures match all 36 prior P3 validation image hashes
exactly (`mali/rgba-p3-control.json`). Smoke receipts show CPU1512MHz throughout,
GPU240MHz, lowp palette sampling, normalized nearest sampling, and play packets=0.
Default main/preparation/presentation policies are confirmed by the RGBA census;
the index census was attempted after that smoke exited, so the verifier references
the same play binary's RGBA census rather than inventing an index census.

Mali fidelity: all 21 sets / 4,207 adjusted raw and canonical packets pass in both
colour and both provenance replays. All 36 fresh RGBA images equal the prior P3
Mali hashes. Compact-vs-RGBA residuals: 819, entirely class(b), 409 horizontal and
410 vertical adjacent texels, all inside original UV bounds; a=0,c=0. No material
or bank mismatch anywhere in 18 world frames. Every composed residual is a world
residual at the same coordinate. All 36 MRT colour proofs and 11,059,200 sampled
ROM/palette colours are exact. See `mali/classification.json`, `pixels.csv` and
`capture-proof.json`. All 36 side-by-side/overlay PNGs and six reviewed contact
sheets are retained at `native/generated/c1e/review/`.

Additional measurement setup corrections (all attempts retained): the first atlas
probe reused one process; Mali retired its old context asynchronously, producing
a negative second baseline. Replacement probes use one fresh process per format
and require three zero-context samples before SDL startup. Both new baselines
are 80 pages. Atlas-associated driver deltas: RGBA117591 pages, index15043 pages,
7.81699× reduction; the three palette textures are an additional384KiB API payload.
Native play total counters include palettes, buffers and HUD, and are reported
separately, including the legacy overlap peak.

The initial verbose RGBA fence run completed6006/all21 checkpoint sets, but its
native trace (held in a16MiB RAM buffer) was submitted as one oversized stdout record:1,966,080 bytes
dropped, only194 complete C1E records. Its GPU distribution is invalid. The
corresponding index attempt was cancelled after identifying this setup issue;
clock/service cleanup verified. Replacement fence diagnostics use a separate
compact-log build: same GL/compiler/gameplay, one C1E record containing completion,
world/HUD submission and fence wait, with the duplicate verbose FRAME removed.
Both replacement smokes retain101/101 native records and have zero log drops.
No failed/partial population is used as a full-race performance result.


Final results: both stock index play runs have zero repeats, settled underruns and
main gaps>50ms. Stock one-fence index p95=16.271169ms; fixed420 p95=16.263011ms,
both above14ms. Fidelity and atlas-memory gates pass; no conditional GO.
See ../../NATIVE_C1E_REPORT.md for the result tables and limitations. No soak.
User observed the colour replay: “Looks good to me”; exact run not identified.

420 setup: the first smoke attempted unavailable performance governor and aborted
before launching the game, with verified cleanup. Corrected runner keeps the only
available simple_ondemand governor and clamps min=max420MHz. Replacement smoke
records all384 GPU samples at420MHz and all1536 CPU samples at1512MHz. Full420 run
records705/705 GPU samples420MHz, all cores1512MHz, both cooling states0.
All final full runs pass21sets; all four Mali capture runs also pass4207 adjusted
raw/canonical command receipts. validation-summary.json records exact checks.

Final independent receipts are in runs/device-session/:47 protected files exact,
production hash unchanged, all original CPU/GPU bounds/governors and RT values
restored, EmulationStation PID5200, no game/probe/sampler. Final uptime39320.78s;
first diagnostic preparation35207.57s (~68.6min; including earlier read-only reachability,
~70min). No further runs scheduled. All timed runs meet the start-temperature rule.
No production deployment, commit or push; branch and HEAD unchanged.

Reproduction: native/c1e/build.py derives private host/ARM play/validation builds;
--compact-fence changes diagnostic logging only. prepare_axis.py regenerates the
bounded provenance assets. run_h700.sh saves/restores clocks and protects production;
verify.py checks state/adjusted packets; analyse.py computes nearest-rank full-race
statistics (--gpu-hz420000000 for the labelled420 run). classify.py checks sampled
source identities; review.py creates side-by-sides. Run commands/receipts and build
hashes are retained. publish.py losslessly compresses telemetry; telemetry-manifest.json
contains source/stored hashes. Binary ROM-derived packs/readbacks remain ignored;
readback-manifest.json records hashes, private review PNGs remain available locally.
Initial source-build-sha256.json is historical; final-source-sha256.json labels current
private scripts/build descriptions. Every failed attempt is retained separately and
never mixed with the valid timing population. C1e does not pass historical M3/P3 gates.
