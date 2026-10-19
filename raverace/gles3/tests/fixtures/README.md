# Hash-only acceptance fixtures

Controller input recordings and text metadata are stored in Git at ../recordings/:
reference.rec (12,660 bytes, 6,006 ticks) and fixture.rec (214 bytes, 1,700 ticks).
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
Build dependencies are listed in ../../../docs/KNULLI_H700.md.
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
