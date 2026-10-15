# Controlled live integration: Milestone 4A

Authorised 2026-10-15 despite M3 remaining performance-blocked. Existing M3
12/14 ms targets remain unmet and unchanged. No 4B or speculative optimization.

Investigated interfaces: original rr_main frame boundary, rr_scene frame/consume
handshakes, geometry/list emission, rr_gl live HUD, SDL controls/pacing/presentation,
and original game/board/PCM/voice checkpoint machinery. The M3 backend currently
reads compact prerecorded commands; a live compiler and asset resolver are needed.

Initial implementation is a private serial bridge. Production/shared game sources
remain untouched; generated copies only replace presentation-side call sites.
Original logic, input recording/replay, physics, DSP/sound/audio and checkpoints
remain linked from the existing build. Serial operation must be reported as such,
not claimed to demonstrate producer/render thread overlap. Original early-game
rendering may precede native activation; unsupported native assets fail closed.
Native commands must originate in current geometry emission, never frames.rsc.

Before hardware testing: build/review the isolated prototype and verify it on the
host. The user's instruction forbids deployment without explicit authorisation;
H700 transfer/run approval is a separate final step only after a concrete build
is ready. Read-only production integrity inspection remains authorised.

## Current continuation result

Full replay coverage investigation complete; STOPPED. All6006 host ticks and21
authoritative checkpoint sets pass. Added302 ROM meshes/four material variants
without changing original M3 IDs, command parity or existing readbacks.
Native static482,760,448 bytes fits512 MiB; boot remains legacy-rendered.
ARM built locally only; no handheld transfer/run. Full interactive coverage and
H700 measurements remain open; M3 performance-blocked. See current report and
`benchmarks/native-live-coverage-20261015/README.md` for evidence/reproduction.

## Historical initial result

 BLOCKED / STOPPED

The host live run completed native ticks1800–5654, then exited2 on tick5655
because mesh code2028/packet1456601 is absent from the static M3 pack. Integration
experiments stopped there. This is not a completed live port or full replay pass.
No ARM prototype was built or copied to H700. Read-only production checks pass.

All3840 racing-window commands match the M3 packets after normalising only offline
font/style/palette revision indices. All5,152,196 source commands are accounted for.
Eleven host world/composed pairs are byte-identical and independently HUD-validated.
CPU/RAM, board/video and raw PCM/voice checkpoint records match at all18 available
checkpoint ticks through5400. The original final6006 checkpoint remains unvalidated.
See `NATIVE_LIVE_INTEGRATION_REPORT.md` for timings, scope and known limits.

## Reproduction references (work is currently stopped)

The build and test used these commands, with new output paths for each execution:

```sh
python3 raverace/native/tools/export_live_lookup.py \
 raverace/native/generated/milestone3/race-v3 \
 raverace/native/generated/milestone3/audit-identity \
 raverace/native/generated/milestone4a
python3 raverace/native/tools/build_live_prototype.py \
 /tmp/rr-m3-reference-gl raverace/native/generated/milestone4a/prototype-host
```

The exporter refuses an existing output directory. The host builder can reuse its
own private directory with `--rebuild`; it never changes the original CMake build
objects or executable. For the recorded test, the existing compatible CMake GLES
build supplied unchanged game/audio objects. Source hashes identify the exact build.

Runtime environment:

```text
SDL_VIDEODRIVER=offscreen
ENG_LIVE_PACK=<repo>/raverace/native/generated/milestone3/race-v3
ENG_LIVE_SHADERS=<repo>/raverace/native/generated/milestone3/fragment-hud-certified-shaders
ENG_LIVE_LOOKUP=<repo>/raverace/native/generated/milestone4a/live.lookup
ENG_LIVE_CAPTURE=<new capture directory>
ENG_LIVE_COMMAND_DUMP=<new packet directory>
```

Runtime arguments:

```text
rr_native_live raverace/extracted
 --replay-input raverace/native/generated/milestone3/inputs/race-20261013-122242-25136.rec
 --input-checkpoints <new states.jsonl>
 --unpaced --presentation-thread off --audio off
 --profile-stages --profile-simulation --profile-video --profile-from 1800
```

The original replay environment sanitiser is unchanged. Adapter settings use the
`ENG_` namespace so they do not bypass its removal of gameplay `RR_*` overrides.
The prototype reads only mesh/material resource files, not recorded scene frames,
font snapshots or style snapshots. Its optional packet/image output is diagnostic
and must not be mistaken for a low-overhead H700 benchmark. Source/checkpoint
validation reads the already completed run and does not resume simulation.

Private executable and generated copies live under the durable home repository:
`/home/bwebb/dev/namco22-decompile/raverace/native/generated/milestone4a/`.
Numeric/raw evidence lives in `raverace/benchmarks/native-live-4a-20261015/`.
All extracted lookup/geometry/image outputs remain ignored. Rollback is simply
exiting the private process; production executable and launcher were never changed.

Known uncompleted work: full live mesh/material/format coverage, full6006 replay
state/audio validation, ARM build/driver validation, physical controls/audio,
low-overhead H700 timing/overlap/presentation/stutter and interactive race coverage.
Further work requires explicit continuation; no optimization or4B has started.
