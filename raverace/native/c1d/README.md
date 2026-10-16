# C1d host-only provenance — PASS / STOPPED

Read [report](../../NATIVE_C1D_REPORT.md) and
[evidence](../../benchmarks/native-c1d-20261016/README.md). No device capability,
ARM build, alias-separated control, production or shared-engine modification.

From repository root, using retained C1b/C1c assets and the existing host build:

```
python3 raverace/native/c1d/prepare.py
python3 raverace/native/c1d/build.py
python3 raverace/native/c1d/run_host.py
python3 raverace/native/c1d/classify.py
python3 raverace/native/c1d/export_evidence.py
python3 raverace/native/p2/publish.py raverace/native/generated/c1d/telemetry raverace/benchmarks/native-c1d-20261016
python3 raverace/native/c1d/write_report.py
```

These are reproduction commands, not authorisation for further work. Run directories
must be new. Generated ROM/index/provenance/image data stays ignored. Four full
host replays are required: normal colour and provenance, each RGBA/index. Scripts
stop on mismatches, verify all21sets/4207adjusted receipts and retain every sample.

`ENG_C1_TEXTURES=rgba|index` is inherited. C1d adopts lowp palette precision by
default and normalized nearest texture() sampling; diagnostic fetch/highp switches
remain available. `ENG_C1_DEBUG=provenance` opt-in emits integer source identity;
unset retains ordinary colour output. `ENG_C1_PROVENANCE_DIR` selects private
readback output, buffered during active gameplay and exported after thread shutdown.

`provenance.inc` allocates debug targets/grids only when requested. The grid lookup
observes actual nearest atlas selection; per-material integer mappings invert the
packing/source affine, rather than guessing sample coordinates with floating floor.
Additional RGBA8 MRT outputs verify original colour rendering and actual raw texels.
Depth stays16-bit and all geometry/order/scissor/near/priority logic is unchanged.

`classify.py` independently decodes ROM pens plus palette colours at every covered
source address, checks all colour proofs, and classifies every residual using both
GPU identities. Adjacency is eight-connected, identical bank/material and both
coordinates inside original recipe bounds; report distinguishes gutters/wraps.
`--proof-mode rgba|index` independently audits one side without a new replay.
No screen-colour-neighbour heuristic, interpolation bias or pixel tolerance is used.

Result:0same-source,5875adjacent-source,0other. All inside original bounds, no gutter
or wrap exception. No class(c) fix/retry was needed. Ready to undertake unchanged
C1b Part2 only after explicit device authorisation; no hardware claim is made here.
