# Focused Milestone 3 profiling evidence — 2026-10-15

Status: investigation complete, M3 BLOCKED. No accepted new backend.
Full explanation: [race report](../../NATIVE_RACE_REPLAY_REPORT.md).

Every log has 3840 consecutive original ticks and five separate warmups.
Chronological order: staged-full, staged-discard, staged-skip,
staged-cheap-fragment, reference-normal, staged-vertex-control.
No transfers/readbacks ran during any H700 lap. Stock governor, no clock changes.

Only `reference-normal.txt` is complete original one-fence rendering eligible
for the <=12 ms median / <=14 ms p95 headroom test; it fails. All other H700 logs
are controlled diagnostics. No-gather vertex control is CPU-prepared geometry
for attribution, not a persistent native asset backend. Its marginal medians
must not be mistaken for paired improvements; see paired-diagnostics.json.

`profile-summary.json`, `shader-reference-summary.json` and
`vertex-control-summary.json` expose serial wall spans and thread CPU costs.
`reference-completion.json` exposes complete-picture distributions and misses.
CPU spans include application and driver work; fences are not exclusive GPU timers.
The original source/renderer/HUD fidelity remains unchanged; incomplete controls
cannot satisfy fidelity or throughput gates. See source-validation.json,
integrity.json, vertex-control-visual.json and production-final.txt for scope.

Raw frame/asset files remain ignored in `native/generated/milestone3/`.
