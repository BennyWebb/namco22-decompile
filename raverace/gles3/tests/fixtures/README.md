# Hash-only acceptance fixtures

Inputs/EEPROM/recordings and image or packet bodies are never stored here.
Set GLES3_TEST_RECORDINGS or pass --recordings. Recording SHA256 is mandatory.
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
