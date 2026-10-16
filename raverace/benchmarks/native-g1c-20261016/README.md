# G1c evidence — 2026-10-16

**Attract gate PASS; overall BLOCKED on reference preparation no-regression.**
See [report](../../NATIVE_G1C_REPORT.md). Hardware is complete and restored.

## Baseline and isolation

Initial clean branch `feature/knulli-h700`, HEAD
`24746a29e3a74cae8d8b103c421330b9bd47fd36`. Read HANDOVER and G1b report first.
All implementation is private `../../native/g1c/`, derived from G1b/G1/C1e/P3.
No shared engine, physics, simulation timing, audio/control logic or production
changes; no commits or pushes. Unchanged static ROM-only assets:2,421 models,
21,783 footprints, seven R8UI pages/112MiB. No replay-derived asset additions.

## Attribution before fixing

Host original-cache attract runs complete13520ticks. `attribution` retains the
initial combined-stream capture; `attribution-clean` repeats with separate stdout
and stderr so all counters and footprint keys are readable without interleaving.
These are attribution runs, never device performance populations. The old cache
algorithm/material resolution is unchanged apart from counters.

Counters confirm saturation: occupancy32768 at12839; every new lookup can scan
32768slots, without inserting a result. Last reported problematic window
13393–13520 averages1940.438probes/lookup and8230full-table fallbacks; walk remains
small. `attribution.json` retains the initial summary; clean-run raw counters
are also retained. Device G1b compiler had reached~290ms on this same phase.

The repaired host attract completes15000ticks: peakoccupancy36031, meanprobes
1.00138, max3, fullfallback0, pageevictions0; late compiler median0.358ms.
Host timing is diagnostic and not a substitute for H700 measurements. Common135
periodic attract domain records are exact across old/fixed runs; differing final
checkpoint ticks are explicitly not compared. Full reference state/packet/image
checks are independent below.

## Bounded cache and dynamic capacity

Logical hash cells =nextpow2(2*21783*128)=8388608,32MiB. Cells pack16-bit generation
and16-bit exact-key-pool index. Pool65535*32bytes=2097120bytes; keys compare all7
recipe integers. Reset generation at pool limit or64-probe limit; generation wrap
clears cells before reuse. Actual occupancy cannot exceed min(capacity/2,65535).
The globally cached value is static lookup result, including misses, not a dynamic
per-picture material ID. RGBA lookup retains its old implementation.

`test_cache.c`:400002lookups plus forced probe collision, pool turnover and
16-bit generation wrap; exact results. `test_allocator.py`:300pictures with586
interleaved evictions; queued bytes preserved, new capacity bounds pass.
The initial allocator test used the obsolete1021physical-size rejection fixture;
updated it to3072-3. Logical input guard1020 remains unchanged. This was a stale
capacity fixture, not a checkpoint/packet/pixel mismatch. Setup corrections/logs
are retained in setup-corrections.json.

Working-set trace:64205logical requests,23653unique; summed padded rectangles are
~36.7MiB per ordinary selector (transient/repeatedly overlapping source areas,
not simultaneous residency). Merely keeping separate copies is inefficient.
Physical source bounds round outward to multiples of256, per bank/cmode, with
original two-source-texel gutters. Exact logical recipes and palette groups remain
unchanged. This yields39allocations/5556848bytes for the observed whole trace.
A3072²page (9MiB), three fenced GPUcopies/27MiB, replaces three1MiB pages:
GPU delta24MiB. Decoder still uses ROM directly; no material catalogue is learned.

`prove_blocks.py` compares every padded logical crop with independent ROM decoding:
192647935texels exact. Material UVs reference the shared allocation source origin;
provenance mapping references original logical footprint and adjusted atlas origin.
Immutable slot staging/fences and current-picture pinning are retained.

## Host correctness

host/ validation/classification/proof files: all21checkpoint sets/63domain records,
4207adjusted packets,36images, class(a/b/c)=0/6802/0. Raw reference packets and all
36colour images are also byte-identical to G1b, recorded in g1b-equivalence.json
and g1b-image-equivalence.json. Every validation decode SHA matches ROM pens.
No new Mali provenance run was requested; host provenance plus previous G1b Mali
baseline must not be mislabelled as a new G1c hardware image proof.

## Device method and populations

Separate directory `/userdata/system/rave-racer-native-g1c-20261016`, original
production `/userdata/roms/ports/rave-gles2/rr_gles2` untouched. CPU1512MHz userspace,
GPU stock/simple_ondemand, q2 strict FIFO, valveoff, default priorities/affinityoff,
-O2 generated sources with stack protection. Per-run saved governor/min/max and
RT defaults restored by EXIT trap;47 protected hashes checked before/after,
EmulationStation restored and no game left. No display timing changes.

Order: one1900tick smoke, one6006tick reference timing, one930second attract.
Smoke confirms actual main/rr-prepare/rr-present nice0/policy0, repaired capacity,
source_blocks256 and async streaming receipts. No switch-dependent timing began
before those checks. Attract has no replay, recording catalogue or controller input;
930second timeout permits >=15minutes of actual presentation after startup.
Timeout124 with clean drain is expected, not a failed game exit.

Reference input unchanged race-20261013-122242-25136.rec. Matched race1800–6006,
dense3550–3590 (41samples); settled audio after actual legacy release178.
All samples and transitions retained, no outlier exclusions; nearest-rankp95/p99.
Full activated timeline also reported, including any1578-type repeat.
No fence timings: the vsync dependency gate remains retired.

`coverage.py` retains every picture/fallback/dynamic upload. `cache_analysis.py`
reports all selectors, each visit and minute, repeats/underruns/gaps/service times
and cache work. A visit ending at the final tick is explicitly incomplete, not
counted toward the two-complete-visits gate. Compiler growth is assessed per
selector/visit as well as minute buckets, avoiding cross-course workload confusion.
Repeated vblanks are inferred with the unchanged nearest-panel-period method,
not hardware vblank-counter events. Snapshot→swap age is not measured scanout age.

Single long-lived SCHED_IDLE/nice19 observer samples memory/Mali/clocks/thermal and
both cooling states at100ms. `resources.py` associates each sample with latest
presented selector (boundary attribution can be one picture/queue latency off).
All per-selector maxima/minima and frequency/cooling distributions retained.
`attract.py` checks game and observer drain receipts. No timed transfer/archive;
all output persistent userdata, /tmp usage recorded before each run.

User observed the attract run: “It's looking and sounding great.” This is a
subjective observation during the run, not an exhaustive image/input test.

## Retention and reproduction

Private tools README documents build and run entry points. Host-generated assets,
executables and ROM-derived pixel readbacks remain ignored. Source/binary hashes,
full raw timing/clock/memory traces, checkpoints and receipt tables are published
losslessly under runs/ and session/ after hardware completion. Large text uses
deterministic gzip; telemetry-manifest.json records raw/stored SHA256. Readbacks
are represented by hashes only. Initial setup failures (no symlinks on userdata,
incomplete transfer and tar UID/GID handling) are retained, corrected before any
smoke/timing launch; no failed correctness gate bypassed. Nothing was excluded
from timing statistics and no replacement timing runs were made.

## Final hardware results and integrity

Attract55443ordered pictures/928.459seconds,55281native,162boot/prewarm legacy;
zero unsupported fallback, inferred repeats, settled underruns, native main gaps
>50ms or page evictions. Complete visits0/1/2/3=5/4/4/4, plus partial selector1.
Probe average1.001745,max3; occupancy39946plateau; full fallbacks/resets0.
All cache, coverage, thread and upload populations account for every picture.
Per-minute/per-visit distributions and all stages are in cache-analysis.json;
per-selector memory/temperature/clocks in selector-resources.json.
Mali peak178.863MiB; RSS HWM360.910MiB; minMemAvailable502.125MiB, CmaFree0.
All9304clock samples1512MHzCPU/240MHzGPU; both cooling states0; peak58.787°C.
Both writer and observer fully drained,0drops/errors. All transitions retained.

Reference all21sets and pacing pass,0repeats/settled underruns/main gaps over the
entire activated timeline. Historical1578repeat did not recur; its original
cause remains unknown. **Preparation regresses:** native median9.032119ms versus
G1b8.333597/8.342555; densep9913.361770ms versus13.120222/13.222303. This is not
waived as noise or hidden behind clean presentation. gate.json marks overall
BLOCKED while separately marking the completed attract gatePASS. Sparse hash
access and expanded immutable slot copies are hypotheses for the added cost;
no further optimisation or extra timing run was started.

The SSH monitor socket dropped late in the run. Password-entry automatic approval
review timed out once; the permitted retry succeeded. No test restart or missing
samples: independent930second timeout completed with124, cleanup0 and clean drain.
Final pack hash verification initially included5host-only JSON metadata files
that were intentionally omitted from the predecessor runtime bundle. Corrected
list verifies all22actual runtime assets; no runtime hash mismatch occurred.
Those setup receipts are retained, not represented as a game correctness failure.

session/device-session/final-state.txt verifies47protected paths and unchanged
production6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e;
all4CPUs schedutil480000–1512000, GPU simple_ondemand0/0, RT950000/1000000,
EmulationStationPID21797, no game/sampler. Per-run restoration0. Staging uptime
47742.78→final49232.98 (~24.8min). No new tests after that snapshot.
Production launcher/settings/saves/replays unchanged. No commit/push/deploy; STOPPED.
