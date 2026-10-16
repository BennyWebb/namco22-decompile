# G1 — ROM-only static pack; BLOCKED at host native validation

Read [report](../../NATIVE_G1_REPORT.md). Branch feature/knulli-h700, unchanged HEAD
6026f57e13172eb23ef7eb7cbf0461aa7351493c; clean starting tree. No device test/transfer.
The compact index atlas is adopted. The vsync-paced14ms fenced gate is retired by
explicit user decision. This attempt stops for missing native reference coverage,
not GPU completion. No gate is retrospectively marked passed.

assets.json is the complete ROM-keyed output manifest:2421models,97116quads,
21783static footprints/materials, seven112MiB index pages;564081621padded texels
compared against ROM decoder with zero differences. ROM/builder and payload hashes,
115.553s build time and all packer statistics included. No recordings used by
assets.py. Generated pages, mesh buffers, full material catalogs and ROMs stay in
ignored native/generated/g1/, not this evidence tree. Shader text is reused from
C1e; it does not contain scene/recording data.

host-gate.json is the failing gate:6006ticks and21sets exact, but0/4207 native
receipts and0images because all4207 reference-phase pictures use legacy fallback
for missing direct materials. Three reference recipes are absent from the static
point-ROM inventory; all nine direct quads in the old tick1800 control use them.
The control/reference catalogs are validation/attribution inputs only, not pack
building inputs. No fallback picture is counted as a native success.

coverage.json retains all6006 picture decisions; coverage-by-state.json groups
raw RAM state/course IDs without guessing course names. Source-state gates are
provisional, not attract-validated. Full coverage counts:159boot,3prewarm,7covered,
5837direct-material fallback. The entire1800–6006 window is fallback.
One stderr coverage record is split by shutdown stdout; analyse_host.py rejoins
its intact prefix/continuation, retaining both the original bytes and a reassembly
count. It does not drop the picture or infer its reason from adjacent pictures.

host/audit-state is the1900-tick fixed-activation diagnostic for RAM-state discovery.
host/whole-colour is the full failing whole-ROM validation population, including
states, thread timing, packet/header-only receipts, method and complete output.
Host timings are not H700 performance results. No image/provenance gate claimed.
Files>64KiB are losslessly gzip-compressed; telemetry-manifest.json records original
and stored SHA256. Every attempted run is retained. No retries after the host gate.

Private build/test code: native/g1/README.md gives reproduction. The validation-only
reference_identity.py remaps static mesh/material IDs; it is never called by the
ROM asset builder. Its input/output hashes are in source-build-sha256.json, along
with the private sources and host binary/build receipts. Missing IDs stay−1.
The selected host build has ENG_G1_STATE_AUDIT unset and does not read a recorded
warm packet. Three live slot payloads provide offscreen warmup.

Device contact was read-only: initial production hash matched C1e; no new binaries,
assets or scripts were copied, no governor/service or game changes were made.
device-final-verification.txt independently confirms47protected paths, production
hash, stock CPU/GPU settings, EmulationStation PID5200 and no game/probe/sampler.
No actual H700 GPU memory/load time, reference timing or10-minute attract data exist.
These remain required gates, not zero-valued results. G1 BLOCKED; STOPPED.
