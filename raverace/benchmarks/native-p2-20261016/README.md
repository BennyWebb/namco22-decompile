# P2 fixed-clock compiler investigation — COMPLETE / performance FAIL / STOPPED

Authorised native reference race only, full6006 ticks, native1800–6006;
settled underruns counter6006 minus counter(actual legacy release tick), so the
first included counter sample is release+1. Dense window3550–3590 inclusive.
Nearest-rank percentiles, every sample retained, no outlier exclusion.
Clean initial feature/knulli-h700 HEAD49d4b30db039245329bd1ccc987cdf71bc936b01.
P1b original binary remains the fixed-clock baseline, unchanged.

Private sources native/p2 derive P1b into ignored native/generated/p2. Validation
builds retain packet/image SHA, play builds omit digest calls, GL-error polling and output-consistency scans;
capacity/format safety checks and authoritative main checkpoint logic remain.
Separate validation replays prove identical packets/state and36 sampled images.
All run output is persistent /userdata, buffered by P1b idle writers; no output
to RAM-backed/tmp, transfers or archival during timed games.

CPU1512000kHz userspace, original governor/min/max recorded and restored via EXIT
trap after every run; GPU simple_ondemand unchanged. Start SoC<50°C. Headline
observerOFF means no process/memory observer: a lightweight10Hz clocks/thermal
sampler still runs as required. Full observer mode targets explicit game PID;
soak follows an explicit per-lap PID file, never guesses a child from its name.
Clock deviations or nonzero CPU cooling during native analysis flag a run;
no flagged samples are removed. Soak must repeat at1416000 if flagged.

Baseline/play/final ×3, alternating. Each compiler step has independent timing,
full validation and separate profiling; attribution timings never enter results.
Original simulation/physics/audio algorithms, fallback, assets/display timings,
GPU clock policy, production executable/launcher/settings/saves/replays intact.
No commits,pushes or production deployment. STOP at report; further work requires
explicit authorisation.

The first v2 validation attempt remained in preflight cooldown above50°C. It was
terminated before any replay or clock change and retained as preflight-hot.
Subsequent runs stop ES before cooling, with the same unconditional restoration.
Initial partial-play-split populations (play-1, timing-a/b and associated profiles)
remain separately labelled; none substitutes for final full-split headline runs.

Dense window has41 pictures, so nearest-rank p99 equals that run's maximum.
Validation-run timings are retained but excluded from performance comparisons by
protocol: packet/image readbacks are enabled there. The startup attempt ended
at tick386 after operator SIGTERM, before native activation; its manually
annotated wrapper status143 is not a successful full replay. A D-state snapshot
was obtained, but the thread resumed before stack capture: cause remains unknown.
No selective native outlier removal results from that interruption.

The selected source is step b. Its existing v2-timing-b population is final-1,
identified by the same executable SHA and method; counted once per headline
family. A device exFAT symlink preflight failed before launching any game; the
retry uses a plain-text alias manifest. The failed scheduler log is retained.
Rounded per-interval inferred repeats are a proxy, not direct scanout counts:
sub-1.5-period delays can accumulate without incrementing that counter. Native
and dense interval means are therefore reported too, with every interval kept.


## Completed evidence and selection

[Full report](../../NATIVE_P2_REPORT.md). Final selected `v2-b-play` contains play
split + a + b, SHA256
`7f313d01fd5c76c8a0af9babc4e60a794968a85f01fca086b829bf242fccf5c3`.
Steps c/d/e are measured exact-output experiments, rejected for runtime adoption.
Host-only sorting without c/d was never transferred or adopted. See selection.json.

All9 headline replays completed. Final dense preparation p99 is14.310/14.739/14.811ms,
above14ms in every run. Final headline settled underruns/main50ms/repeats are all0.
The1852.85-second14-lap soak has986.39s native exposure,3 inferred repeats,2131
settled underrun sample frames and one64.068ms main gap; precise CPU/scheduling
cause unknown. CPU clock qualification has9868 native samples,0 flags; CPU cooling0.
GPU cooling is active later; no unconditional no-throttling pass is claimed.
No conditional1416MHz CPU repeat is triggered. User settled observation: looks/sounds good.

All6 corrected validation builds pass6006/21sets/4207packets/36Mali image hashes.
All46 complete hardware replays independently pass21 authoritative sets; the startup
interruption is retained separately, never counted complete. Round1 spans compiler
experiments; rounds2/3 are consecutive baseline/play/final triplets, with below50°C
cooldowns. Actual chronology is run-order.csv; no randomised-design claim.

- `run-comparison.csv`, `service-timings.csv`, `headline-summary.json`: complete
  per-run tails and explicit gate evaluation; validation/attribution rows are labelled.
- `paired/`: all signed tick-matched comparisons, negative differences included.
- `tables/`: headline preparation/compiler/walk/presentation and soak bucket/per-lap tables.
- `validation-summary.json`, `validation-method.json`, `full-replay-audit.json`:
  exact coverage, state/packet/image identities and all full replay checks.
- `runs/v2-profile-*-run/`: full-native/dense SVG flame graphs, top-function CSVs,
  authenticated symbolication and sample counts. Attribution timings never pooled.
- `runs/soak-1512000/`: shared process-targeted observer, all14 laps, clock qualification,
  five-minute summary, captured stacks and focused stall-findings.json.
- `build/`: generated-source/build identities, failed/successful recorder builds,
  ROM metadata proof, library byte/address proof, cache memory and executable hashes.
- `runs/final-check/`, `restoration-summary.json`: all47 protected hashes, final
  process/policy checks and35 independent restored sessions. final-1 alias counted once.
- `telemetry-manifest.json`: SHA/source size for1531 published raw/derived artifacts;
  large telemetry is lossless gzip, never filtered. `private-profile-manifest.json`
  indexes raw perf.data/perf-script and unsanitised dmesg kept under ignored
  `raverace/native/generated/p2/hardware/`; ROM/assets/executables are not published.
- `archive-identities.json`: recovered archive SHA256 checks. The archives are ignored.

Final production hash remains6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e;
CPU schedutil480–1512MHz and GPU simple_ondemand original policies restored;
EmulationStation PID3285, no game/sampler/perf remains. No commit/push/deployment.
P2 performance gate FAIL. Historical gates unchanged. STOPPED; further work needs authorisation.
