#!/usr/bin/env python3
"""Compare replay checkpoints with the human recording; clocks are separate.
Usage: validate_input_replay.py RECORDING.rec PLAYBACK.states.jsonl
"""

import json
import pathlib
import sys

source = pathlib.Path(sys.argv[1] + ".states.jsonl")
expected = [json.loads(s) for s in source.read_text().splitlines()]
actual = [json.loads(s) for s in pathlib.Path(sys.argv[2]).read_text().splitlines()]
# A quit can occur within the following emulated tick. Only completed recording
# boundaries are compared, never a partial-tick shutdown state or an invented oracle.
frames = {s["frames"] for s in expected if "frames" in s}
assert frames, "Recording too short: no 300-tick checkpoint; record at least five seconds"


def selected(rows):
    result = {}
    for r in rows:
        frame = r.get("frames", r.get("board_frame", r.get("audio_frame")))
        if frame in frames:
            key = (frame, "simulation" if "frames" in r else "board" if "board_frame" in r else "audio")
            assert key not in result, f"Duplicate checkpoint {key}"
            result[key] = r
    return result


reference = selected(expected)
observed = selected(actual)
assert len(reference) == len(frames) * 3, "Missing recording simulation/board/audio checkpoint"
assert reference.keys() == observed.keys(), "Missing playback checkpoints"
for key, expected_row in reference.items():
    assert expected_row == observed[key], f"Divergence at {key}: {expected_row} != {observed[key]}"
print(
    f"PASS: {len(frames)} completed checkpoints, through tick {max(frames)}; CPU/RAM, board/video RAM, raw PCM and C352 voice hashes identical"
)
