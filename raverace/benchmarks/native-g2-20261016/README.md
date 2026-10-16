# G2 — bounded cache and separate playable Native entry

Started on `feature/knulli-h700`, HEAD `588fade914a295c714f08ecaa3733533825c00e0`.
Changes remain private to `native/g2/`, this evidence and the report/handover.

- `host/`: two full 6006-tick runs (colour/provenance), all 21 checkpoint sets,
  4207 adjusted/raw receipts exact, 36 image provenance proofs, class(c)=0.
- `test-cache.log`, `test-staging.log`, `test-session.log`: exact-key collision/
  generation tests; interleaved immutable-slot reuse; fake-sysfs restoration,
  owner SIGKILL watchdog recovery, save isolation and asynchronous telemetry.
- `rom-builder-parity.json`: first-launch C ROM decoder reproduces all existing
  pen pages and mesh bytes. `asset-wrapper-test.json`: cold build then verified hit.
- `collector-reference-test.json`: bounded session collector reconstructs every
  reference picture from independent streams; distinguishes boot handover from
  native pacing. `app-hashes.json` identifies the separate package.
- `smoke/`: switch receipts to1900, default policy/nice census, capacity131072,
  immutable3072² page rotation and zero settled copies; all restoration checks.
- `reference/`: the single requested device timing. Raw thread/tick CSV, all21
  checkpoint sets, stderr/stdout, persistent10Hz memory/clock samples, census,
  all47 protected paths, before/after clock/RT/hash receipts. No simultaneous
  transfers/archive. Analysis window1800–6006, dense3550–3590; nearest-rank tails,
  every sample retained. Play build deliberately performs no packet hashing;
  exact packets proven separately on the same source in host validation builds.
- `staging-summary.json`: actual CPU copies, distinguished from old pending bytes.
- `ports-smoke/`: cold-cache first launch, ~120s attract, scripted START+SELECT,
  persistent bounded session telemetry/summary, isolated save and restoration proof.

Reproduction: `native/g2/build.py` derives the private G1c/P3 renderer, then
`host.py`/`validate.py`; `run_h700.sh` performs smoke/reference with EXIT cleanup.
`package_assets.py` emits a hash-bound coordinate packing plan (no texels or XYZ)
from the existing ROM-only catalogue; `asset_builder.c` decodes original ROMs at
first launch. `assets.py` atomically verifies the compiled cache. `package.py`
assembles the app without any prebuilt cache. Existing production assets are
read-only inputs. No replay-derived recipes are added.

`session.py` spawns the unchanged-priority game before making its supervisor
SCHED_IDLE/nice19. Game logging uses P1b preallocated asynchronous rings; the
supervisor drains anonymous pipes to persistent rolling streams (session-only open adapter duplicates
the inherited profile fd instead of exclusive creation), samples resources
at10Hz and aggregates per state/course. Logs:5×2×2MiB/session, last8sessions;
128state groups/4096exact fallback keys/256recent events. Overflow is counted.
Preparation p99 uses nearest-rank upper0.01ms bins (overflow>100ms); benchmark
analysis remains full precision. Repeats are inferred from swap-return spacing,
not hardware vblank counters; resource attribution uses most recently received
state. Untrappable whole-system failure cannot guarantee an exit summary.

ROM-derived packs, executable binaries, EEPROM contents and packing plan remain
ignored under `native/generated/g2/` and on the device; not included here. Session
EEPROM lines are persisted privately and redacted to hashes in retained logs.

Setup failures retained: initial smoke helper build accidentally treated the C
source as an `-isystem` operand, producing no exported hook; the inherited profile
pipe was also rejected by the existing async writer's O_EXCL pathname open.
The game was stopped, clocks/ES restored; exit95 reports that output-open error,
not a state mismatch. The corrected helper checks its export, and `session_io.c`
duplicates only an exact `/proc/self/fd/<number>` pipe when ENG_G2_SESSION is on.
A host C exclusive-open transport test passes. Corrected smoke is separate;
initial partial telemetry is explicitly incomplete and not performance evidence.
Supervisor tests also cover real inherited-pipe transport, unique session names,
and failed-restoration status91 with recovery journal/lock retained.

Final corrected smoke:7170pictures,7008native/162legacy; event chord at120.012s;
exit0,0incomplete records,0dropped records/output errors,0native repeats,
0settled underruns,0main gaps. One prewarm legacy repeat at160 retained.
Cold asset compilation22.024s; second verified cache hit0.689s. EEPROM unchanged
in attract (0save writes); host changed-data persistence and isolation tests pass.
`final-verification.json` and `final-state.txt`:30app hashes,47protected paths,
launcher hash,private save hash/source,stock restored clocks/RT,ESPID26586,
no native game/supervisor/watchdog. Native Ports entry deliberately remains.
