# Rave Racer implementation history and private archive

The full feature/knulli-h700 history is intentionally absent from this branch's
ancestry. This branch starts at main0837a39 and copies accepted file contents.
The private feature tip is fdfd35014f609d8ffddd5b9a18b53d1a873d7e24.

Early GLES2 H700 work explored ordered submission, texture arrays/world atlas,
four-texture batching, predecode, GPU prewarm and P-series priority variants.
Rejected configurations and their captures/probes are private evidence, not product.
The old playable-knulli preset, RR_GLES2 viewer and knulli-gles-test are retired.
Desktop rr, packages, shared interfaces and other games remain independent.

The native renderer progressed through p1/p1b/p2/p3, B1/B2, index/provenance,
whole-ROM assets, bounded caches/staging, G2, then native-only N1. N1 renders
picture1, removes legacy fallback/overload/cache composition and strictly stops on
unsupported commands. Its final reference/attract gates had pacing0/0/0.
The sound CECC gap came from pushed-pointer/RTS dispatch, not CEC9 fall-through;
ROM-resolved CE5B/CEF4 now seed the single strict translation. Interpreter miss
fallback was rejected for shipped builds; the interpreter remains a test oracle.

Cone Fix A preserves protected geometry and batches only adjacent identical-state
instances. State5/0 max draws1480→18 (tick11936:17), CPU p9965.807→3.442ms,
repeats1545→0, missing settled samples1178356→0 and gaps>50ms451→0.
Host/Mali images and checkpoint state remained exact. The cone fix is validated
and closed. Its recording requires a non-default saved EEPROM, so the public
regression suite no longer carries the cone entry or its hash fixtures; the
private archive retains the original evidence. Compiler tail remains known.
Source of truth: deployed N1 normal SHA256
`a624fe906556c5625ec26ba5e3749d7f25c8cc0ae09fd39241ad97aad338d029`;
recording SHA256 `6008dd12d5d064a98d9a3b05ded1facd5124b90c40b7df09eac0d52cfc970556`.
Installed app: `/userdata/system/rave-racer-native-g2/`; recording app:
`/userdata/system/rave-racer-native-record-20261018/`. Saves:
`/userdata/saves/rave-racer-native/`; preserve its rr_eeprom.nv for later migration.
No cleanup test replaces those apps or their Native Ports entries.

Offline restricted archive: `/home/bwebb/rave-racer-archive/20261019-fdfd350/`.
Git bundle contains all22 pre-cleanup refs and full feature ancestry; verified.
Benchmark tarball includes ROM-derived captures. Regression tarball includes
native/generated/regressions, the reference inputs, fixed/pre-fix cone goldens,
N1 reference/provenance and G2 reference/provenance. These archives must remain
private: never push them or attach unsanitized bundles to public releases.

SHA256 (absolute paths are part of the saved SHA256SUMS file):

```text
d786f8ada93055a404214564a82424126d0ffcf7b5682f907601ce163185b9b9  /home/bwebb/rave-racer-archive/20261019-fdfd350/feature-all-refs.bundle
5d14809a40df060828a4d06119f535dfa7bcf9acc20dd1d90fcae42a157866c1  /home/bwebb/rave-racer-archive/20261019-fdfd350/benchmarks.tar.gz
6637c0d0020a6a609856da775c460499e3949507afe546b5acd6f58f32057da6  /home/bwebb/rave-racer-archive/20261019-fdfd350/regressions-and-goldens.tar.gz
```

Pre-existing main images/icons, engine/c25/c71_bios.inc and font_data.h were left
unchanged. History hygiene reports inherited large translated source separately
from newly introduced files. No feature benchmark/recording/payload ancestry is
carried by this branch. Full validation and commit inventory: CLEANUP_REPORT.md.

## Controller-only regression fixtures

Reference (6,006 ticks, 12,660 bytes) and fixture (1,700 ticks, 214 bytes) inputs
are now committed as reviewed ASCII controller events and safe text metadata.
The user approved a blank-EEPROM rebaseline after the 600-tick all-ones boot
produced `08a9266d2a6097e116562a74c0adfe5702f36028d76679bc8297664e8f7bb187`,
rather than the supplied image's `a34d3b84…d866`. Every replay cold-boots with
that generated EEPROM. No EEPROM payload or stored-seed fallback is included.
The retained hash-only checkpoints, commands, images and sound oracle reference
are rebaselined for that reset state. Old results above describe their original
seed. The public cone gate is retired; its fix remains validated and closed.

## Calibrated full-race regression

The 600-tick blank boot leaves analogue zero points incompatible with the
emulated cabinet. Its reference hashes prove determinism but not a completed
race. Preserve those earlier tests; add full-race.rec (13,937 ticks, 25,066
bytes, input SHA256 b7ea9c4e…71b972e). Generate its calibrated seed from all-ones
EEPROM through the ROM TEST/SERVICE adjustment and EEPROM writer, exporting at
1,600 ticks; expected SHA256 81e19e9c…73721c7. No saved EEPROM bytes are stored.
The user capture covers 10,674 race pictures, results and 1,543 bonus 5/0
pictures, with device pacing 0/0/0. All 141 device checkpoint records match host
translated and interpreter runs; sound agrees on every tick. Retain only hashes
of checkpoints, packet/image manifests and oracle audio accounting. The old
standalone cone recording remains excluded; this new input covers that stage.
