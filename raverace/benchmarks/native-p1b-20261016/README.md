# P1b native stall investigation — COMPLETE / performance FAIL

Authorised stock-clock native replay only, no production changes or P2.
Initial clean feature/knulli-h700 HEAD95a4ad281579aac697b07a2c591d543dcc19ba46.
Private code native/p1b, generated builds/raw data native/generated/p1b.

Memory-cookie stdout/stderr and all write-mode fopen calls feed preallocated
rings; only SCHED_IDLE/nice19 rr-writer opens/writes outputs. Dedicated reserved
checkpoint memory is retained until shutdown, completely drained before exit.
Packet SHA256 receipts are computed synchronously on rr-prepare per live native
compilation, included in measured preparation CPU/wall costs, and flushed outside
the race. No uninstrumented compiler cost is inferred from these runs. The16MiB pool is prefaulted before game startup.
Ring-full writes drop/count chunks instead of blocking; every normal run requires
zero drops and every checkpoint/packet. A64MiB host pressure fixture proves
nonblocking overflow and complete128KiB reserved checkpoint drain.

Prewarm uses one startup-loaded validated tick1800 packet ONLY for invisible
GPU warmup, not displayed game frames. Pre-1800 menu assets are intentionally
unsupported by the race-only native pack, so live pre-menu compilation is not
used. Three fully touched upload copies and both native shader variants plus
complete HUD composition render to an offscreen FBO at legacy ticks1740–1742.
Race packets1800–6006 continue to originate from immutable live state. Native-only
cache retirement starts after16 stable native pictures,8 legacy textures per
subsequent picture; existing fallback lookup/recreation remains unchanged.

Valve defaultOFF; when blocked beyond2 panel periods, it replaces only PREPARED
unshown slots, never a slot being read/prepared. Every simulation tick and native
compilation remains; counts and replacement tick IDs are retained. Temporary
wall-clock pacing applies during overload, returning to FIFO on a free slot.
Test-only artificial present-delay switches are unset in every hardware runner.

Observer10Hz, SCHED_IDLE/nice19, no /proc/pressure reads, memory-buffered output
in64KiB batches to its own idle writer, per-read timestamps, no catch-up bursts.
Headline observerOFF. Starting SoC temperature ceiling50°C after cool-down,
except subsequent laps of the single continuous30-minute soak session.

Full6006 reference ticks;1800–6006 inclusive4207-sample native analysis, no outlier
removal, nearest-rank p95/p99. Matrix q2/q3 valveOFF ×3 alternating, q2 valveON ×3,
one q2 observerON. Full checkpoint/4207 receipt parity in every run. Separate
strace attribution run, never pooled with headline timings. Soak uses consecutive
complete replay processes to reset original state, no cooldown/ES between laps;
continuous stock session and observer, no single-process memory-leak claim.

All evidence stays on persistent /userdata during runs; no concurrent transfer
or archival. No governor/frequency writes in P1b runners. ES restored and original
production hash checked after each run. Soak is one run, restoring once at end.

Exit interpretation declared before headline measurements: “handful” means at
most5 repeats per headline run, each a single inferred repeat, separated by at
least60 ticks; all three stock q2 valveOFF runs must also have zero native main
gaps>50ms and zero native underrun sample frames. Soak degradation is assessed
from all six5-minute wall buckets with native exposure reported separately.
Missing/partial evidence is not an acceptance pass.

Headline results are available in `runs/all-run-metrics.csv` and
`runs/pooled-metrics.json`. Nine complete observer-OFF populations and one
observer-ON population are retained, with every checkpoint and packet digest.
Large text/CSV inputs are gzip compressed without removing rows. The telemetry
manifest includes compressed-file and original-content SHA256 values.
Raw ROM packet bodies and pixels remain in ignored generated storage. Raw strace
is also kept there because device writes can contain audio data; derived syscall
records and archive checksums provide review evidence. No commit is performed.


Final result: all three q2 OFF headline runs pass the main-gap and isolated-repeat
subgates, but fail zero native audio underruns. The 1867.31-second soak completes
14 laps, all checkpoint/packet comparisons passing; it adds11 inferred repeats,
45564 missing audio frames and one~50ms main gap. No acceptance threshold changed.
See ../../NATIVE_P1B_REPORT.md for results, distributions and limitations.

IMPORTANT: soak follow-child selection stayed on supervisor PID12882. Global
clock/thermal/memory/vmstat samples are usable; process RSS/I/O/thread stacks are
not replay-process evidence. observer-summary.json marks coverage invalid.
The directly targeted observer-q2 run has valid game-thread records. The remaining
soak stall's cause is unknown; coincident global reclaim is only correlation.
The separate trace finds zero native regular-file/stderr writes or write-mode
opens on game threads; driver eventfd notifications remain. Traced timings are
attribution-only and never pooled into headline performance results.

Final device-final.txt verifies47 protected hashes, stock governors, restored ES
and no diagnostic game/sampler/tracer. No commit,push or production deployment.
Raw archives remain in native/generated/p1b, with hashes in archive-manifest.json.
