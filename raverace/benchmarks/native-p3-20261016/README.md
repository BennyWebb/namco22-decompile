# P3 evidence — COMPLETE / performance gate FAIL / STOPPED

[Report](../../NATIVE_P3_REPORT.md) contains the result and unchanged exit gate.
Continuation starts at `ea0e2ea6d7e83ec44d84dd197759fc8a6cdd9762` on
`feature/knulli-h700`. No commits, pushes or production deployment.

Selected: P2 b + B1 + B2, default priorities. Dense preparation p99 is
13.705/13.492 ms; final-2 has one inferred repeat at tick 1800. Both final runs
have zero settled audio underruns and main gaps >50 ms. The seven-lap soak is
clean. This is a failed performance gate, not a waived transition criterion.

## Current evidence index

- `resume-method.json`, `resume-setup-notes.txt`: scope, smoke gates and setup fixes.
- `resume-results.json`, `resume-service-timings.csv`: all stage distributions.
- `resume-selection.json`, `resume-gate.json`: priority/step selection and gate.
- `paired/resume-*.{csv,json}`, `paired/r-a*.{csv,json}`: signed per-tick comparisons.
- `resume-build-identities.json`: exact diagnostic binaries and build commands.
- `resume-full-replay-audit.json`: all 13 completed continuation replays, including
  seven soak laps. Exactly one device validation proves all 21 sets, 4,207 packets
  and 36 Mali images; play timings exclude packet/image hashing.
- `runs/r-smoke-*`, `runs/r-b*-timing-smoke`: four tick-1900 smokes and actual
  thread policy receipts. `runs/r-a1`, `runs/r-a2`: corrected priority timings.
- `runs/r-b1-timing`, `runs/r-b2-timing`, `runs/r-final-2`: step/final timings;
  B2 timing is final-1, counted once. `runs/r-validation-final`: separate validation.
- `runs/r-final-2/handover-event.json`: tick-1800 repeat and stage attribution.
- `runs/r-soak-1512000`: 929.43 s wall time, seven full process laps, 493.18 s
  native exposure; per-lap and five-minute buckets, clocks, census and observer.
- `resume-manual-observation.json`: user answered “Yes” to clean settled motion/audio.
- `resume-restoration-summary.json`, `runs/resume-final-check`,
  `resume-completion.json`: all 47 protected hashes, clock/RT restoration,
  EmulationStation restored, no game running, 48.94-minute continuation budget.
- `telemetry-manifest.json`: every published raw artifact and lossless source hash;
  large text is gzip-compressed. No ROM, assets or executable published.

## Measurement

Full reference replay 6006 ticks; native 1800–6006, dense 3550–3590 inclusive.
Nearest-rank p95/p99; all samples and failed attempts retained. Dense has 41
samples, so its p99 is the maximum. Inferred repeats use the unchanged P1b
rounded swap-return interval method, panel period 1000/59.7104 ms. They are not
physical scanout measurements; snapshot-to-swap is not scanout age.
Underruns are sample frames, reported separately over native and actual legacy
release completion through 6006. Validation and smoke timings are excluded.

CPU fixed 1512 MHz, GPU stock, queue 2, valve OFF, affinity OFF, O2 and stack
protection. Cooldown ≤55°C or 180 seconds idle. Per-run EXIT traps restore clocks,
RT defaults and EmulationStation and verify protected hashes. Clock/thermal
sampling nominal 10 Hz, census 1 Hz; soak observer follows the game PID. No game
output to RAM-backed /tmp and no concurrent transfer/archive during timing.
Corrected A1/A2 each run once; B1 and B2 each once; final gets two timings total.
Soak is a process-loop including legacy startup, not a single-process reset.

## Retained blocked history

`blocked-attempt-report.md` preserves the initial stopped report. Unprefixed
`run-results.json`, `completion.json`, `priority-blocker.json`, initial `a1/a2`
and `runs/final-check` belong to that attempt: RR_* was stripped and priorities
never applied. They are invalid scheduling comparisons, retained rather than
replaced. Current continuation evidence uses `resume-*` and `runs/r-*` names.
`control` is the original valid baseline, reused without another control run.
The smoke checker terminal-1900 versus reference-2100 setup comparison was fixed;
18 common periodic domain records and equivalent smoke terminals pass.

Host proof remains in `projection-test.txt`, `cull-differential-test.txt`,
`host-parity.json` and `runs/host-*`: 128449792 UBSan projection comparisons,
one million cull comparisons, full host checkpoint/packet/image parity.
Host software-GL images are distinct from final device Mali validation.
Private reproduction helpers: [native/p3](../../native/p3/README.md).
Production unchanged; clocks and EmulationStation restored. STOPPED.
