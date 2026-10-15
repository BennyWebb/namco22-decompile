# Controlled Milestone4A H700 validation — 2026-10-15

Read `../../NATIVE_LIVE_INTEGRATION_REPORT.md` for current outcome/gates. M3 is
performance-blocked and4B is not authorised. This directory retains every collected
sample, including failures and environmental interference; no outlier filtering.

Device diagnostics only: `/userdata/system/rave-racer-native-live-4a-20261015`.
Production launcher/executable/settings/saves/replays are not deployment targets.
Eight validated old atlas pages copied within diagnostic storage; complete pack,
lookup, shaders and prepared binary checked against SHA256SUMS before running.
All input replay sidecars copied into diagnostics. ROM source is read-only under
the existing game directory; headless prototype does not configure persistent
EEPROM saving. Production baseline is an exact copied binary, not its launcher.

Methods:
- `native/tools/run_live_h700.sh NAME EXECUTABLE on on|off paced` runs the original
 6006-tick replay, all authoritative checkpoints, original audio mixing/output,
 serial/coupled scheduling and original controller map. It restores EmulationStation
 after each run. Per-run logs/checkpoints/timestamps live in RAM then are copied to
 diagnostic storage on exit. Native renderer retains one final glFinish; component
 fences, GPU HUD verification, command-file dumps and capture readbacks are off
 in timing runs. First native frame and every slow sample remain included.
- `instrument_live_hardware.py` privately relinks only bounded telemetry at the
 presentation call sites and exports the existing simulation profiler's RAM rows.
 No simulation/game, command compiler, shader, audio or pacing algorithm changes.
 Shutdown CSV emission is idempotent in v2. Debug data stripped from the v2 transfer
 to reduce size; the unstripped binary/symbols remain local. This is not optimization.
- `live_memory_sampler.c` polls RSS/status, Mali allocated pages, MemAvailable,
 CmaFree, temperature and CPU frequency once per second in one persistent process.
 Initial failed test/baseline used a fork-based shell sampler; disclosed in
 INVESTIGATION.md. CPU governor/clock settings and swap are unchanged.
- `analyse_live_hardware.py RUN` validates complete tick sequences and generates
 native-frame CSV, raw memory JSON, deadline flags and summaries. Median is standard;
 p95/p99 nearest-rank. Frame intervals use actual SDL swap-return timestamps for
 telemetry builds, completed original replay-tick timestamps for exact baseline.
 These endpoints differ slightly; they are labelled separately. They are not
 display scanout or photon timestamps. First interval has no preceding timestamp;
6005 intervals cover6006 presented ticks. Native phase includes4207 intervals,
 including1799→1800. `FRAME interval_ms` first1800 has no prior native completion;
 use presentation timestamps for the transition instead.
- World/HUD completion is CPU wall time to one final glFinish, including submission
 and GPU/driver wait. `finish_ms` is CPU fence wait, not isolated GPU execution.
 CPU clock measures the renderer thread's active time. Separate world and HUD
 submission measurements overlap GPU work and must not be called standalone GPU
 durations. The serial bridge offers no across-frame simulation/GPU overlap.
- Requested native static ceiling remains512MiB. Whole process includes retained
 legacy textures, HUD/dynamic buffers, driver allocations, source ROMs and CPU state.
 RSS includes Mali mappings on this unified-memory device; do not add RSS+GPU bytes.
 Mali debug counters are allocated pages at4096 bytes/page. Samples are1Hz and can
 miss shorter peaks; VmHWM/kernelOOM records provide additional evidence.

Run matrix:
- `validation`: exact prepared ARM build; OOM before native1800, exit137. Raw kernel
 logs/memory retained; buffered per-tick data was lost on SIGKILL. Sole persisted
 simulation checkpoint300 matches; full board/audio triplet absent. Not a pass.
- `baseline`: exact production copy,6006 ticks, audio on. Overlapped verified archival
 of~192MiB old M2 tmpfs outputs; worst stalls retained, not used as clean comparison.
- `validation-clean`: exact prepared build,6006 ticks,4207 native frames,18 world/HUD
 capture pairs. Checkpoints pass. Captures deliberately add stalls; not headline timing.
- `timing`:6006 ticks,4207 native pictures; all samples/checkpoints complete. Exit2
 after second cleanup tried to recreate telemetry CSV. No in-race failure; all data
 retained and a separate v2 repeat checks corrected shutdown.
- `timing-repeat`: v2 no-capture repeat with no concurrent bulk transfer/archival.
- `baseline-clean`: clean-environment production-copy comparison using same coupled,
 serial, audio-on policy and persistent sampler. Baseline has no native final fence;
 compare full tick intervals, not falsely matched GPU completion.
- `controls`: interactive original boot/menu path limited to1500 ticks. Cannot
 establish native racing controls, because1800 activation is not a state detector.

ROM-derived screenshots stay ignored under
`raverace/native/generated/milestone4a-hardware/results/validation-clean/`.
Review PNGs: `review/tick-T-host-left-h700-right.png`. Raw reference/source HUD bytes
come from prior validated host captures; every hardware HUD readback is independently
checked with `check_native_hud_pixels.py --live-states .../original-captures`.
Small cross-GPU world raster differences are disclosed in visual-comparison.json.
The original M3 window's byte command parity is prior host evidence, not rerun
command-dump I/O during hardware performance testing. H700 native command counts
and original checkpoint hashes still verify complete replay accounting.

`stale-m2-tmp-archive` on device preserves all nine old M2 trees and their SHA256
manifests/check results. Only after successful copy/hash verification were the old
RAM copies removed. No user save/configuration/replay was removed. No system
memory-limit, swap, clock/governor or renderer residency change was made.

Full reproduction: export current prepared/expanded pack as in the host-coverage
README, stage it in the separate device directory, verify SHA256SUMS, cross-build
`live_memory_sampler.c`, generate telemetry build with `instrument_live_hardware.py`,
then invoke the diagnostic runners. `INVESTIGATION.md` records binary identities,
failed startup/shutdown evidence, environmental changes and measurement limits.
