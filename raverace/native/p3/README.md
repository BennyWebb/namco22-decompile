# P3 isolated fast-iteration prototype

Derives only from P2 selected step b through P1b/P2 transformations. Never edits
shared engine, simulation/audio algorithms, shaders, FIFO/fallback or production.

```
python3 raverace/native/p3/build.py raverace/build/playable-knulli arm priority
python3 raverace/native/p3/build.py /tmp/rr-m3-reference-gl host b1 --validation
python3 raverace/native/p3/build.py /tmp/rr-m3-reference-gl host b2 --validation
```

`ENG_P3_PRIORITY` unset/off leaves scheduling calls absent. `nice` promotes main and
present to−10 and prepare to−5; `rr` selects SCHED_RR5 for main/present, nice−5 for
prepare. Each thread applies its own switch once at native activation (tick1800),
after startup/audio/driver creation. No global RT setting changes. Census data must
verify the audio/driver threads are unaffected; replay cannot prove input feel.

B1 `projection.h` uses checked64-bit multiplication, exact shift range handling,
signed division-overflow guard and checked add/subtract; otherwise original128-bit
formula. Original valid shifts0..127, C division truncates, arithmetic shift floors.
The unsigned left shift avoids UB. Final clamp matches original. Boundary Cartesian
suite+100million random comparisons run under UBSan before any device experiment.

B2 preserves the original conservative float32 enclosure calculation. Positive
finite zoom and positive Z bounds make projection monotone in X/Y. For a negative
numerator, the minimum uses low Z; for nonnegative it uses high Z, with the reverse
choice for maximum. IEEE round-to-nearest division/multiplication are monotone;
no reciprocal/reassociation is introduced. Initial±1e100 clamps remain. All lower
Z≤1 cases return original reason0, negative/nonfinite zoom uses original corners,
reason precedence and packet flags stay unchanged. Validation builds execute both
original/candidate reasons on every quad and abort on difference. Synthetic UBSan
checks cover all five reason outcomes; packet/image parity independently checks flags.

Device scripts save/restore clocks, RT defaults and production hash via EXIT trap.
Cooldown≤55°C OR180s idle; no GPU/thermal settings writes. Clock/thermal sampling
nominal10Hz in all runs, full process observer only for soak. A once-per-second
all-thread stat/schedstat/status census measures policies, CPU service, runqueue
wait and context switches without changing audio/driver priorities. All output
persistent buffered, no concurrent transfers/archives during games. One run per
variant, one final device validation, final2timings,15-minute process-loop soak.


P3 STOPPED EARLY / BLOCKED: initial RR_P3_PRIORITY was stripped by replay startup,
so A1/A2 promotion did not occur. All3 device runs retained. ENG_P3_PRIORITY is a
source-only correction, not hardware tested. No B timing/final validation/soak
has run; no candidate is adopted. See ../../NATIVE_P3_REPORT.md. User explicitly
requested stopping after failure rather than extending the matrix.
