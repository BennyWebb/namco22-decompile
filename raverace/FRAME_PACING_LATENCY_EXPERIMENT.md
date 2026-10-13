# RG40XX H frame pacing and image age experiment (2026-10-13)

## Decision

Keep the installed three-stage renderer. The new `RR_PREP_WAKE_ON_SNAPSHOT`
experiment is OFF by default and was not deployed. Five alternating handheld
first-lap pairs showed only a 0.28 FPS median gain, with essentially unchanged
1% lows and slightly worse p99 presentation interval, worst hitch, and image
age. This is not a meaningful subjective pacing or latency improvement.

The playable executable remains SHA-256
`77536f56ad17de9cf8772175c73442d9cb246d87aeb6fbf6c8d7f07c3d6ab53e`.
The known-good backup and sole `Rave Racer.sh` launcher were not changed.

## Pipeline attribution

The representative workload is the recorded player's first lap, ticks
1800–5639, with original HUD and audio, 64 MiB texture cache, and original
three-stage snapshot/preparation/GLES ownership. `produced` is stamped before
snapshot copying; presentation is the swap-return timestamp. These measure
software image age, not controller-to-photon latency.

The previous diagnostic run measured 14.59 ms of preparation CPU and 21.92 ms
of GLES-owner CPU per displayed picture. The GLES owner remains the throughput
limit. In the new matched runs, the old wait policy had median-of-run mean
capture-to-publication 0.78 ms, READY queue wait 7.72 ms, preparation wall
15.46 ms, PREPARED wait 2.75 ms, and GLES presentation wall 23.15 ms.
The combined snapshot/prepared queue never exceeded two pictures; no completed
preparation was discarded. At presentation completion, the queue held two
pending pictures in 63.9% of old-policy samples and 69.5% of wake-up samples.
The preparer policy allows at most one PREPARED picture; the other pending
picture is the latest READY snapshot. Snapshot drops were about 1,169 during the
3840-tick measurement window because simulation runs at 59.9 Hz and GLES
presents about 41 pictures/s.

Compared with the previous two-thread renderer's 38.51 ms mean image age,
the deployed three-stage path measured 49.84 ms in the earlier five-pair test.
The old picture spent about 8.61 ms before rendering and 29.80 ms in rendering;
the three-stage picture spent about 11.25 ms in capture and queue waits and
about 38.6 ms in preparation plus GLES execution. Thus roughly 2.6 ms of the
extra 11.3 ms is queueing, and roughly 8.7 ms is added per-picture execution
wall time. These are derived from separate matched experiments and rounded
medians, so they do not add exactly. Overlap raises throughput while a given
picture still traverses both CPU stages.

The old diagnostic captured the rare tick-1965 texture burst: 79 misses and
uploads, 23.81 ms texture decode wall, 14.78 ms texture upload wall, and
78.13 ms draw wall. The next displayed picture waited 67.82 ms after its
preparation. The wake-up experiment does not alter texture work or the
GLES-owned cache. Moving decode to another thread would require an immutable,
versioned handoff for palette updates, eviction, and ordered GPU operations.
The existing 96/128 MiB cache-budget alternatives were already tested in
`ARM64_GLES2_PERFORMANCE_AUDIT.md`; they are not a new unmeasured shortcut.

## Tested change

The existing policy gives a stale READY snapshot up to 6 ms after GLES claims
its previous frame so a newer simulation snapshot can replace it. With
`RR_PREP_WAKE_ON_SNAPSHOT=ON`, the preparer uses a timed condition wait and
starts as soon as the producer publishes a fresh snapshot, while retaining the
same 6 ms cap. It has the same three slots, one completed picture ahead, and
metadata lock boundaries. No busy-wait, affinity, resource migration or new
worker was added. OFF retains the prior sleep behavior. Per-frame CSV now
separates snapshot publication, READY queue, preparation wall and PREPARED wait.

The candidate saved about 0.98 ms in READY wait, but added about 0.78 ms in
PREPARED wait. The bottleneck stayed on GLES, so the early work mostly moved
waiting between stages. The older unpaced preparer and strict late-latch
experiments in `MULTICORE_RENDERING_PIPELINE.md` already showed worse CPU
waste or throughput. Reducing queue depth further would not remove the
approximately 38 ms combined per-picture execution path and would risk more
GLES starvation. No further queue policy was deployed.

## Five alternating handheld A/B pairs

Both binaries were built from the same source with identical tracing and
different values of the single wake-up flag. The test kept the installed Ports
binary intact and verified all replay endpoints. Values are medians of the five
complete first-lap runs unless noted.

| Metric, ticks 1800–5639 | Prior wait | Wake on snapshot |
|---|---:|---:|
| Rendered FPS | 41.262 | 41.544 |
| 1% low FPS | 20.411 | 20.655 |
| Presentation interval p50 / p95 / p99 | 23.824 / 31.932 / 39.296 ms | 23.655 / 31.611 / 39.364 ms |
| Worst presentation hitch | 96.078 ms | 100.550 ms |
| Image age median / p95 | 49.126 / 65.210 ms | 49.338 / 65.590 ms |
| Simulation rate | 59.904 Hz | 59.905 Hz |
| Snapshot publication / READY wait | 0.780 / 7.720 ms | 0.739 / 6.739 ms |
| Preparation wall / PREPARED wait | 15.456 / 2.753 ms | 15.433 / 3.536 ms |
| Preparation / GLES-owner CPU per picture | 14.725 / 22.594 ms | 14.690 / 22.617 ms |
| Peak pending queue / snapshot drops / prepared drops | 2 / 1169 / 0 | 2 / 1174 / 0 |
| Main / preparation / GLES thread CPU equivalents | 0.869 / 0.617 / 0.937 | 0.870 / 0.612 / 0.946 |
| Audio underruns (runs with any) | 1/5 | 0/5 |

One prior-wait run had 109,732 audio underrun samples and a simulation clock
resync; another prior-wait run and one candidate run also resynced once.
These isolated disturbances are retained in the results, not attributed to
the wake-up flag. The median simulation rate met the emulated target in both
variants. The current GLES owner still uses almost a full A53 core; the
preparer uses roughly 0.6 core and the simulation thread about 0.87 core.

The handheld logs and CSVs are under
`/userdata/system/rave-racer-multicore/ab-20261013-172218-26240`; a local copy
and `summary.json` are under `/tmp/rr-pacing-results`. The runner and analyzer
are `tools/RaveRacer-Multicore-Replay-Benchmark.sh` and
`tools/analyse_multicore_replay.py`. One-second `/proc` samples provide CPU
utilisation and thread state, not sub-millisecond scheduler timing. A separate
representative schedstat diagnostic is recorded below.

## Correctness

Native fixed-cadence frames 600, 1700 and 3600 matched the previously
validated pixels byte for byte; frame 1700 and 3600 ordered texture-event
digests matched with `GLES_TEXTURE_TRACE=1`. Native frame 1700 with
ThreadSanitizer reported no application race and matched the unsanitized
pixels and state; the existing Mesa internal suppression was used. On the
same Mali GPU, recorded replay frames 1965 and 3600 matched pixels, ordered
geometry, PCM/voice, board and simulation state between the two test binaries.
All saved state checkpoints through replay tick 6006 matched the recording.
The final source also builds with parallel preparation OFF, and both ARM64
flag variants rebuild after the compile-guard review. The guard does not
change the measured three-stage code path.
The correctness output is under
`/userdata/system/rave-racer-multicore/pacing-correctness-20261013-173839-794`.

## Scheduler diagnostic

One additional representative first-lap pair sampled each task's Linux
`schedstat` once per second. These counters distinguish CPU execution from
time runnable but waiting for a CPU; condition-variable sleep is not counted
as runnable delay. Values below are deltas across 54 samples within ticks
1800–5639, about 63 s of sampled wall time per run.

| Thread | Prior wait CPU / runnable delay | Wake-on-snapshot CPU / runnable delay |
|---|---:|---:|
| Simulation main | 55.069 / 1.743 s | 54.842 / 1.773 s |
| Preparation | 37.831 / 2.302 s | 38.271 / 2.441 s |
| GLES owner | 59.516 / 1.190 s | 59.872 / 1.267 s |
| SDL audio output | 0.666 / 0.343 s | 0.660 / 0.333 s |

The candidate did not reduce scheduling delay; its preparer waited runnable
about 0.14 s longer in this pair. The previously recorded condition-variable
diagnostic showed 22.83 s of preparation idle and 5.86 s of GLES-owner idle
over the full diagnostic run, with metadata-lock acquisition totals below
14 ms per thread. Condition waits represent pipeline availability, not CPU
scheduler delay. The 1 Hz sampler cannot resolve individual wake-up latency
or GPU driver waits, and this diagnostic pair does not replace the five-pair
headline benchmark. Its files are under
`/userdata/system/rave-racer-multicore/ab-20261013-174532-8107` and
`/userdata/system/rave-racer-multicore/pacing-schedstat-diagnostic.log`.
