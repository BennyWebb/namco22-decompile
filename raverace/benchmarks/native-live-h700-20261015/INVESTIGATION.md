# Controlled 4A H700 validation (2026-10-15)

Explicit user authorisation permits diagnostic transfer/run only. M3 remains
performance-blocked, <=12 ms median/<=14 ms p95 world+HUD unchanged. No4B or
speculative optimisation. Production executable, launcher, settings, saves and
replays must remain untouched. Restore EmulationStation after each test.

Prepared ARM SHA256 a9e84cf5f0d0576efbcb011a880c340e1cb38dd990055891601bd50146bc2f04.
Pack manifest2872f4e1984a21e610e5f764c5cd14198fa3b00ffd023bc0a55fdce9a2e2b362.
Separate device directory `/userdata/system/rave-racer-native-live-4a-20261015`.
All eight original pages copied from prior diagnostics, remaining assets/binary
transferred, complete bundle SHA256 verified on device. exFAT extraction requires
`tar --no-same-owner`; initial ownership errors corrected before execution.

Tests retain original simulation/audio/checkpoint code and serial coupled pacing.
Audio output on, original ALSA48000Hz/stereo/512-sample callback. Replay controls
are authoritative; controller mapping/detection is observed but replay does not
establish live responsiveness. No overclock, governor or system-setting changes.
Monitor process RSS, kernel Mali allocation pages, CPU frequency and temperature
once per second. Mali pages use4096 bytes/page. GPU backing is included in RSS;
never add RSS+Mali figures as independent physical memory.

Initial exact prepared-build run (`validation`) failed with kernelOOM exit137
shortly after logged tick300, before native activation1800. Native preload36.949s;
sampled GPU543,862,784 bytes and RSS607,384KiB, kernel killRSS612,360KiB. OOM
trigger was shell monitor fork order2 (16KiB kernel allocation), with all memory
unreclaimable, free4528KiB/CMA764KiB and no swap. Background native asset residency
and growing legacy cache coexist. Buffered timing stream lost on SIGKILL; only
one complete simulation-domain record300 persisted, no full board/audio triplet.
No fabricated timing or checkpoint pass. Kernel and raw evidence retained.

Read-only investigation found~192MiB of pre-existing M2 files on RAM-backed/tmp.
All nine explicitly named M2 trees were copied to `stale-m2-tmp-archive` within
the diagnostic directory, hash-verified, then their RAM copies removed. Files
are preserved, not discarded. /tmp dropped to~0.6MiB. Baseline overlaps that
archival interval; do not call its environment fully matched for performance.
Subsequent runs use a persistent C sampler rather than fork/exec polling.
Neither change alters renderer residency or gameplay, and both are disclosed.

`rr_production_baseline` is an exact copied production binary. It ran all6006
ticks, audio on, serial/coupled policy, without assets preloaded. It does not
have the native renderer's final glFinish, so only completed tick intervals and
common aggregate CPU diagnostics are comparable; baseline submission is not a
matched world+HUD GPU-completion measurement. Not the launcher's threaded policy.

A telemetry-only private relink stores8192 bounded presentation records in RAM
and exports the already-collected simulation records at shutdown. No changes to
original game, renderer shaders, pacing, audio logic or state. Fields include
simulation, scene, backend completion, swap, presentation timestamp, audio queue/
underrun/overrun/reset counts. Existing one-fence native logs retain submission,
finish wait and completion; finish wait is not isolated GPU execution time.
Telemetry SHA25601be097c05579091788c83cb7d3e97f70d74fad81c6373e8f0c03986a5f28fad.
Capture runs are separate from timing runs. No screenshot readbacks or command
file writes during headline timing; all cold and slow samples remain included.

Clean-environment exact prepared-build replay completed6006 ticks,4207 native
pictures,5,699,037 source commands, exit0/zero traps. All21 authoritative checkpoint
triplets pass, including original PCM/voice state. No claim that the OOM prefix
passed. All18 H700 captures pass exact original palette/shadow/LUT CPU oracle.
Hardware world raster differs slightly from host (all36 readbacks differ), with
composed mean absolute RGB errors0.00675..0.16608/255. Four difficult/new scenes
reviewed at1965,3600,5655,5889 show no conspicuous missing/corrupt geometry/HUD.
Capture run is diagnostic only, not headline performance due readback/disk stalls.
Its peak Mali allocation617,852,928 bytes and RSS744,612KiB show whole-live footprint
is materially above the native-only482,760,448 byte resource figure. Audio output
underruns3,621,771 samples, no overruns/resets. User reports garbled/stuttering sound
and slow but visually good motion in the earlier observed prefix.

No-capture telemetry run now measures all6006 original ticks without changing
simulation, shaders, command compiler, serial scheduling or final fence policy.

First no-capture timing replay completed all6006 ticks and all21 authoritative
checkpoint triplets exactly. The diagnostic cleanup was called twice: telemetry
CSV was successfully written on first call, then exclusive-create failed on the
second call after gameplay completion, exit2. All6006 presentation and simulation
samples and all4207 native records survived. This is an instrumentation shutdown
bug, not state divergence or an in-race renderer crash. Fixed with an idempotent
flush guard in a separate private telemetry build; original sources unchanged.
First timing run retained regardless. Repeat is without concurrent bulk transfers.

First timing: native renderer10.182/12.086/13.445/23.884ms median/p95/p99/worst;
2 renderer misses(1800,3482). Actual native presentation intervals31.578/38.361/
41.947/55.202ms, all4207 exceed16.693ms. Scene walk3.302ms and compile7.262ms
medians, simulation10.473ms, renderer CPU2.675ms/final-fence wait7.527ms; no
cross-frame CPU/GPU overlap in this serial bridge. Audio3,602,645 underrun samples.
This live distribution does not retrospectively pass the different M3 benchmark.
