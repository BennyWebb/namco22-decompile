# Private Mali texture diagnostics

Read `../../MALI_TEXTURE_ARRAY_DRIVER_INVESTIGATION.md` for measurements and
limits. No production executable, build objects, source defaults or launchers
are changed. Run from the repository root. No commits are made.

- `build_trace.py`: private copies of texture/submission sources, linked to the
  existing `/tmp/rr-world-array-arm-build` objects. CSV events at ticks 1960–1970
  and per-operation/per-tick totals elsewhere are buffered in memory; text is
  written after the replay. Binding API duration is sampled 1/64. Submission
  reference timestamps do **not** prove GPU completion. No new fences, queries,
  waits or finishes are inserted into replay tracing. Existing retirement
  fence polls are timed; fence/poll events use the affected cohort identity,
  and invalidation records preserve layer/mask before release. `ENG_MALI_CALL_TRACE=/existing/empty/path.csv` enables
  tracing; exclusive file creation prevents accidental replacement.
- `build_probe.py`: standalone SDL `mali`/GLES diagnostics using matching
  Buildroot headers, compiler and `libmali`. `probe.c` compares fresh 2D and
  arrays, idle and in-flight layer updates, immutable storage, orphaning,
  rotation and eight-layer storage. Five warmups; 40 rotated samples; repeat
  three processes. `ENG_PROBE_PAYLOAD_DIR` loads captured RGBA bytes for each
  observed shape, enabling a synthetic/actual comparison. A dedicated orphan allocation preserves the other cases'
  initialized layers. `ENG_PROBE_PRESSURE_MIB=64|112` adds retained RGBA8
  storage in 8 MiB increments. This changes occupancy, not upload payloads.
- `build_burst_capture.py`: host-only fixed-history capture of the actual
  tick-1965 decoded bytes and ordered request/hit/miss/bind tags. Buffer first,
  write after replay; not a performance mode.
- `burst_probe.c`: 75 ordered allocation shapes from the actual tick-1965 miss
  trace; known-idle mutable arrays, identical uploaded bytes, fresh retirement
  versus persistent storage. `ENG_BURST_PAYLOAD_DIR` instead uses the complete
  actual captured fixed-history burst; workloads are compared only internally. Each strategy has 40 samples after five warmups,
  repeated in three processes. Fresh retirement uses one fence/delete per
  resource; the full-game allocator's searches and renderer are absent.
- `build_retain.py [BUILD_DIR [OUTPUT_DIR]]`: private candidate; keeps empty
  two-layer cohorts after retirement fences signal. Exact-dimension empty
  cohorts may be reused; incompatible idle storage is deleted under the
  original physical cap or metadata bound. Runtime
  `ENG_WORLD_ARRAY_RETAIN=1` enables it; unset retains the original policy.
  Both source paths and original binaries remain available. The original
  atlas compile/runtime controls still default OFF. `capacity_reap_calls`
  counts resource-pressure reaping, **not** fence waits;
  `blocking_fence_polls` counts actual blocking-form GL polls.
- Shell runners: refuse conflicting known test/game processes, stop and wait
  for EmulationStation and its wrapper, use bounded timeouts for diagnostics,
  and restore the frontend. Files go beneath the existing diagnostic directory
  `/userdata/system/rave-racer-world-atlas-20261014`, never Ports. Confirm the
  device is idle before using a runner; do not run them concurrently.
- `benchmark.sh`: production/original/candidate, five alternating three-way
  rounds, same first-lap ticks 1800–5639, HUD/audio/profile/miss trace and
  one-second process/CPU/memory monitor. No reference readbacks or detailed
  API tracing. `analyse_benchmark.py` extends the established analysis to
  three variants. `analyse.py` summarizes isolated API times and replay
  operation totals. All reported GL durations are CPU wall time.

Production executable SHA256 must remain
`6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`.
Do not copy diagnostics into the playable launcher or add Ports entries.

Do not transfer binaries/results during any timed run. Strip debugging
sections from future private game binaries before device transfer, and flush
preparation writes while idle. The final accepted set replaces the entire
transfer-confounded round two with round six; both raw rounds are preserved.
The reporting scripts do not distribute separate diagnostic GL times into
headline replay upload averages.
