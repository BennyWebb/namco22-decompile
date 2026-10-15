# Stage 1b environment and build-flag headroom audit — 2026-10-15

Scope: measurement only on `feature/knulli-h700`; no Stage 2/M4B or architecture,
compiler algorithm, shader, simulation, audio or production changes. Generated-code
flag variants and default-OFF affinity/logging diagnostics are explicitly authorised.

Evidence is accumulated here without dropping failed runs. Reference replay is the
original 6006-tick `race-20261013-122242-25136.rec`; analysis window 1800–6006.
Nearest-rank p95/p99; every sample retained. GPU/CPU/temperature/task telemetry is
one persistent C process at 20Hz with absolute monotonic deadlines and no forks.
Raw devfreq transition tables plus numeric counter deltas retain driver units;
frequency-label entries have zero deltas. Missing sysfs values are explicit.
Sampler writes buffered telemetry; no transfers or archival overlap timed runs.

Hardware runner `native/audit/stage1b/run.sh` records original GPU governor/min/max
and per-core CPU governors, verifies preflight MemAvailable and/tmp, and installs
an exit trap restoring clocks and EmulationStation after every run. Governor runs
use the unchanged Stage 1 control and exact production diagnostic copy. GPU exposes
only `simple_ondemand`, so the allowed maximum-frequency clamp is used instead of
an unavailable performance/userspace governor; original min/max are restored.

Initial repository state: clean, branch `feature/knulli-h700`.
Initial SSH timeout retained as a connectivity failure before any device change;
user confirmed reconnection. Initial connected sysfs read: GPU240MHz,
`simple_ondemand`, available frequencies 240/336/420/504/600/696MHz; min/max 0/0.
CPU `schedutil`,480–1512MHz; device 44°C at login. This idle reading is not evidence
of in-race frequency behaviour.

Status: COMPLETE / STOPPED. Measurements, limitations and restoration proof are
in [NATIVE_STAGE1B_HEADROOM_REPORT.md](../../NATIVE_STAGE1B_HEADROOM_REPORT.md).

The unchanged governor-run recipe is preserved as `run-B.sh` with its hash.
Later opt-in affinity/flag modes extend the source runner only after B finishes;
B runs do not preload any library. The affinity preload has no new workers or
queue changes. Enabled children restore their creator's ORIGINAL allowed mask,
preventing accidental inheritance by audio/driver helpers; only explicitly named
main/prep/present roles receive pins. Unset forwarding preserves original thread
start routines/arguments and makes zero affinity syscalls in the standalone trace.


User scope amendment during E: **one full replay per flag configuration**;
repeat rounds 2/3 skipped. B retains three runs/configuration/executable and D
retains two counterbalanced pairs/configuration/executable. E does not establish
flag-performance repeatability. Its host and ARM correctness validations remain
complete for every variant and the unchanged ARM control.

The command receipt helper is validation-only: original dump packets pass through
stdio SHA256 cookies and retain byte lengths/hashes without packet files on the
handheld. Host validations compare every packet against the established reference;
ARM receipts match that same reference exactly. Neither packet dumps nor this
helper are active in performance runs. ROM-derived packets/executables remain in
ignored native/generated paths, not this evidence directory.

B uses the preserved clock_sampler-B.c. Later C/D/E add cooling-device states,
CPU-policy maximum and sampler CPU accounting. All game runs use a 20Hz sampler;
standalone helper development exceptions and corrected/failed attempts are named
in diagnostic-development-notes.md. No failed run is relabelled as a pass.

Analysis commands (from repository root):

```sh
python3 raverace/native/audit/stage1b/analyse.py <run-directory>
python3 raverace/native/audit/model_pipeline.py <native-performance-run-directory>
python3 raverace/native/audit/stage1b/collect_metrics.py raverace/benchmarks/native-stage1b-20261015
python3 raverace/native/audit/stage1b/collect_models.py raverace/benchmarks/native-stage1b-20261015
python3 raverace/native/audit/stage1b/verify_runs.py raverace/benchmarks/native-stage1b-20261015 raverace/native/generated/milestone3/inputs/race-20261013-122242-25136.rec.states.jsonl
```

The original model method is unchanged, including its fixed three-period latency,
three-slot FIFO and assumed 0.1ms handoffs. It targets 59.9040599Hz software pacing;
the measured 59.7104Hz panel means its results cannot certify physical no-drop
scanout. Model timings never include correctness-only hashing runs.

Optionalperf build failure is F-perf-build.txt/F-perf-status.json. No profiler was
installed or run; no missing flame graphs/counters are represented as measured.
PGO was not attempted: no separately validated non-reference training replay was
introduced into this bounded audit.


Completed populations:50 isolated H700 game runs (24B governor,16D affinity,
2B logging supplements,4E correctness,4E timing), all 6006 ticks/all 21 checkpoint
sets/exit 0. Three host flag validations pass; a separate first host harness
shutdown failure remains retained. Two panel probes each retain 3001 returns.
Every game run verifies restored original clocks, matching production hash and
EmulationStation readiness. Final 47 protected hashes and no-game process listing
are device-meta/final-integrity.txt. M3 remains blocked; Stage 2/M4B not started.

Clock logs are `clocks.txt.gz`, compressed losslessly only after all testing.
clock-raw-sha 256.json records original byte lengths and decompressedSHA256;
round trips are verified. Every sample is retained. all-run-metrics.csv includes
separate run_class labels; correctness/helper timings are not performance data.
pipeline-model-comparison.csv includes only native performance runs. Per-run
pipeline-model.json uses the original unmodified model method.

Affinity usage is diagnostic-only:
`LD_PRELOAD=<diagnostic-dir>/affinity.so RR_AFFINITY=main:0,prep:1,present:2 <diagnostic-executable> ...`.
Unset/empty RR_AFFINITY makes zero affinity syscalls; do not preload into the
sampler or EmulationStation. Production launchers/configuration were not edited.
The shutdown logging copy requires opt-in ENG_LIVE_SIM_TELEMETRY=<csv>; unset
retains existing report behaviour and does not export CSV. Hot-path opcode hashes
are production-logging-build.json. No timing result comes from hash-enabled runs.

Final recommendation and thermal caveat: generated-source O2 is useful for a future
prototype, affinity OFF, explicit diagnostic clock switches. `power_allocator`
switches on at 60°C and targets 70°C; nonzero GPU cooling states are observed in
stock production workloads even below 70°C. Do not infer no throttling from peak
temperature alone. No production setting or thermal/display policy was changed.
