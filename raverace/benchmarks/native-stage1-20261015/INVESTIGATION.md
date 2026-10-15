# Stage1 bounded architecture/CPU audit — 2026-10-15

User authorises investigation/profiling only: no CPU optimisation, thread changes,
new renderer architecture, Stage2,4B, commit/push or production deployment. M3
remains performance-blocked. H700 diagnostics isolated, frontend restored after
runs and protected production/config/ROM/replay hashes verified.

Source findings: production already uses3-slot simulation→preparer→GL worker
pipeline with immutable snapshots, TLS snapshot selectors, no lock across copy/GL,
OS scheduling without affinity pinning and latest-ready/drop semantics. A6ms prep
coalescing delay is calibrated to historical GLES work, not native costs. Simulation
keeps original ticks/audio; SDL callback separately drains atomic audio ring.
Native prototype forbids presentation-thread on and runs simulation→native walk→
compile→GL/HUD/final fence→swap in one thread. Moving a call alone is unsafe: live
compiler and HUD read mutable g_rr, direct FIFO, globals and single live_slot.

Matched audit: same-frame immutable snapshot passed to EXISTING GLES world
preparation API; original/native order alternates by tick. No new worker/queue or
native snapshot architecture is implemented. Existing original geometry is run as
sequential diagnostic shadow work only. Copies and extra original work are timed
separately and never called acceptance throughput. The original shadow uses same
private geometry source provenance hooks; small metadata-hook overhead exists
in both walks. Palette activation/HUD preparation and GLES backend triangle/bake
work are outside original world-preparation API; differences are disclosed.

Detailed mode partitions original native compiler into14 exclusive wall-time
buckets using ARM generic virtual counter, with per-frame whole-stage thread CPU
and core start/end probes. No timer per inner vertex or comparison. Counter pair
cost is calibrated and logged. No subtraction/hiding of instrumentation cost;
mode0 control retains shadow comparator but disables fine probes. emitter capture
cost sampled per callback in detailed mode. All rows stored in fixed6006-row RAM
array and written at shutdown; native trace remains bounded RAM. Thread sampler
uses one process,0.5s /proc task CPU accounting/core observations and memory/clock
samples, no affinity/scheduler/frequency changes. Actual production-threaded run
uses existing --thread-profile and --thread-sync-profile options, not modified
production code; original binary is read from the earlier diagnostic copy.

Host functional gate: complete6006 ticks and all21 checkpoint sets pass,3840 M3
command packets/5,152,196 source commands match byte-for-byte after only original
offline revision-ID normalization. Shadow original quad count must equal native
source count every tick. Generated builds, command dumps and ROM-derived resources
stay ignored. Stage1 tools/support sources contain no assets.

First launch raced the staging transfer and could not open run_stage1.sh. No game
or frontend change occurred. Transfer completion and file presence were checked
before corrected launch. That launch error remains in the isolated device log.

Completion: detailed/control/unchanged-production runs exit0, each6006 ticks and21
checkpoint sets pass. All4207 host native packets exactly equal prior validated
full-replay packets without normalization. Original3840-picture M3 parity also
passes. Compiler controlCPU7.231 ms median; native frontend additionalCPU6.722 ms.
Culling/material resolution dominate; no optimisation implemented. Model supports
GO for a separately authorised limited three-stage prototype, with fragile tails:
10% CPU contention causes163 late three-period targets. This is not proof of60FPS.
Final47 protected hashes unchanged; EmulationStation restored, no profiling game.
Stage1 complete / STOP. M3 blocked; Stage2/M4B remain unauthorised.
