# Fresh-install package and device cleanup

Branch: `tests/fresh-install`, from main `605d5e4`; no new package installed.

## Package

- `raverace/build/releases/rave-racer-knulli-h700-0.1.0.zip` (local build artifact).
- SHA256: `51c02f94fbaec95ebabaad6d68b79c87ea44bd8903b1984afdce30bd1bb55695`.
- Guide: [INSTALL.md](INSTALL.md), including all 36 required ROM names, CRC32s and SHA256s.
- ZIP has only `Rave Racer.sh` and `rave-racer/{bin,launcher,roms,cache,saves,logs}` plus documentation.
- ROM/cache/save/log folders ship empty; no ROMs, plans, decoded assets, EEPROMs, recordings or goldens.
- Python standard library plus the shipped C codec; no NumPy, pip or device downloads.
- Product ELF: `193c620ad9a61c636e9f8fb080af72e84efba2f577c92b6dd3feada9a7dc3ce6`.
- Product C change is diagnostic gating only; quiet/full state and pixels match exactly.
- Dedicated setup binary enables blank EEPROM export without linking validation/provenance hooks into the game.

## Compiler and first launch

- C streams pen rows and geometry, hashes/compares deduplicated contents, merges and packs exact rectangles.
- Every output file matches the host NumPy compiler: 32/32 SHA256s, host and ARM/device.
- Hash-only receipt: `gles3/tests/fixtures/asset-compiler.sha256`.
- Historical `sizing.json` elapsed-time field is canonical zero in both compilers; actual timing goes to logs.
- Versus the pre-change host output, 30/32 common files are unchanged; only timing and compiler-source
  identities differ in sizing.json/recipe.json. There is no tolerance or asset-data normalization.
- No runtime asset bytes changed. Shared metadata writer keeps both compilers' formats identical.
- Device ES PID 3064 stayed running; CPU fixed by unchanged launcher policy at 1,512 MHz.
- Initial compiler run: 193.437 s, peak 148388 KiB (144.91 MiB).
- Second empty-cache run: 162.377 s, peak 148220 KiB (144.75 MiB).
- Second build including all-output verification and flushes: 174.885 s, below five minutes (also 205.740 s in the cleared-device cold check).
- Automatic TEST/SERVICE calibration: 90.953 s; EEPROM exactly `81e19e9c…73721c7`.
- Full scratch flow completed, then 1800 interactive ticks, zero traps, exit 0; total 321.527 s including play.
- Final package cache-hit run: 54.099 s including 1800 ticks; exit 0, original governors restored.
- Explicit full-telemetry scratch option also passed 300 ticks; exit 0, governors restored.
- No Ports entry was registered for these tests; all test files were in a scratch folder.
- Host cold success (off-screen): assets 13.788 s, calibration 7.233 s, start + 60 ticks 0.638 s;
  total 21.786 s, generated EEPROM hash exact.
- Host streaming build: 14.027 s. Final cancellation/restart/cache-integrity/calibration fixture: 35.365 s total,
  including a 13.681 s final build. ROM check measured 0.027 s on the host.

- Cold check after the old installation was removed: compiler 201.896 s, 148400 KiB RSS;
  verified/flushed build 205.740 s; ROM check 0.187 s; calibration 90.273 s.
- Cleared-device full flow plus 600 game ticks: 315.873 s, exit 0, zero traps, stock governors restored.
- Latest calibrated EEPROM again matches the pinned 81e19e9c…73721c7 hash.

## Crash safety and normal play

- Work goes to `.building`; all files are SHA-verified and flushed before rename publishes the generation.
- Incomplete generations are discarded on the next launch. Cancellation during decode and verification, and damaged-cache tests passed.
- Parent-death guard terminates setup displays, compilers and games; independent session watchdog restores original governors.
- Stage and percentage appear on screen; START+SELECT cancels. Failures name the stage; no alternate method.
- First EEPROM is generated from ROM TEST/SERVICE inputs, SHA-checked, flushed and atomically published.
- Default mapping is the verified RG40XX H built-in configuration; personal override stays inside saves/.
- Normal mode omits thread profiles, per-picture renderer diagnostics and resource sampling. Full telemetry requires `--telemetry full`.
- Final supervisor CPU: 0.431% mean, 0.999% max of one core (51.058 s, separate 1-Hz observer).
- Final log growth: 12368 B/min (12.08 KiB/min); play log is capped at 64 KiB plus one rotation.

## Validation

- Final binary: reference 6006, fixture 1700, attract 36000 and full-race 13937 ticks exact.
- Reference: 21 checkpoint sets; adjusted commands/semantic receipts and sampled images unchanged.
- Full race: all 141 records/final state, 10674 race and 1543 bonus pictures, commands and images exact.
- Sound translated == interpreter every tick: reference, fixture, attract and full race; zero traps.
- 36-image ROM provenance class(c)=0; boot oracle, unsupported exit86, record/replay and all 6 CTests pass.
- Package layout/empty folders, missing/wrong ROM failures, quiet/full equivalence and product link audit pass.
- Full-race sound gate now explicitly requests diagnostics for its unchanged coverage-count assertion.
- All-output SHA manifests match between host oracle, host streaming compiler and device compiler.
- Repackaging the same build produces the identical ZIP SHA256.

## Personal backup and approved wipe

- Only 36 ROM files, personal EEPROM and controller mapping were copied off (38 files, 32541018 bytes).
- PC backup: `/home/bwebb/rave-racer-device-backup-20261021/`; every SHA256 matches the device.
- Personal EEPROM: `d783d26064be0efe884f344c58e8be80d3eb71802578df13cbb1a15b415bc9a4`.
- Mapping: `355aef433cc4a6d11381b1b218319e35978f5b23f1396a0ccfef0049a94e3284`.
- This is the latest personal save after the hands-on run, not a generated test seed.

- Deleted all 62 approved inventory/scratch paths, including current install, ROMs, saves and rollbacks.
- After cold validation on the cleared device, removed its scratch folder, ZIP and helper files too.
- Recursive /userdata + /tmp scan: no Rave Racer content remains; ten lexical matches are unrelated.
- Kept PortMaster cover art for bravedogsroad/hargrave/kleebuucravesfruitsalad/ravenshike/
  redtrianglesc/rrootage/undergrave, fbneo rranger/rrangerb ROMs and the GB Frommer travel-guide ROM.
- Only Ports scripts: PortMaster.sh and Ship of Harkinian.sh; no Rave Racer gamelist entry remains.
- ES running (PID 18296); all four CPU governors schedutil, 480000–1512000 kHz bounds.
- GPU simple_ondemand, min/max 0/0. All 15 original governor/bound values restored and unchanged.
- Free space: 55477796864 → 74099064832 bytes (51.67 → 69.01 GiB), reclaimed 17.34 GiB.
- Latest final guarded-launch scratch check passed 60 ticks, exit 0; no new Ports entry installed.

## Repository

- Implementation: `10e8d29`; cancellation/process safety: `d3d5c7c`; report and architecture notes follow.
- Verified complete feature bundle and matching `fdfd35014f609d8ffddd5b9a18b53d1a873d7e24` ref.
- Bundle: `~/rave-racer-archive/20261019-fdfd350/feature-all-refs.bundle`.
- Deleted remote `cleanup/gles3-main`, `tests/full-race`, `feature/knulli-h700`; confirmed absent.
- Remote main remains `605d5e4cc318186aef9d19166e07da7940da59c1`.
- New history scan: source/hash text only, no payload/recording/benchmark paths, no blob over 1 MiB.
- Release ZIP is ignored and is not committed. No ROM-derived payload was committed or shipped.
