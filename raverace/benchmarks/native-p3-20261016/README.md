# P3 scheduling/preparation investigation — BLOCKED / STOPPED EARLY

Starting clean feature/knulli-h700 atfdf4d15c44686b3f497b26d6ece47b3b7275ed2f.
Selected P2 b binary control SHA7f313d01fd5c76c8a0af9babc4e60a794968a85f01fca086b829bf242fccf5c3.
Native race only. CPU1512MHz userspace, GPUstock; queue2FIFO/valveOFF/O2/stack
protection/no affinity/P1b warmup/async logging/gradual release preserved.
No production/shared engine/fallback/coverage/audio/display changes, commit or push.

Reference all6006ticks; native1800–6006; dense3550–3590 inclusive. Nearest-rank
p95/p99, no outlier removal.41dense samples⇒per-run p99=max. Underruns are sample
frames: fullnative counter6006−1799; settled counter6006−actualrelease tick. Inferred
repeats use unchangedP1b roundedinterval method withT1000/59.7104ms; no claim of
physical scanout counts. Snapshot→swap is not physical scanout age.

Device budget≤2h target; cooldown≤55°C OR180s idle, temperature recorded. One timing
run per control/A1/A2/B1/B2, only final gets2timings. Exactly one final validation
ondevice; host validations for B steps.15min soak; no per-step profiles. All kernel
RT defaults preserved; priority switches defaultoff/native-only. Thread census
checks audio/driver policy and CPU progress. No game output toRAM-backed/tmp;
preflight/tmp+MemAvailable checked; no transfer/archive alongside timedgames.

Private scripts/maths proof: ../../native/p3/README.md. UBSan projection boundary
suite+100million random comparisons: projection-test.txt. Cull random differential:
cull-differential-test.txt. Host fullreplay evidence:host-parity.json. Setup-only
failures retained in setup-notes.txt; no replay or clockchange occurred there.


## Disposition

[Report](../../NATIVE_P3_REPORT.md): priority RR_* switch was stripped by replay
startup. A1/A2 requested-mode files do not prove applied policies; census shows
all stages at defaults. These runs are INVALID scheduling comparisons, retained
in full. Corrected source switch ENG_P3_PRIORITY is untested on hardware. Per user
stop-on-failure rule, no replacement runs, B device timings, final validation,
final repeats, optional perf or15min soak were started. No final build selected.

`run-results.json` includes all native/dense service distributions and validity;
`runs/{control,a1,a2}/` includes every timing/checkpoint/clock/census sample.
`thread-census-summary.json` records actual policy/nice, CPU/runqueue and switches.
All3 hardware replays pass21state/audio sets; play binaries do no packet/image SHA.
`runs/host-*/` retains4host full validations (initial B1/B2 plus native-only guards),
all21sets/4207packets/36host images; host software GL is not new Mali evidence.
`host-parity.json`, unit test outputs and proof identities retain exact validation.
`build/` includes failures/successes and tested original priority header;
`source-identities-final.json` distinguishes corrected, untested source.
`priority-blocker.json` records the cause and unperformed work. `paired/` signed
samples are retained but do not establish priority benefit because none was applied.
`runs/final-check/` retains all47protected hashes, policies/RT defaults and ES proof.
No ROM/assets/executables published; ignored native/generated/p3 retains binaries.

Final production unchanged, all47protected hashes PASS; all3 sessions restored
clock policies/RT defaults/EmulationStation. No game/sampler/perf running.
P3 gate NOT MET (incomplete), not a performance pass or rejection of real-time.
STOPPED. A resumed hardware matrix requires explicit user authorisation.
