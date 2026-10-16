# G2 — bounded cache and separate playable Native build

| Item | Result |
|---|---|
| Part1 reference preparation | **PASS:8.305ms median**, target≤8.54ms; G1c9.032ms |
| Race pacing:repeats / settled underruns / main gaps>50ms | **0 / 0 / 0** |
| Dense preparation median / p99 | 12.227 / 12.735ms |
| Host correctness | All21 checkpoint sets,4207 packets,36 images; provenance(c)=0 |
| Material cache | 131072 cells,512KiB; exact keys,65535-entry pool,64-probe bound |
| CPU pending copies | 27.56MB actual vs1.215GB pending; **zero race-window copies** |
| Playable package | Separate **Rave Racer (Native)** Ports entry; production retained |
| First-launch assets | ROM-built on handheld; cold compilation22.024s |
| Ports smoke / final integrity | **PASS:**7170pictures,START+SELECT,47protected paths exact |

## Part1: recovered preparation cost

Started clean `feature/knulli-h700`, HEAD `588fade914a295c714f08ecaa3733533825c00e0`.
Only private `native/g2/`, evidence and documentation changed; no commit/push.
Cache cells shrink32MiB→512KiB; exact-key pool remains2097120bytes.
Generation reset on pool exhaustion/64 probes, wrap clearing and counters remain.
Reference peak occupancy10652,1.04287probes/lookup,max4; zero resets/evictions.
Forced-collision/pool/wrap test passes400002lookups.

Each slot retains its immutable packed prefix. Unchanged entry,generation,offset
and bounds reuse bytes; after the first change, every following region is recopied.
Dynamic material metadata and per-resource-copy GL UV metadata update only on
exact change. Ownership/layout/generation stress passes; no live GL reads added.
Across5847 prepared pictures:27559504pen bytes and171840metadata bytes copied;
race1800–6006 both0. Old pending bytes1215043472 count demand, not actual copies.
The two changes are measured together; their individual timing contributions are
not isolated. No compiler algorithm/geometry/physics/audio changes.

Host colour/provenance runs each complete6006ticks:21sets,4207adjusted/raw packets
exact. All36 colour images match G1c; comparison with established RGBA provenance
has6802adjacent-boundary residuals, classes(a/b/c)=0/6802/0. No fidelity gate waived.
Standalone ROM compiler reproduces all7pen pages and mesh buffer byte-for-byte.

One ARM smoke to1900 confirms actual default nice0/policy0,queue2,index path,
3072² dynamic rotation and smaller cache. Exactly one full reference timing:
fixedCPU1512MHz,stockGPU240MHz,strictFIFO/default priorities;21sets pass.
Play build performs no hashing; packets are proven in separate host validation.
All samples retained; native1800–6006,dense3550–3590,nearest-rank tails.

| Race metric,ms | Median | p95 | p99 | Worst |
|---|---:|---:|---:|---:|
| Preparation | 8.305 | 10.665 | 12.369 | 13.645 |
| Compiler | 5.391 | 7.018 | 8.220 | 8.849 |
| Swap interval | 16.746 | 16.832 | 17.045 | 19.135 |
| Snapshot→swap | 33.352 | 33.457 | 33.603 | 35.773 |

Legacy release tick178; settled audio thereafter and race audio both0.
Race peak RSS320.723MiB,Mali176.395MiB; minimum MemAvailable544.355MiB,
CmaFree9.230MiB. Peak56.924°C; no sampled clock/cooling flags.
This is one timing run, not a repeatability or soak claim.

## Launch, assets and private saves

Choose **Ports → Rave Racer (Native)**. START+SELECT exits through the existing
controller handler. Production “Rave Racer” remains available separately.
Installed app:`/userdata/system/rave-racer-native-g2/`; native binary SHA256
`74d00920781c9ea03a1bf9852d5a8da635ec0b360527d8c76be894c979ab40ca`.
Session profile:CPU1512MHz,stockGPU,queue2,index textures,state activation,
legacy safety fallback,valve/affinity/priority overrides OFF.
Independent watchdog plus normal cleanup restores saved governors/bounds after
exit or supervisor/game failure. Thermal policy remains unchanged.

First launch verifies original ROM hashes, compiles pens/vertices using a shipped
ROM-bound coordinate packing plan, then verifies every output hash and atomically
publishes `cache/<hash>/`. The plan contains placements/indices, not texels/XYZ;
no recordings or replay-discovered recipes are used. Progress is logged to the
session’s `assets.log`. Later launches hash-check and load that cache.

Native saves:`/userdata/saves/rave-racer-native/rr_eeprom.nv`.
First launch copies production EEPROM if present, otherwise original8192-byte
`rv1eeprm.9e` seed (production headless entry currently has no persisted EEPROM).
Existing controller/config files are copied once. All game working files use the
native save directory; production ROM/config/save paths are never written.
Changed EEPROM is snapshotted to the existing async memory transport every120ticks
and on normal exit; supervisor persists atomically. A crash can lose≤120ticks of
EEPROM changes. This is host persistence adaptation, not emulated-state logic.

## Session telemetry and rollback

Persistent logs:`/userdata/system/rave-racer-native-g2/sessions/<date-pid>/`.
Human/JSON summaries:`latest-summary.txt` and `latest-summary.json` in the app.
Per state/course: native/legacy counts,reasons,exact fallback keys,repeats,
settled audio gaps,main gaps>50ms,prep p99,dynamic churn/copies and resource ranges.
Resources sampled10Hz:Mali pages,RSS/HWM,MemAvailable,CmaFree,CPU/GPU frequencies,
temperatures/cooling. Asynchronous game writer and SCHED_IDLE/nice19 supervisor;
no game-thread disk logging, no diagnostic output under/tmp.
Five rolling streams×2×2MiB/session,last8sessions;128state groups,4096fallback
keys,256recent events,overflow counts. Summary p99 uses upper0.01ms bins.
Repeats are inferred from swap-return intervals, not hardware vblank counters.
Resource state attribution follows the latest received coverage receipt.

One-step removal:
`/userdata/system/rave-racer-native-g2/remove.sh`
Removes only the separate Ports entry; retains native saves/cache/evidence.
Refresh Ports afterwards. No production rollback is needed.

## Verification and stop

See [raw evidence/methods](benchmarks/native-g2-20261016/README.md).
Corrected scripted smoke:7170pictures,7008native/162legacy; START+SELECT at120s,
exit0,zero incomplete records/dropped logs/output errors. One boot/prewarm repeat
at160; native repeats,settled underruns and main gaps all0; clock/cooling flags0.
Verified cache-hit startup0.689s. EEPROM unchanged during attract; copied8192-byte
save exists and host tests exercise changed-data persistence/production isolation.
Initial setup attempt retained: empty input-hook library and exclusive pipe-open
error caused incomplete telemetry/exit95; fixed adapters, not gameplay changes.
All30app hashes and47protected production paths exact; production SHA256 remains
`6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`.
All4CPU governors/bounds restored,schedutil480000–1512000; GPUstock restored.
EmulationStation PID26586; no game/watchdog left active. No commit/push.
**COMPLETE / STOPPED.** Further coverage or optimisation requires authorisation.
