# P2 private diagnostic builds

These scripts derive isolated copies from P1b. They do not edit production sources,
launchers or deployed game files. Generated output is ignored under
`raverace/native/generated/p2/`. See SAFETY.md for exact-output arguments and
PERF_BUILD.md for recorder provenance, symbolication and attribution limits.

Build examples from repository root:

```
python3 raverace/native/p2/build.py raverace/build/playable-knulli arm play
python3 raverace/native/p2/build.py raverace/build/playable-knulli arm e --validation
python3 raverace/native/p2/build.py raverace/build/playable-knulli arm e --profile
```

Steps are cumulative: play (complete validation/play split), a (exact-key revision
cache), b (selective gather), c (ROM-derived metadata), d (guarded experimental Z
reuse), e (permutation sorting). Build order does not establish measured benefit;
retain each step's independent validation/timing/profile evidence and select the
final candidate only after review. Profile binaries add frame pointers/symbols and
no-inline attribution; never use their timings as performance measurements.

Metadata export:

```
python3 raverace/native/p2/export_metadata.py raverace/extracted \
 raverace/native/generated/milestone4a-coverage/race-live-v2 \
 raverace/native/generated/p2/metadata
```

The sidecar is CPU-only; no GPU texture/mesh format changes. It is mandatory for
step c and later via ENG_P2_METADATA. All existing geometry/UV entries are checked
against ROM provenance before export. Runtime checks metadata identities.

Device scripts are hard-scoped to /userdata/system/rave-racer-native-p2-20261016.
run_h700.sh saves policies/hash, stops ES for below50°C cooldown, fixes CPU1512,
keeps GPU stock, uses queue2/valveOFF/no affinity, and restores clocks/ES with an
EXIT trap. Headline mode has process observer OFF but mandatory10Hz clock/thermal
sampling ON. No output is placed in RAM-backed /tmp. All scripts preserve failed
and interrupted directories; do not rerun a name that already exists.

v2_steps_h700.sh gates each timing run on complete state/packet/36-image parity.
headline_h700.sh is launched only after selecting
an evidence-supported final candidate. Its first final headline aliases the
selected candidate's existing timing run with the identical binary and protocol;
it is not an extra population or a duplicated sample. Two further alternating
baseline/play/final rounds complete the required three runs of each family.

soak_h700.sh uses sequential complete replay processes: the lifted reset entry
never returns and replay completion exits. No safe in-process original-state
reset lifecycle exists. Explicit per-lap PID files fix process selection; fixture
checks two child PIDs and main-thread stat records. Qualifier repeats a flagged
1512MHz soak at1416MHz while retaining both outcomes. No further frequency change
is automatic. GPU policy and thermal trips are never changed.

Analysis: verify.py checks exact receipts/images/states; analyse.py retains all
samples and nearest-rank tails; paired.py compares identical source ticks;
profile.py resolves only authenticated ELF symbol spans; analyse_soak.py produces
per-lap resources/reclaim and five-minute buckets. publish.py losslessly compresses
telemetry, retaining private perf/kernel-memory dumps outside public evidence.
Final reports require explicit review of all gates, including soak trend; missing
runs are not a pass. These are measurement tools, not a production deployment.

Selection variants can use --no-metadata and/or --no-z-reuse to omit experiments
that do not demonstrate benefit. Their generated directories include these suffixes,
so existing binaries/profile identities and measurements are never overwritten.
Any selected combination requires its own complete parity/image validation and
timing/profile evidence; an omitted experiment is still documented and retained.

Final investigation selection is step b (play split + cache + selective gather).
Steps c/d/e remain reproducible rejected experiments. The sorting combination
without c/d passed host validation but was not transferred or promoted after
sorting also regressed. Device /userdata is exFAT: final-1 is represented by a
plain-text source-population alias, not a device symlink or duplicated trial.


P2 measurement is complete; strict performance gate FAIL. Final b headline dense
preparation p99=14.310/14.739/14.811ms; all three settled audio/gap/repeat counters0.
The1852.85s soak retains3 repeats,2131 settled underrun frames and one64ms main gap.
See ../../NATIVE_P2_REPORT.md and ../../benchmarks/native-p2-20261016/README.md.
No further optimisation is authorised; clocks and EmulationStation are restored.
