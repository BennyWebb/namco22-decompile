#!/usr/bin/env python3
"""Real-ROM GLES2 texture, eviction and simulation regression (requires GLES2)."""
import json
import pathlib
import re
import subprocess
import sys
import tempfile

gles, headless, roms = (str(pathlib.Path(p).resolve()) for p in sys.argv[1:4])
script = ["--frames", "600", "--coin", "180", "--gas", "200", "--steer", "300:2304"]


def run(binary, arguments):
    return subprocess.run([binary, roms, *arguments], capture_output=True,
                          text=True, timeout=120)


reference = run(headless, script)
assert reference.returncode == 0, reference.stderr
state = json.loads(reference.stdout)
assert state["traps"] == 0 and not state["dsp_fault"] and not state["sound_fault"]

with tempfile.TemporaryDirectory(prefix="rr-texture-test-") as directory:
    images = {}
    boundaries = {}
    upload_counts = {}
    for label, options, budget in (
        ("original", [], 64 * 1024 * 1024),
        ("legacy", ["--vbo-upload", "legacy"], 64 * 1024 * 1024),
        ("legacy-eviction", ["--vbo-upload", "legacy", "--texture-budget-mib", "4"], 4 * 1024 * 1024),
        ("white-batches", ["--render-mode", "white-batches"], 64 * 1024 * 1024),
        ("textured-no-rebind", ["--render-mode", "textured-no-rebind"], 64 * 1024 * 1024),
        ("eviction", ["--texture-budget-mib", "4"], 4 * 1024 * 1024),
        ("diagnostic", ["--diagnostic-colours"], 64 * 1024 * 1024),
    ):
        capture = pathlib.Path(directory) / (label + ".ppm")
        result = run(gles, [*script, "--unpaced", *options,
                           "--capture-frame", "600", "--capture", str(capture)])
        assert result.returncode == 0, result.stderr
        assert json.loads(result.stdout) == state, label + " changed simulation"
        image = capture.read_bytes()
        assert image.startswith(b"P6\n640 480\n255\n") and len(image.split(b"\n", 3)[3]) == 640 * 480 * 3
        images[label] = image
        total = next(line for line in result.stderr.splitlines() if line.startswith("[PROFILE] total "))
        vertex_uploads = float(re.search(r"avg_vertex_uploads=([0-9.]+)", total)[1])
        vertex_bytes = float(re.search(r"avg_vertex_bytes=([0-9.]+)", total)[1])
        triangles = float(re.search(r"avg_triangles=([0-9.]+)", total)[1])
        assert abs(vertex_bytes - triangles * 120) < 7, total
        upload_counts[label] = vertex_uploads
        if label.startswith("legacy"):
            draws = float(re.search(r"avg_draws=([0-9.]+)", total)[1])
            assert abs(vertex_uploads - draws) < 0.1, total
        if label == "original":
            assert vertex_uploads <= 1, total
        if label == "eviction":
            assert int(re.search(r"texture_barriers=(\d+)", total)[1]) > 0, total
        timing = re.search(r"samples=(\d+)", total)
        assert timing and int(timing[1]) == 600, total
        boundaries[label] = re.search(r"flushes texture=(\d+) scissor=(\d+) capacity=(\d+) mutation=(\d+) end=(\d+)", total).groups()
        for name in ("simulation", "scene", "lookup_decode", "uploads", "submission", "swap", "render_other", "vertex_upload", "draw_call"):
            value = float(re.search(r"(?:^| )" + name + r"=([0-9.]+)", total)[1])
            assert value >= 0, (name, total)
        reports = re.findall(r"\[TEX\] decoded=(\d+) uploads=(\d+) GPU=(\d+) peak=(\d+) budget=(\d+).*?evictions=(\d+)", result.stderr)
        assert reports, result.stderr
        for decoded, uploads, resident, peak, reported_budget, evictions in reports:
            assert int(reported_budget) == budget
            assert int(resident) <= int(peak) <= budget
        decoded, uploads, _, _, _, evictions = map(int, reports[-1])
        if label == "diagnostic":
            assert decoded == uploads == 0
        else:
            assert decoded > 0 and uploads > 0
        if label in ("eviction", "legacy-eviction"):
            assert evictions > 0, "Tight-budget test did not exercise eviction"
    assert upload_counts["original"] < upload_counts["legacy"]
    assert images["original"] == images["legacy"], "VBO streaming changed pixels"
    assert images["eviction"] == images["legacy-eviction"], "Cache barrier changed pixels"
    assert boundaries["original"] == boundaries["legacy"], "VBO streaming changed batches"
    assert images["original"] == images["eviction"], "Eviction changed rendered pixels"
    assert images["original"] == images["textured-no-rebind"], "Binding elimination changed pixels"
    assert images["original"] != images["white-batches"], "White-batch mode still samples original textures"
    assert boundaries["original"] == boundaries["white-batches"] == boundaries["textured-no-rebind"], "Draw boundaries differ"
    assert sum(map(int, boundaries["diagnostic"])) < sum(map(int, boundaries["original"])), "Diagnostic mode lost its batching"
    assert images["original"] != images["diagnostic"], "Original textures are not visible"

too_small = run(gles, [*script, "--unpaced", "--texture-budget-mib", "1"])
assert too_small.returncode == 2 and "texture exceeds budget" in too_small.stderr, too_small.stderr
print("Textures, diagnostic/white boundaries, binding elimination, timing, bounded eviction, identical state and budget failure passed")
