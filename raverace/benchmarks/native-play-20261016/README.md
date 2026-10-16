# First real Native play session — read-only retrieval, 2026-10-16

Repository `feature/knulli-h700`, HEAD `07aa9ecc81fa851ebdf49498728cd78b9b067325`.
User reports all four courses looked, felt and sounded perfect. This observation
is recorded alongside the telemetry; neither is substituted for the other.

Device session inventory:
- `20261016-223205-25904`: G2 setup-failed smoke, finished22:35:43 BST; excluded.
- `20261016-223653-26445-804047685`: corrected G2 scripted smoke, finished22:38:57;
  excluded (already retained under G2 evidence).
- `20261016-225025-27784-297185128`: **only subsequent/user session**,
  started22:50:25 BST, summary finished23:04:07, ~13m42s including startup/exit.
  Method has interactive mode with no replay or smoke injection. Pipeline
  `[THREAD-PERIOD]` interval818.527370s; session directory and FAT mtimes resolve
  overall bounds to approximately1s. State/course times are counts/59.7104Hz
  estimates, not exact reconstructed wall-time dwell.

Only this session and latest-summary.txt/json were copied, plus text clock-journal
and metadata/hash evidence. Session files are text/JSON/CSV telemetry, including
rotated `.log.1`; no EEPROM contents, images, ROMs, packs or caches were copied.
`latest-summary.*` are byte-identical to the session's summaries. SHA256 receipts
are in `retrieved-sha256.json`, with remote comparison in `production-integrity.json`.

`analyse.py` reads local copies only. `state-course.csv` contains every one of59
nonempty state groups (main/sub/screen/major course), counts, nominal seconds,
prep p99 and events. `state-course-grouped.csv` merges menu screen IDs for the
concise report. Raw course selectors0–3 are retained; no unsupported mapping to
course names is asserted. Preparation p99 is the session's nearest-rank upper
0.01ms histogram bin, not a new percentile computed from incomplete raw logs.

`events.json` preserves all16 anomaly events with ticks/states/service times;
`problems.log` also retains all16. For ticks41123–48876, retained profile CSV
provides exact monotonic swap timestamps; early repeat times are explicitly
approximate tick/59.7104 timestamps because old raw records rotated out.
Elapsed exact times use THREAD-PERIOD start52421.810237677 as origin.
No events or samples were excluded. Six repeat events infer7repeated vblanks,
including one boot/prewarm repeat and one2-repeat event. This is swap-spacing
inference, not a hardware vblank counter.

`event-resource-context.json` gives nearest retained1Hz resource records around
late events; live sampler was10Hz and summary extrema retain all8203 samples.
Those nearby samples cannot prove the cause of a subsecond scheduling/driver gap.
Core profiling capped its retained detailed records at8192: THREAD overflow40684,
SIM overflow40685 (one more simulated tick than presented on exit). These are
bounded diagnostic-array saturation, distinct from asynchronous transport drops0,
incomplete joins0 and exact-key overflow0; full session summaries remain present.

All handheld commands were reads or copies off-device. No game launch, clock or
service action, installation, removal or remote archive/file creation occurred.
`check_integrity.py` performs read-only production/protected-path/clock checks;
it writes its JSON only on the host. Production digest must match G2 and all47
protected paths must pass. No commit/push and no implementation changes.

Final read-only check23:11:38 BST passed:production SHA256
`6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`,
all47protected paths,all copied-file hashes,current clocks equal saved originals,
EmulationStation PID26586,no native game. No further device actions followed.
