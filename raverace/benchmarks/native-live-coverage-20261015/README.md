# Focused M4A host asset coverage — complete, stopped

6006 replay ticks pass;21 authoritative checkpoint sets match. M3 remains
performance-blocked. No H700 transfer/run, commit, push, deployment or4B work.
Read `../../NATIVE_LIVE_INTEGRATION_REPORT.md` for limitations and residency proposal.

Reproduce from repository root with **fresh** output directories (examples below
use shell variables only for path clarity). ROM-derived assets stay ignored.
The original M3 inputs/pack and existing host/cross build objects are prerequisites.

```sh
base="$PWD/raverace/native/generated/milestone4a-coverage"
old="$PWD/raverace/native/generated/milestone3/race-v3"
rec="$PWD/raverace/native/generated/milestone3/inputs/race-20261013-122242-25136.rec"
python3 raverace/native/tools/build_race_capture.py /tmp/rr-m3-headless "$base/capture-build" --first 1 --last 6006
mkdir "$base/capture"
ENG_NATIVE_CAPTURE="$base/capture" "$base/capture-build/race_capture" raverace/extracted --replay-input "$rec" --input-checkpoints "$base/capture.states.jsonl"
python3 raverace/tools/validate_input_replay.py "$rec" "$base/capture.states.jsonl"
python3 raverace/native/tools/audit_live_coverage.py "$base/capture" "$old" "$base/audit"
python3 raverace/native/tools/extend_live_assets.py raverace/extracted "$old" "$base/audit" "$base/race-live-v2"
python3 raverace/native/tools/validate_live_assets.py "$old" "$base/race-live-v2" "$base/audit" raverace/extracted "$base/asset-validation.json"
python3 raverace/native/tools/export_live_lookup.py "$base/race-live-v2" "$base/audit" "$base/lookup"
python3 raverace/native/tools/build_live_prototype.py /tmp/rr-m3-reference-gl raverace/native/generated/milestone4a/coverage-host
mkdir "$base/captures" "$base/commands"
SDL_VIDEODRIVER=offscreen ENG_LIVE_PACK="$base/race-live-v2" ENG_LIVE_SHADERS="$PWD/raverace/native/generated/milestone3/fragment-hud-certified-shaders" ENG_LIVE_LOOKUP="$base/lookup/live.lookup" ENG_LIVE_CAPTURE="$base/captures" ENG_LIVE_COMMAND_DUMP="$base/commands" raverace/native/generated/milestone4a/coverage-host/rr_native_live raverace/extracted --replay-input "$rec" --input-checkpoints "$base/host.states.jsonl" --unpaced --presentation-thread off --audio off --profile-stages --profile-simulation --profile-video --profile-from 1800 > "$base/host.txt" 2>&1
python3 raverace/tools/validate_input_replay.py "$rec" "$base/host.states.jsonl"
python3 raverace/native/tools/validate_live_evidence.py "$old" "$base/commands" "$rec" "$base/host.states.jsonl" "$base/validation"
mkdir "$base/original-captures"
SDL_VIDEODRIVER=offscreen ENG_GPU_HUD=1 ENG_GPU_HUD_VERIFY=1 ENG_GPU_HUD_CAPTURE="$base/original-captures" ENG_GPU_HUD_CAPTURE_TICKS=5655,5867,5874,5878,5889,5921,6006 /tmp/rr-m3-reference-gl/rr_gles2 raverace/extracted --replay-input "$rec" --input-checkpoints "$base/original.states.jsonl" --unpaced --presentation-thread off --audio off > "$base/original-host.txt" 2>&1
python3 raverace/native/tests/check_native_hud_pixels.py "$old" "$base/captures" "$base/hud-validation.json" --live-states "$base/original-captures"
python3 raverace/native/tools/build_live_prototype.py raverace/build/playable-knulli raverace/native/generated/milestone4a/coverage-arm
```

Original source readbacks use bottom-up640x480 RGBA8; review PNGs flip vertically
and place original left/native right. PNGs are persistent under
`raverace/native/generated/milestone4a-coverage/review/`, not temporary files.
All seven comparisons were visually inspected; numeric differences are retained
in `visual-comparison.json`. They do not establish exact original-renderer parity.
Native HUD oracle uses source HUD state from the independent original replay for
new scenes, recorded original M3 HUD state for the old11.

`coverage.json` includes all boot-only misses; no claim that all6006 ticks use
native rendering. `coverage-budget.json` full-content lower bound applies to the
current non-overlapping padded-RGBA rectangle atlas design, not every possible
future texture representation. No residency/streaming rewrite was implemented.
`run-summary.json` identifies the exact binaries/pack/lookup and native maxima.
Expanded pack assets retain original8 page hashes and binary material prefix;
allnew meshes/materials are independently checked against ROM data.

Host unpaced software-GL captures are correctness tests, not hardware performance
or59.904 FPS evidence. No new H700 timing distribution is claimed. The prepared
ARM binary is an untested device prototype, not an approved production executable.
