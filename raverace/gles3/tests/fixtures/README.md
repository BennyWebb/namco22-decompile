# Hash-only acceptance fixtures

Controller input recordings and text metadata are stored in Git at ../recordings/:
reference.rec (12,660 bytes, 6,006 ticks) fixture.rec (214 bytes, 1,700 ticks), and full-race.rec (25,066 bytes, 13,937 ticks).
They contain only F controller events, descriptive comments and input markers.
No EEPROM, checkpoint bodies, images, packet bodies or audio are stored in Git.
GLES3_TEST_RECORDINGS or --recordings overrides the default input directory;
recording SHA256 checks are mandatory for either source.

The factory EEPROM is generated from the user's ROMs by booting with an empty
(all-ones) EEPROM. Its SHA256 must match recordings.json or the test fails loudly;
there is no stored-seed fallback. Runtime sidecars are created in the output
folder from that verified EEPROM and the committed text metadata.
A fresh clone plus the ROMs is sufficient for the complete regression suite:
`python3 raverace/gles3/tests/suite.py --roms /absolute/path/to/roms`.
Build dependencies are listed in [KNULLI_H700.md](../../../docs/KNULLI_H700.md).
The former dense variants had identical input bytes to fixture.rec and are
covered by that single gate. The cone fix is closed; see HISTORY.md.
Checkpoints are represented by SHA256 of each complete original JSONL record;
final summaries use canonical JSON SHA256. sound-reference.jsonl contains only
cumulative PCM/voice hashes and accounting counters, never samples.

reference-commands.csv is the accepted fixed-source normalized packet manifest.
It keeps every command, original order, transform, material and depth; only group
boundaries are removed by expanding page/mode/scissor per visible command.
reference-semantic.csv uses stable ROM catalogue identities, independently gated
by the exact accepted complete packet hashes before being retained. The legacy
semantic manifest records the earlier capture-specific identity normalization;
it is historical evidence, never a substitute acceptance path.

Image fixtures are SHA256 only. Provenance additionally decodes original ROM pens
and the actual palette snapshot; matching image hashes alone does not replace it.

Factory recipe: all 8,192 bytes initially 0xff, idle controls, 600 emulated ticks,
then a cold reset for every test. Generated SHA256:
`08a9266d2a6097e116562a74c0adfe5702f36028d76679bc8297664e8f7bb187`.
The old `a34d3b84…d866` identifies the supplied EEPROM image, not this blank-boot
result. The user approved rebaselining to the generated EEPROM; input SHA256s
remain unchanged. No settings or bytes are copied from the old image.

## Acceptance (2026-10-19)

Fresh clone, fresh build and empty asset cache; ROM directory excluded EEPROM.
The generated EEPROM matched the full SHA256 above on every setup boot.
Reference: 6,006 ticks, 21 checkpoint sets, 4,207 command/semantic receipts and
36 image hashes exact. Fixture: 1,700 ticks, six sets, 1,700 receipts and 18 images
exact. Attract: 36,000 ticks, 120 sets and 20 images exact; zero traps/stops.
ROM provenance: 36 images, 5,529,600 pixels, class (c) = 0. Boot oracle: 159 exact.
Sound: translated == interpreter on all 43,706 ticks across the three cases.
The 600-tick record/replay round trip and malformed/mismatch checks pass;
CTest is 5/5, compilation has zero warnings, and product link/symbol audit passes.
The new-commit history scan allows only the two SHA-pinned controller inputs and
finds no EEPROM, ROM/binary payloads or files over 1 MiB. No device/deployment used.

## Full-race addition

full-race.rec contains only controller events/comments/markers. SHA256:
`b7ea9c4e76e17e0b3e93ebe19659e296397e965d469e5ebaa48adc2ce71b972e`.
It reaches race (10,674 pictures), results, and bonus 5/0 (1,543 pictures).
The original 141 device checkpoint records are retained only as line hashes in
full-race.json. Complete normalized/semantic receipt files and sampled image
manifests are accepted by SHA256; their game-derived bodies remain local.
Sound is compared translated versus oracle on all 13,937 ticks, additionally
pinned by SHA256 of the oracle's complete hash/counter JSONL stream.

This recording uses a second generated seed: start all 8,192 bytes at 0xff;
TEST at tick 1, TEST+SERVICE at 1000, release SERVICE at 1100, release TEST at 1200
to idle; export at tick 1600. The ROM adjustment samples centred
steering/released pedals and its EEPROM writer persists the calibration.
Expected SHA256: `81e19e9ce2fca04dbbd487b12e85cc473e30e61f2797c703743c9b86373721c7`.
No saved bytes are used. The older 08a9266d seed has incompatible analogue zero
points and its reference replay remains a historical determinism gate, not a
full-race acceptance test. Existing fixtures and recordings are unchanged.

Full-race acceptance: generated calibrated EEPROM matched; all 141 device
checkpoint records and final state exact in validation, product and oracle;
13,937 normalized receipts, 13,937 semantic receipts and 36 sampled image hashes
exact on a repeated run. Translated sound equals the oracle on all 13,937 ticks;
zero sound traps, unsupported stops or dropped diagnostic records. Existing
reference/fixture gates remain exact; calibrated record/replay, CTest (5/5) and
product symbol audit pass. Hand-written build sources produced zero warnings.
Only controller text, safe metadata and hash manifests are added; the working-tree
payload/size scan finds no EEPROM, ROM payload or file over 1 MiB. No deployment.

`asset-compiler.sha256` records every product compiler output, including the
local asset-kit metadata. The NumPy host oracle and bounded-memory C/Python
compiler must produce the same complete file set and SHA256s. Timing is logged
outside the content-addressed files (`sizing.json` uses zero for its historical
packing_seconds field). No payload from those files is stored here.
