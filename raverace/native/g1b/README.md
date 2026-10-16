# G1b private diagnostic derivative

See ../../NATIVE_G1B_REPORT.md and ../../benchmarks/native-g1b-20261016/README.md.
This is an isolated G1 → C1e → P3 build derivative, not a production replacement.
The attract compiler slowdown is unresolved; do not treat this as accepted play code.

- `prepare.py`: reuse the hash-verified ROM-only G1 pack; debug padding and
  validation identity mapping only. Never adds replay-discovered materials.
- `build.py host|arm [--validation]`: generate private source/builds beneath
  ignored `native/generated/g1b/`. Generated simulation remains -O2, protected.
- `dynamic.h` / `dynamic.inc`: deterministic bounded allocator and runtime
  ROM decoding, immutable slot payloads, three fenced GL upload copies.
- `transform.py`: hook those resources, snapshot RAM activation and receipts
  into the established private renderer; no shared engine edits.
- `host.py NAME [--debug] [--stream-trace]`: full original-state replay.
- `validate.py`: all checkpoint/packet gates, runtime pen parity and C1d-style
  source-identity classification against the existing RGBA baselines.
- `test_allocator.c`: CPU ownership/eviction/bounds stress.
- `verify.py`, `analyse.py`: reference restoration, checkpoints and nearest-rank
  timing populations. Timing window 1800–6006; settled audio starts at actual
  release. `coverage.py` retains every picture, raw course selectors and fallback.
- `attract.py`: arbitrary-length attract telemetry, memory, clocks and drain checks.
- `review.py`: all 36 image side-by-sides and contact sheets, kept ignored locally.
- `publish.py`: lossless telemetry and readback hashes without ROM assets/pixels.
- `run_h700.sh`: isolated fixed-CPU execution, persistent output, protected hashes,
  per-run restoration trap and EmulationStation recovery. Do not run without a
  separately authorised hardware task. The interrupted attract run is historical.

Runtime: `ENG_C1_TEXTURES=index`, existing P3 FIFO/default priorities. New optional
`ENG_G1B_STREAM_TRACE=1` routes native traces through the existing asynchronous
memory cookie as generated; default preserves the predecessor RAM trace transport.
It does not authorise synchronous game-thread disk writes. Historical `TRACE_DRAIN
mode=sync` labels that streaming transport, not persistent-storage calls.

Reference timing uses preserved `reference-v2` binaries. Attract uses the one-line
logging variant `play-stream`; host byte equivalence and smoke receipts are retained.
No clock, affinity, priority or gameplay changes distinguish those builds.
