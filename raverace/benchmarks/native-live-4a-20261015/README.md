# Controlled Milestone 4A evidence — BLOCKED, 2026-10-15

M3 remains performance-blocked; no renderer target was waived. The user authorised
an isolated live-integration experiment and required stopping at a significant blocker.

Host prototype: live simulation -> live native command compiler -> optimized M3
renderer and original live HUD. No prerecorded command stream is opened. Serial,
unpaced diagnostic execution with the original human input recording produced3855
native pictures before missing mesh code2028/packet1456601 stopped tick5655.
No H700 deployment or physical gameplay benchmark occurred. No full replay pass.

- `blocker.json`: actual stopping condition and incomplete replay status.
- `live-command-validation.json`: all3840 freshly generated race packets match M3
  after normalising only offline revision IDs; all5,152,196 source commands retained.
- `checkpoint-validation.json`: all18 available checkpoint triples through5400 match
  original CPU/RAM, board/video and raw PCM/voice state; explicitly not full replay.
- `full-validator-rejection.txt`: original strict validator rejects missing final checkpoints.
- `host-fidelity.json`, `host-hud-oracle.json`: eleven world/composed host pairs match
  byte-for-byte; independent original HUD arithmetic matches all sampled pixels.
- `host-diagnostic-profile.json`, `host-live.txt`: all completed host samples including
  the first native166.361 ms hitch. Traced host data is not H700 FPS/cadence evidence.
- `host-build.txt`, `build-identity.json`, `source-sha256.txt`: build and source identities.
- `production-readonly.txt`, `integrity.json`: read-only H700 production/replay/backup
  verification; no experimental executable was transferred to the device.

Native renderer timings start after scene walk/compilation. End-to-end completion
intervals include those costs and diagnostic I/O; unfenced subspans overlap GPU
execution. Original legacy texture/quad counters do not account for native resources.
Sound output and physical presentation are untested; sample hashing remains active.

The significant blocker stopped further integration work. Extending live asset
coverage and completing full replay validation require a separately authorised
continuation. Milestone4B is not started. See `../../NATIVE_LIVE_INTEGRATION_REPORT.md`.

Lookup data, scene packets and images remain ignored under the home repository's
`raverace/native/generated/milestone4a/`; none is included in this evidence directory.
