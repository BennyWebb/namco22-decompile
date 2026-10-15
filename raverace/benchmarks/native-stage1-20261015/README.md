# Stage 1 architecture and CPU audit evidence — 2026-10-15

Completed investigation only. Read the [feasibility report](../../NATIVE_ARCHITECTURE_FEASIBILITY_REPORT.md)
for definitions, tables, limitations and the GO recommendation. This is not
implementation authorisation or a claim of consistent59.904 FPS. M3 remains blocked.

## Runs and retained evidence

- `detailed/`: isolated serial native diagnostic,14 compiler buckets and same-frame
  original GLES world-preparation comparator.4207 audit rows,6006 simulation and
  presentation rows; original/native execution order alternates by tick.
- `control/`: same binary/comparator with fine probes disabled. Use these whole-stage
  timings for feasibility. `pipeline-model.json` retains every sensitivity scenario;
  its inputs are joint per-tick samples, not independently shuffled percentiles.
- `production/`: exact diagnostic copy of production executable, existing independent
  three-stage mode with built-in thread/synchronisation/simulation profiling.
  `thread.csv` retains5404 presented records;3800 are in ticks1800–6006. Original
  simulation profiler exports aggregate statistics, not raw simulation samples:
  its simulation p99 cannot be recovered and is not invented.
- Each run retains `run.txt`, checkpoint records/validation, replay ticks, exit status,
  `/proc` samples, per-thread CPU/core observations, memory and derived distributions.
  All three runs exit0 and pass21 authoritative checkpoint sets through6006.
- `host-validation/`: complete host replay/checkpoints and M3 parity reproduction,
  plus all4207 native command packets identical to the pre-audit full-replay packets
  without normalisation. Generated packet files remain in ignored local assets.
- `matched-threaded-comparison.json`: same-tick original shadow versus actual worker
  comparison; `recipe-redundancy.json`: selected-frame repeated lookup evidence.
- `integrity-before.txt`, `integrity-after.txt`, `integrity-validation.json`: all47
  protected production/ROM/config/replay hashes match; EmulationStation restored,
  no profiling game remains; governor unchanged. `INVESTIGATION.md` records method
  and the harmless initial staging-launch error. `SHA256SUMS` covers retained files.

All native runs retain4207 pictures /5,699,037 commands. No slow samples removed.
No added GPU fences, shader changes, game logic, audio changes or optimisations.
The sequential comparator is diagnostic overhead and is explicitly excluded from
product service modelling; no timing subtraction conceals actual required work.

## Reproduction and instrumentation

Sources: `native/audit/stage1_support.inc`, `thread_sampler.c`, `run_stage1.sh`,
`analyse_stage1.py`, `model_pipeline.py`, and `native/tools/build_stage1_audit.py`.
The private builder takes existing CMake build, prepared live-prototype directory,
and ignored output directory as its three positional arguments. It reuses existing
compiler flags and links original game objects, replacing only private diagnostic
bridge/GL objects. Completed builds and build logs are in ignored
`native/generated/stage1/{arm,host}` and `build-{arm,host}.txt`.
No generated ROM-derived resources are included in this evidence directory.

The audit stores fixed-size6006-row CPU telemetry in RAM and flushes at shutdown.
Fine buckets use the24MHz ARM virtual counter; whole-stage probes use wall and
thread CPU clocks. Fine-probe overhead is measured, not subtracted: compiler median
CPU+4.57%, walk+~6.7%. Sampler interval0.5s; kernel task accounting100Hz; reported
last-core observations are not CPU-time shares per core. No CPU affinity, priority,
frequency, governor or scheduler change. The shadow original API does not cover
GLES backend baking/clipping, palette activation or HUD: the paired difference is
frontend overhead, not total rendering overhead.

Remote runner modes are `detailed`, `control`, `production`; full command lines
and environment are in `run_stage1.sh`. It uses only the isolated directory
`/userdata/system/rave-racer-native-stage1-20261015`, RAM logs and the previous
validated M4A asset directory. Production-copy binary is read, never overwritten.
It stops/restores EmulationStation with a cleanup trap. Hardware re-execution
requires authorisation; no further run is requested by this record.

Local analysis, from repository root (Python3 with NumPy):

```sh
python3 raverace/native/audit/analyse_stage1.py raverace/benchmarks/native-stage1-20261015/control
python3 raverace/native/audit/model_pipeline.py raverace/benchmarks/native-stage1-20261015/control
```

Repeat the analysis command for `detailed` or `production`. Percentiles are nearest
rank; medians use the central sample(s). Analysis count uses the exact hardware
period16.693359375ms; model also records the requested rounded16.693ms threshold.
Model is offline arithmetic only, not a threaded implementation. Assumptions:
strict FIFO/three in-flight slots, no hidden replacements, bounded backpressure,
assumed0.1ms handoffs, explicit contention stress, native-phase-only initial empty
pipeline. Two/three-period targets denote latency, not an increased frame deadline.
Legacy transition, scanout and actual native concurrency remain unmeasured.
