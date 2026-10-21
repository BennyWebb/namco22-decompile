#!/usr/bin/env python3
"""Deterministic whole-ROM atlas, mesh and material compiler. No replay inputs."""

from pathlib import Path
import argparse, hashlib, json, math, shutil, struct, time
import numpy as np
from rom import ROM, validate_model
from atlas import build

p = argparse.ArgumentParser()
p.add_argument("roms", type=Path)
p.add_argument("output", type=Path)
p.add_argument("--validation", action="store_true")
a = p.parse_args()
out = a.output.resolve()
rom = ROM(a.roms.resolve())
t = time.monotonic()
source_files = sorted(Path(__file__).resolve().parent.glob("*.py")) + sorted(
    (Path(__file__).resolve().parents[1] / "shaders").iterdir()
)
if a.validation:
    source_files += sorted((Path(__file__).resolve().parents[1] / "tests/shaders").iterdir())
compiler_sources = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in source_files if p.is_file()}
if out.exists():
    recipe = json.loads((out / "asset-kit/recipe.json").read_text())
    assert (
        recipe["sources"] == rom.sources and recipe["compiler_sources"] == compiler_sources
    ), "asset compiler/cache version changed; use a fresh output directory"
    for name, v in recipe["files"].items():
        assert hashlib.sha256((out / name).read_bytes()).hexdigest() == v["sha256"], name
    print("verified ROM compiler cache", out)
    raise SystemExit(0)
out.mkdir(parents=True, exist_ok=False)
encode = lambda x: (json.dumps(x, sort_keys=True, separators=(",", ":")) + "\n").encode()
models = []
recipes = set()
geometry = []
meshes = []
inventory = []
nexttexel = 0
for code in range(69, 2490):
    if rom.word(code) < 0:
        continue
    m = rom.model(code)
    validate_model(m)
    models.append(code)
    for q in m["quads"]:
        verts = np.asarray(m["vertices"][q["first_vertex"] : q["first_vertex"] + 4], dtype=np.int32)
        u = verts[:, 3]
        v = verts[:, 4]
        b, g, c = q["material"]
        r = (b, g, c, int(u.min()), int(v.min()), int(u.max() - u.min() + 1), int(v.max() - v.min() + 1))
        recipes.add(r)
        data = np.zeros((8, 4), "<f4")
        data[::2, :3] = verts[:, :3]
        data[1::2, :2] = verts[:, 3:5]
        geometry.append(data)
        packet = q["packet_address"] + 1 + (3 if rom.word(q["packet_address"]) == 23 else 4)
        meshes.append({"code": code, "packet": packet, "texel": nexttexel})
        nexttexel += 8
        inventory.append({"code": code, "packet": packet, "recipe": r, "lighting_packets": q["lighting_packet_count"]})
assert len(models) == 2421 and len(meshes) == 97116
sizing, mapping, pages = build(rom, {(r[0], *r[2:]) for r in recipes}, out, "whole-ROM")
mp = {tuple(m["recipe"]): m for m in mapping}
dims = sizing["page_dimensions"]
height = math.ceil(nexttexel / 1024)
geo = np.zeros((height * 1024, 4), "<f4")
geo[:nexttexel] = np.concatenate(geometry)
(out / "meshes.f32").write_bytes(geo.tobytes())
from finish import finish

finish(rom, meshes, recipes, nexttexel, sizing, mapping, out, source_files, compiler_sources)
print("compiled", len(meshes), "meshes in", time.monotonic() - t, "seconds")
