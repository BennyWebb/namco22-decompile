# Diagnostic failures and corrections (none excluded from performance data)

- Initial IP connection timed out; subsequent successful idle-only read was followed
  by stalled SCP/closed connection and repeated No-route/timeouts. User rebooted;
  fixed IP192.168.1.69 recovered. No game or clock changes occurred in that period.
- First host sampler smoke harness waited on sampler before reaping its sleep child,
  leaving a zombie target. Task-owned sampler was stopped. Corrected harness reaps
  the child first;23 samples at50.0034ms mean spacing pass. Not H700 timing evidence.
- Initial standalone panel-probe link through SDK GLESv2 stubs failed with missing
  `glClearColor` DSO reference. Linking the same source against SDK `libmali` succeeds.
- Initial host ptrace smoke inside sandbox was unavailable (child exit before trace).
  Repeated outside ptrace-restricted sandbox; default-off records0 affinity syscall
  stops with all game-role/helper masks unchanged. These are attribution tests only.
- Initial affinity smoke demonstrated inheritance: main:0 made the audio-helper
  inherit CPU0 despite no explicit audio pin. Corrected opt-in shim restores the
  original allowed mask at child entry; only requested named roles then receive
  their masks. Final enabled smoke: main0/prep1/present2; audio-helper original0–15
  on host;12 entry/exit affinity stops (three role pins plus three mask restores).
  No H700 affinity/performance run used the initial implementation.
- First unchanged production replay exits0 through6006. Governor/hash restoration
  succeeds, but matrix's immediate `pidof emulationstation` runs before asynchronous
  frontend startup finishes and stops orchestration. Empty original observation is
  retained as `es-restored.initial-race.txt`. Frontend later confirmed running;
  matrix now waits up to30 seconds between runs. Both completed stock runs remain
  acceptance evidence, not omitted/replaced. No timed sample was changed.
- Validation-only packet hash helper cross-compiles successfully. Its first HOST
  compile lacked OpenSSL development headers; using the existing SDK's OpenSSL
  headers with the host's libcrypto.so.3 succeeds. Known SHA256 vectors (abc,
  empty,1MiB pattern) and lengths match Python hashlib, and no packet files are
  written. This helper will never be loaded in timed performance runs.

- Host flag O1 attempt1 completed6006/all21 states and all packets but the harness omitted required ENG_STAGE1_CSV, causing a shutdown exit2. Entire failed run retained under ignored host-validation/O1-attempt1; corrected harness rerun required exit0.
- An asynchronous scp had not finished when validation-only current ARM replay was launched. Controller paused, transfer completion and executable hashes verified, then resumed. This is correctness-only (hash-enabled), and none of its timings are admissible performance results. No transfer overlapped any timed performance run.
- First standalone ARM hash known-vector check lacked the sampler; retained and repeated with persistent20Hz telemetry before replay validation.

- User shortened E timing tests after the first stockO1 run: one full6006 replay per configuration, skipping rounds2/3. In-flight bestO1 is allowed to finish and restore; old controller paused/replaced at idle boundary. All completed samples retained. Flag-repeatability is explicitly not established. B and D completed repeats remain intact.
