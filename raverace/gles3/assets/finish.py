"""Shared deterministic metadata emission for host and streaming compilers."""

import hashlib
import json
import math
import shutil
import struct


def finish(rom, meshes, recipes, nexttexel, sizing, mapping, out, source_files, compiler_sources):
    encode = lambda x: (json.dumps(x, sort_keys=True, separators=(",", ":")) + "\n").encode()
    mp = {tuple(m["recipe"]): m for m in mapping}
    dims = sizing["page_dimensions"]
    height = math.ceil(nexttexel / 1024)
    (out / "meshes.json").write_bytes(encode(meshes))
    materials = []
    matbytes = bytearray()
    groups = bytearray()
    lookup = []
    meta = bytearray(1024 * math.ceil((len(recipes) + 512) / 1024) * 32)
    for i, r in enumerate(sorted(recipes)):
        b, g, c, u, v, w, h = r
        m = mp[(b, c, u, v, w, h)]
        page = m["page"]
        x, y, _, _ = m["rect"]
        pw, ph = dims[page]
        uv = [1 / pw, 1 / ph, (x + 2.5 - u) / pw, (y + 2.5 - v) / ph]
        materials.append({"id": i, "recipe": r, "page": page, "rect": m["rect"], "uv": uv})
        matbytes += struct.pack("<4fI12x", *uv, page)
        groups += struct.pack("<I", g)
        lookup.append(struct.pack("<8i", *r, i))
        struct.pack_into("<8i", meta, i * 32, b, u, v, x + 2, y + 2, w, h, g)
    assert len(materials) <= 65535
    (out / "materials.json").write_bytes(encode(materials))
    (out / "materials.bin").write_bytes(matbytes)
    (out / "groups.bin").write_bytes(groups)
    (out / "provenance-mapping.i32").write_bytes(meta)
    (out / "gles3.lookup").write_bytes(
        struct.pack("<4I", 0x4B4C314E, 1, len(meshes), len(materials))
        + b"".join(
            struct.pack("<3i", m["code"], m["packet"], m["texel"])
            for m in sorted(meshes, key=lambda m: (m["code"], m["packet"]))
        )
        + b"".join(lookup)
    )
    (out / "atlas.meta").write_text(str(len(dims)) + "\n" + "".join(f"{w} {h}\n" for w, h in dims))
    (out / "gles3.meta").write_text(f"GLES3 {len(dims)} 1024 {height} {len(materials)}\n")

    # Coordinate-only plan is generated locally, not distributed through Git.
    rects = {}
    for x in mapping:
        r = x["merged_recipe"]
        c = x["canonical_recipe"]
        page = x["page"]
        ox = x["rect"][0] - (c[2] - r[2])
        oy = x["rect"][1] - (c[3] - r[3])
        key = (page, ox, oy, r[4], r[5])
        rects.setdefault(key, (*r, page, ox, oy))
    plan = (
        struct.pack("<5I", 0x31503247, len(rects), len(meshes), height, len(dims))
        + b"".join(struct.pack("<2I", *d) for d in dims)
        + b"".join(struct.pack("<9I", *r) for k, r in sorted(rects.items()))
        + b"".join(struct.pack("<2I", m["packet"], m["texel"]) for m in meshes)
    )
    kit = out / "asset-kit"
    kit.mkdir()
    (kit / "layout.bin").write_bytes(plan)
    template = kit / "template"
    template.mkdir()
    files = {}
    for name in [
        "atlas.meta",
        "gles3.meta",
        "gles3.lookup",
        "materials.bin",
        "groups.bin",
        "provenance-mapping.i32",
        "meshes.f32",
    ] + [f"page-{i}.r8ui" for i in range(len(dims))]:
        q = out / name
        files[name] = dict(bytes=q.stat().st_size, sha256=hashlib.sha256(q.read_bytes()).hexdigest())
        if name != "meshes.f32" and not name.startswith("page-"):
            shutil.copyfile(q, template / name)
    for q in source_files:
        if q.suffix not in (".frag", ".vert"):
            continue
        shutil.copyfile(q, template / q.name)
        shutil.copyfile(q, out / q.name)
        files[q.name] = dict(bytes=q.stat().st_size, sha256=hashlib.sha256(q.read_bytes()).hexdigest())
    (kit / "recipe.json").write_bytes(
        encode(
            dict(
                format="gles3-rom-cache-1",
                compiler_sources=compiler_sources,
                sources=rom.sources,
                files=files,
                plan_sha256=hashlib.sha256(plan).hexdigest(),
            )
        )
    )
    (out / "materials.json").write_bytes(encode(materials))
