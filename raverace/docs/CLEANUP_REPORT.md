# Cleanup and GLES3 readability review — 2026-10-19

Branch: cleanup/gles3-main, from main 0837a39; readability baseline a583ac3.
Worktree: /tmp/rr-cleanup-push-scan. Main and feature/knulli-h700 are unchanged.
No merge/cherry-pick, PR or deployment. Installed applications and Ports entries are unchanged.
The deployed cone-fixed N1 remains behavioral ground truth; identities and private archive: HISTORY.md.

Original cleanup commits:
- 5af0693 sound: single strict ROM-resolved translation and interpreter test oracle.
- 965221f assets: deterministic ROM compiler and shader text.
- a848d0b platform: recording, sessions, clocks, watchdog, profile and packaging.
- 53401e9 runtime: direct GLES3 tree and preserved shared interfaces.
- f981cd7 tests: hash-only replay/image/sound/semantic fixtures and retained tooling.
- f0b7bea docs: operation, architecture, history and handover signpost.
- 42d2341 retirement: history/tree exclusion audit.
- afbfa00 validation: exact desktop options and isolated device runner.
- b6d21de platform: first-install rollback and acceptance instructions.
- a583ac3 docs: completed cleanup validation and review report.

Readability commits:
- 89354cc remove dormant scheduler branches and consolidate memory/audio/headless implementations.
- a60b2d4 apply clang-format/Black and commit the C style.
- a979686 descriptive renderer names, validation separation and warning fixes.
- 687f5dc reusable product audit, Python style and palette names.
- 4d7c1c7 remove stale legacy declarations and milestone log labels.
- 2a2d2f2 gate the three resampler telemetry additions to GLES3; stripped ARM executable unchanged.
- HEAD final documentation: this report, operating instructions and complete rename/removal mapping.

Source and composition:
- gles3/src/main.c contains only the GLES3/headless path: no desktop/legacy conditional branches,
  tex_bake perflog, RR_GL_DRAWALL, RR_TEST_VIEW, RR_POKE, RR_DUMP_* or RR_DEBUG.
- Desktop src/rr_main.c is exactly main after stable formatting; no GLES3 calls or branches.
- Deleted gles3/src/memory.c, audio.c and rr_headless.inc. Both builds use shared rr_mem.c/rr_audio.c;
  one src/rr_headless.inc uses platform hooks. RR_AUDIO_RESAMPLER_TELEMETRY gates GLES3 audio metrics.
- GLES3 no longer links disabled readable-replacement/checker modules; compatibility hooks preserve
  lifted execution and abort on unexpected checker activity. Desktop retains its existing modules.
- Removed unused HUD/trace/cache fields and helpers; exact projection/culling/WRAM/C352 paths remain.
- clang-format 23.1.3 and Black 26.10.1 applied; .clang-format/pyproject.toml commit the 120-column style.
- One statement per line; all scoped C/H/inc/Python/GLSL sources satisfy the 120-column bound.
- Hand-written GLES3/platform/test sources build with -Wall -Wextra, zero warnings, no suppression flags.
  Only generated rr_lifted.c, rr_c25.c and snd_driver.c retain -w; 68K/C25 O2 and sound O1 remain.
- READABILITY_RENAMES.csv lists 294 explicit identifier, log, filename and removal mappings.
  Examples: c1d_*→provenance_*, g1b_*→dynamic_cache_*, g1c_*→material_cache_*, g2_save_*→save_*;
  packet *3 types now Render*/Draw*; race_v3 shaders now world/world_near; historical semantic CSV renamed.

Product/test separation:
- rr_gles3_validation has a separate core and tests/shaders; GLES3_RECORD_DIAGNOSTICS is gone.
- Provenance/palette capture, command dumps, receipts, image hashes, fixed capture ticks,
  ENG_OBJLOG, PROPCYCL_CULLDUMP, batching diagnostics and forced-unsupported injection are validation only.
- Product link map/symbols/strings/compile flags contain none of those hooks; validation contains them.
  tests/audit_product.py passes on host and ARM and checks both link maps and target definitions.
- Strict unsupported logging, top summary line and exit 86 remain in the product; no fallback added.
- Product asset kits contain only product shaders; rr_validation_asset_kit supplies test shaders locally.

Source counts (C/H/inc/Python/GLSL; GLES3/platform plus seven shared runtime sources):
- Shared scope: src/rr_main.c, rr_mem.c, rr_audio.c, rr_headless.inc, rr_replay_session.inc,
  rr_overlay.c and rr_sim_profile.c.
- Files: 69 → 68; lines: 7,840 → 13,168; bytes: 470,664 → 499,592. Formatting expands whitespace/line counts.
- GLES3 main.c: 689 → 285 lines; 36,440 → 10,192 bytes. No generated source or fixtures in these counts.
- Stripped ARM product: 6,967,352 → 6,402,000 bytes versus the cleanup candidate.
- New rr_gles3 SHA256: 84f2a3970087ee3b56eddfae0d4310b8c38d953d6eb1da49c616322477c85700.

Final host acceptance — exact, no image/state tolerances:
- Reference 6006: 21 checkpoint sets/63 records, 4207 normalized+semantic receipts and 36 images unchanged.
- Cone 13808: 141 records, final state and 26 bonus images unchanged, including 11822–11828 and11936.
- Attract 36000: 360 records and 20 sampled images unchanged; zero unsupported/sound traps.
- Provenance 36 images: 5,529,600 ROM-decoded pixels, class(c)=0; boot 159 CPU-oracle pictures exact.
- Sound PCM/voices and checkpoint state equal interpreter on every tick, zero traps:
  reference 6006, attract 36000, cone 13808, fixture 1700 and dense1/2/3 each 1700.
- Record/replay 600: state, board and audio exact twice; overwrite/malformed/mismatch checks pass.
- CTest 4/4 passes; desktop rr builds/links. Shared-engine/other-game interfaces unchanged by this pass.
- Fresh host/ARM/desktop build directories and fresh ROM-compiled caches; all final build logs warning-free.
- Final binary evidence: /tmp/rr-readable-final-host/, /tmp/rr-readable-final-record-replay.log;
  /tmp/rr-readable-{host,arm}/gles3/*.map and audit_product.py. Earlier runs retained separately.

Device acceptance — one reference and one cone replay, isolated package, CPU 1512 MHz throughout:

| Section | Render CPU p99, baseline→new ms | Preparation p99, baseline→new ms | Pacing | Max draws |
|---|---|---|---|---|
| Reference race 4207 pictures |3.938→3.887|11.626→11.534|0/0/0|39|
| Cone 5/0, 1628 pictures |3.394→3.392|15.415→15.707|0/0/0|18|

- All four p99 changes are within ±0.3 ms; largest is cone preparation +0.291457 ms using raw baseline.
- Reference 6006/cone 13808 checkpoints exact. All sampled cores fixed 1512000 kHz; zero clock flags.
- Both journals restored clocks; final policies CPU schedutil/GPU simple_ondemand; EmulationStation restored.
- All 47 protected paths, three installed binary hashes and both existing Ports entries unchanged.
- Device budget 47.4 minutes including network interruptions; actual replay-driver span 6.52 minutes (<2 hours).
- Results: /userdata/system/rave-racer-gles3-readability-test-20261019/ and /tmp/rr-readable-device-results/.
- Local results tar SHA256: 9d9aa1193629a6e1e9255b218d86e29ed716139c890c58515bef351a26895915.

History/tree hygiene and follow-ups:
- 682 tracked files; 143 paths changed/121 added across history versus main; 17 local commits including this report.
- Full history/tree scan: zero violations, no new ROM payload/recording/benchmark path or file over 1 MiB.
- All 25 protected main artwork/font/BIOS paths unchanged. Inherited 69 large files/157 blob versions unchanged.
- All asset payloads, recordings, EEPROM snapshots, image bodies and packet bodies remain local, outside Git.
- Known follow-ups remain 51 unresolved sound indirect variants plus CB49, and the bonus compiler/walk tail.
- No further optimization, PR or installation. Build/deploy/rollback: KNULLI_H700.md; user review before installation.

Offline private archive, created before cleanup: /home/bwebb/rave-racer-archive/20261019-fdfd350/.
- feature-all-refs.bundle: d786f8ada93055a404214564a82424126d0ffcf7b5682f907601ce163185b9b9
- benchmarks.tar.gz: 5d14809a40df060828a4d06119f535dfa7bcf9acc20dd1d90fcae42a157866c1
- regressions-and-goldens.tar.gz: 6637c0d0020a6a609856da775c460499e3949507afe546b5acd6f58f32057da6
Bundle includes all 22 original refs. Keep ROM-derived archives private; never push their contents.
