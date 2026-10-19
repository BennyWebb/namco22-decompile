# GLES3 cleanup review — 2026-10-19

Status: local cleanup/gles3-main from main0837a39; no merge/cherry-pick, push or installation.
Review worktree: /tmp/rr-gles3-main. Original feature/main refs and feature worktree unchanged.
Source of truth is the deployed cone-fixed N1; full binary identities are in HISTORY.md.
This branch contains accepted source contents, never feature history or development payloads.

Local commits, in dependency order:
- 5af0693 sound: one strict ROM-resolved translation and test oracle.
- 965221f assets: deterministic ROM compiler and checked-in shader text.
- a848d0b platform: recording, sessions, clocks, watchdog, profile and packaging.
- 53401e9 runtime: direct GLES3 source tree, exact paths and shared interfaces.
- f981cd7 tests: hash-only replay/image/sound/semantic fixtures and retained tools.
- f0b7bea docs: operation, architecture, history and short handover.
- 42d2341 retirement: history/tree payload and legacy-composition audit.
- afbfa00 validation: desktop exact-option wiring and isolated device runner.
- b6d21de platform: first-install rollback and acceptance instructions.
- HEAD final report/audit: corrected smoke mapping requirement and audit byte accounting.

Composition and retirement:
- Direct rr_gles3 target; gles3/src, assets, shaders, tools and tests; platform/knulli-h700.
- Presets gles3-host, knulli-h700 and existing desktop; lifted68K/C25 O2, sound O1.
- No Python relinkers, milestone chains, private source rewriting, object caches or captured packs.
- No legacy H700 viewer/preset, world-atlas/multitexture/P-series variants, overload valve,
  RGBA pages, legacy world cache, per-picture fallback or shipped interpreter fallback.
- ARM symbol/link-map audit excludes legacy world renderer, tex_bake and sound interpreter.
- Neutral engine/render_stats.h owns renderer value types; desktop's backend remains independent.
- Exact B1/B2, WRAM and C352 fast paths remain; shared interfaces and other games are preserved.
- No changes under top-level include/, tools/ or other game folders; desktop rr builds/links.
- Other games were not rebuilt: their ROM inputs are unavailable, as accepted by the user.
- New sources include 75 GLES3 files, 11 platform files and four condensed documentation files.

Assets and fixtures:
- Whole-ROM compiler enumerates2421 models/97116 meshes/21783 materials, independent of captures.
- Asset layout/lookup/plan/texels/XYZ and all six shader texts match the deployed source of truth.
- ROM payloads exist only in ignored build/package/cache directories, never Git.
- Recordings use configurable local paths and mandatory input/EEPROM SHA256 checks.
- Reference input SHA256 d76551345170fbbe0b4700dfe91cebf36d2c1a408dda69de6c2b043a95043216.
- Cone input SHA256 de71c87454f1ddcbae69c71c6ca00ef48386964d55832abaea628a76c63320b4.
- Fixtures contain checkpoint/final/image/PCM/voice hashes and counters, no packet or sample bodies.
- Stable ROM IDs replace capture-specific validation IDs only after exact full command equality.

Clean host validation:
- Fresh single-branch clone /tmp/rr-clean-checkout, separate fresh host/ARM/desktop build directories.
- Runtime sources match the committed runtime; subsequent commits change tests/docs/audit only.
- Empty ROM build cache: compiler144.432s; empty first-launch cache: C decoder0.407s on host.
- Every regenerated asset verified; device also started with an empty candidate cache.
- Reference6006: all21 sets/63 records,4207 normalized and semantic packets,36 images exact.
- Normalization expands draw state per command; every command/order/transform/material/depth stays.
- Cone13808: all141 records, final state and26 images exact, including11822–11828 and11936.
- Attract36000: all360 records,20 sampled images exact; zero unsupported/sound traps.
- ROM provenance:36 images,5,529,600 decoded pixels, class(c)=0; boot159 CPU-oracle pictures exact.
- Unsupported fixture: exit86, loud message and identical top summary line.
- Sound PCM/voice and checkpoint state exactly equal interpreter every tick, zero traps:
  reference6006, attract36000, cone13808, fixture1700 and three dense1700 runs.
- Record/replay:600-tick state/board/audio checkpoints exact; overwrite and malformed cases rejected.
- CTest4/4: projection100M, culling1M, cache/reset/wrap and staging200 checks.
- Shared C352 general/fast transcript4615 states exact; texture decoder666144 sampled cases pass.
- Evidence: /tmp/rr-clean-final-host/, /tmp/rr-clean-record-replay.txt and fresh build link maps.

Device validation (isolated package; installed apps/Ports entries untouched):
- Candidate rr_gles3 SHA256520dafc912099b0761ba3fabd008c399fb350f8e70307cb3467286894e0a1656.
- Reference6006 and cone13808 checkpoint records exact; both pacing0/0/0.
- Reference race4207 pictures: render CPU p993.938ms, preparation p9911.626ms.
- Cone state5/0:1628 pictures, max18 draws (11936=17), render CPU p993.394ms.
- Bonus preparation p9915.415ms, max18.018ms; compiler p9910.482ms, walk p995.108ms.
- Two bonus preparation samples exceed panel period; no repeats. Known compiler/walk tail remains.
- Attract smoke:125s/7468 pictures, pacing0/0/0; mapped START+SELECT exits to EmulationStation.
- Initial smoke omitted the saved mapping; its synthetic chord was ignored. That run is excluded.
- Runner now requires GLES3_TEST_CONTROLLER_CONFIG before smoke; product input behavior unchanged.
- All final watchdog journals confirm restoration; CPU schedutil/GPU simple_ondemand restored.
- All47 protected paths, three installed binaries and both existing Ports entries unchanged.
- Evidence directory: /userdata/system/rave-racer-gles3-cleanup-test-20261019/.

Archive created and verified before cleanup, outside the repository:
/home/bwebb/rave-racer-archive/20261019-fdfd350/ (private; includes ROM-derived evidence).
- feature-all-refs.bundle: d786f8ada93055a404214564a82424126d0ffcf7b5682f907601ce163185b9b9
- benchmarks.tar.gz: 5d14809a40df060828a4d06119f535dfa7bcf9acc20dd1d90fcae42a157866c1
- regressions-and-goldens.tar.gz: 6637c0d0020a6a609856da775c460499e3949507afe546b5acd6f58f32057da6
Bundle contains all22 original refs; history and regression/benchmark tarballs are checksummed.

History/tree hygiene:
- 680 tracked files;132 changed/110 added versus main;10 local commits, all reachable history scanned.
- No newly introduced ROM payload, recording, benchmark/evidence path or file over1MiB.
- All25 protected main artwork/font/BIOS paths unchanged.
- All69 current files over1MiB and157 reachable large blob versions are unchanged main inheritance.
- Source sound dispatch manifest retains51 unresolved indirect variants plus CB49 as follow-ups.
- No installed replacement; future explicit installation and one-step restore are documented.
STOP for user review; no push or deployment authorized by this report.
