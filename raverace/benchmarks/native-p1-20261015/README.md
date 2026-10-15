# P1 hybrid pipeline evidence — final

Investigation complete under the user-reduced scope; **performance gate FAIL**.
See [P1 report](../../NATIVE_P1_PIPELINE_REPORT.md). No P2, commit, push or production
replacement. Starting clean HEAD is recorded in repository-before.json; current
branch remains feature/knulli-h700 at c98d2b20f5e946a0e45e98125332215332fb0c12.

The user accepted hybrid legacy/native rendering, presentation-paced simulation
at the unchanged measured59.7104Hz panel rate, O2 generated sources (O1 switch,
stack protection retained), opt-in clocks and affinity OFF. The user subsequently
deferred both30-minute thermal soaks and reduced repeat rounds: **one unfenced and
one separate fence-diagnostic replay per stock/both-clock × queue2/3**. Eight runs
completed, all6006 ticks, all21 authoritative checkpoint sets, zero tick drops.
No repetition confidence or sustained thermal pass is claimed.

## Files and retained populations

- `reference-1-*-timed/`: four normal FIFO/vsync runs; use these for paced results.
- `reference-1-*-fence/`: four separate glFinish-instrumented attribution runs.
  Never pool their timings/repeats with normal results.
- Each run retains `thread.csv.gz` (all6006 pictures), `ticks.csv.gz`, `run.txt.gz`,
  `memory.txt.gz`, `states.jsonl`, exit/method/preflight, clocks and production
  before/after, ES restoration, `p1-summary.json`, and `stalls.json.gz` containing
  every main progress gap >50ms and its VM/I/O/PSI/thread evidence.
- `all-run-metrics.csv`, `measurement-counts.json`: service/pacing/memory summaries,
  showing exactly one run per configuration/mode. No outlier exclusion.
- `all-main-progress-stalls.csv`: all main publication gaps >50ms, including boot.
- `observer-operation-analysis.json`: largest observer operation delays and
  periodic-stall association. Complete operation records remain in raw memory logs.
- `run-validation.json`: every timing run's ordering/checkpoint/restoration checks.
- `validation-q2/`, `validation-q3/`: separate capture/hash correctness runs;
  all4207 native packet receipts and all21 checkpoint sets identical;36 sampled
  Mali world/composed RGBA comparisons per queue match the prior validated Mali
  renderer. Their timings are **not** normal performance evidence.
- `host-*`: host correctness results, all21 checkpoint sets /4207 packets where
  applicable; forced missing-mesh fixture at2501 has exactly one fallback and4206
  other packets identical,36 image matches. Host offscreen runs are unpaced.
- `observer-validation.json`, `observer-manual.json`: verbatim user observations.
  City racing/all controls worked, no corruption; audio stuttered on frame drops
  and several long stalls. All9806 unsupported pictures rendered through legacy;
  this does not establish native City asset coverage.
- `build-logs/`: retained build attempts, including failures. Earlier
  `host-build-attempt-*` copies are also retained. The initial `RR_P1_*` fixture
  was cleared by replay startup and did not test its intended switch; the
  replacement `ENG_P1_*` fixture is the accepted result. Original ignored runs
  remain available; successful-but-unexercised fixture is not a claimed pass.
- `matrix-history.txt`, `matrix-status.txt`, final integrity/shutdown records:
  eight runs completed, scheduler stopped before round2, clocks restored,
  ES running, no game/sampler,47 protected files OK. The redundant later SSH
  recheck lost connection; `final-device-verification.txt` records that limitation.
- `archive-sha256.json`: hashes of all12 locally retained hardware archives.
  The failed later SSH attempt did not alter completed test evidence.

All original hardware archives and expanded raw evidence are retained locally at
`raverace/native/generated/p1/` and `raverace/native/generated/p1/hardware/`.
ROM-derived command bodies and RGBA captures remain there, outside tracked evidence.
Packet hashes and image-comparison results are included here. Original remote
outputs remain under `/userdata/system/rave-racer-native-p1-20261015/`.

## Timing and observer method

Reference `/userdata/system/rave-racer-replays/race-20261013-122242-25136.rec`;
full6006 ticks; analysis1800–6006 inclusive,4207 samples. Nearest-rank p95/p99;
ordinary median. All samples, transition stalls, failed attempts and observer gaps
retained. Normal order stock-q2, both-q3, stock-q3, both-q2, each followed by a
separate fence run. Both clock means CPU performance and GPU min=max696MHz;
stock GPU simple_ondemand/min0/max0 and CPU schedutil. Governors restored after
every run. No transfers/archival during timed games; persistent archives between
runs. Preflight memory, /tmp and mount evidence is per run: /tmp5364KiB throughout.
All test output is persistent /userdata, with a private working directory.

Simulation wall excludes explicit FIFO-blocking time but includes main scheduler,
checkpoint and logging costs. Preparation is measured separately from GL-thread
CPU/draw/swap. Fenced world+HUD completion and final wait use the existing M3
one-fence convention; those runs disturb overlap and cadence. Audio counter units
are missing stereo output sample frames (48kHz), not callback counts. Native
underruns are counter6006 minus1799; shutdown totals include boot and drain.

Swap-return timestamps are the presentation proxy. Inferred repeats use
`max(0,floor(interval/(1000/59.7104)+0.5)-1)`, every interval retained. This is not
an independent hardware-vblank counter. Snapshot-to-swap age is a scanout proxy,
not an exact physical scanout age. Queue histograms sample publication occupancy,
not continuous time occupancy. The old16.693ms counts remain in summaries but are
not panel-deadline counts because the accepted panel period is~16.7475ms.

`memory_sampler.c` is one long-lived20Hz process, no shell forks. It records
RSS/HWM, MemAvailable/CmaFree, Mali global allocated pages, GPU frequency/governor/
transition counters, all four CPU frequencies, thermal/cooling, VM fault/reclaim/
allocstall counters, PSI, process I/O/diskstats, thread state/wchan and D stacks.
Actual sampling is **not continuously ≥10Hz**: median~50ms, worst gap3.803s.
Pressure-file reads and output flushes have slow-operation records and voluntary/
involuntary switch deltas. Catch-up samples do not fill missing observations;
coarse sample-start timestamps cannot place every field precisely within a stall.
Thus the old2.74s D-state race stall remains unproven causally. Captured main
filemap_fault stacks do identify some boot/code-loading waits. No speculative fix.

Mali page counts are global, not a separate process memory charge; do not add
RSS plus GPU pages. Transition windows are publication−2.0..−0.1s and+2..+5s.
Temperature/clock samples in short runs do not substitute for deferred soaks.
Inherited `queue=latest`, arcade target_hz and extra_hud_bytes log strings/formulas
are not assertions of actual FIFO scheduling or CPU RGBA allocation; consult the
private source and ordered per-picture data.

## Reproduce the completed analysis, without hardware runs

Private build (requires existing original build tree and validated Stage1b objects):

```sh
python3 raverace/native/p1/build.py /tmp/rr-m3-reference-gl host
python3 raverace/native/p1/build.py raverace/build/playable-knulli arm
# Add --o1 to select original O1 generated objects; no production files replaced.
```

On the retained expanded local evidence:

```sh
python3 raverace/native/p1/analyse.py raverace/native/generated/p1/hardware/reference-1-both-q2-timed
python3 raverace/native/p1/summarise.py raverace/native/generated/p1/hardware
python3 raverace/native/p1/publish_evidence.py
```

To reconstruct from this directory's compressed telemetry, copy a run directory
to an ignored analysis directory, decompress thread.csv.gz, run.txt.gz and
memory.txt.gz there, then pass it to analyse.py. Keep states.jsonl so the analysis
uses the replay window rather than treating it as free play. `stalls.json.gz`
already retains every observed >50ms gap. No hardware rerun is needed.

Per-queue ARM correctness:

```sh
python3 raverace/native/audit/stage1b/validate_flags.py \
  raverace/native/generated/p1/hardware/validation-q2 \
  raverace/native/generated/p1/hardware/validation-q3/states.jsonl \
  raverace/native/generated/milestone4a-coverage/commands
```

`native/p1/run_h700.sh` and `matrix_h700.sh` document the isolated test method,
not authorisation to run more tests. Local matrix helper now defaults to one
round, matching the user's amendment; the original three-round remote script is
retained in ignored final-metadata.tgz. Further measurements/soaks need approval.

**STOPPED at P1. Zero-repeat/clean-audio gate failed. No production replacement,
compiler algorithm/texture-format optimisation, P2, commit or push.**
