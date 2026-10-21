#!/usr/bin/env python3
"""Bounded-memory whole-ROM compiler. Python orchestrates; C streams texels/geometry."""

import argparse
import array
import ctypes as C
import hashlib
import json
import math
import os
from pathlib import Path
import time
import resource
from finish import finish


def encode(value):
    return (json.dumps(value, sort_keys=True, separators=(",", ":")) + "\n").encode()


def digest(path):
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1048576), b""):
            h.update(chunk)
    return h.hexdigest()


def progress(stage, percent):
    print(f"[BUILD] {stage} {percent}%", flush=True)
    if os.environ.get("RR_BUILD_PROGRESS"):
        target = Path(os.environ["RR_BUILD_PROGRESS"])
        temporary = target.with_suffix(".new")
        temporary.write_text(f"Building assets: {stage} {percent}%\nPlease wait. START+SELECT cancels.")
        temporary.replace(target)


class Compiler:
    def __init__(self, library, roms):
        self.c = C.CDLL(str(library))
        self.c.rr_codec_init.argtypes = [C.c_char_p]
        self.c.rr_codec_words.restype = C.POINTER(C.c_int32)
        self.c.rr_codec_init(os.fsencode(roms))
        self.words = self.c.rr_codec_words()
        # Match the host ROM object's source catalogue exactly.
        self.sources = {}
        names = (
            [f"rv1pot{s}{i}.{5-i}{e}" for s, e in [("l", "b"), ("m", "c"), ("u", "d")] for i in range(4)]
            + [f"rv1cg{i}.{s}" for i, s in enumerate(["1a", "1c", "1d", "1e", "1f", "1j", "1k", "1n"])]
            + ["rv1ccrl.5a", "rv1ccrh.5c"]
        )
        for name in names:
            p = roms / name
            self.sources[name] = dict(bytes=p.stat().st_size, sha256=digest(p))

    def word(self, at):
        if not 0 <= at < 0x200000:
            raise ValueError(f"point address outside ROM: {at:#x}")
        return self.words[at]

    def rect(self, r):
        return (C.c_int * 6)(*r)

    def hash_rect(self, r):
        result = (C.c_ubyte * 32)()
        self.c.rr_codec_digest(self.rect(r), result)
        return bytes(result).hex()

    def equal(self, a, b):
        if not self.c.rr_codec_equal(self.rect(a), self.rect(b)):
            raise ValueError("Digest collision")

    def models(self):
        meshes, recipes = [], set()
        count = 0
        for code in range(69, 2490):
            cursor = self.word(code)
            if cursor < 0:
                continue
            count += 1
            for _ in range(4096):
                chunk = self.word(cursor)
                cursor += 1
                if chunk < 0:
                    if chunk != -1:
                        raise ValueError(f"model {code}: invalid terminator")
                    break
                size = self.word(chunk)
                if not 0 <= size <= 256:
                    raise ValueError(f"model {code}: invalid chunk length")
                pos, end = chunk + 1, chunk + 1 + size
                while pos < end:
                    length = self.word(pos)
                    base = pos + 1
                    if length not in (23, 24, 16, 13) or base + length > end:
                        raise ValueError(f"model {code}: invalid packet at {pos}")
                    if length in (23, 24):
                        at = base + (3 if length == 23 else 4)
                        uv = [self.word(at + i) & 4095 for i in range(8)]
                        u, v = uv[::2], uv[1::2]
                        recipes.add(
                            (
                                (self.word(at + 1) >> 12) & 15,
                                (self.word(base + 2) >> 8) & 127,
                                (self.word(at) >> 12) & 15,
                                min(u),
                                min(v),
                                max(u) - min(u) + 1,
                                max(v) - min(v) + 1,
                            )
                        )
                        meshes.append(dict(code=code, packet=at, texel=len(meshes) * 8))
                    early_end = self.word(base) & 0x800000
                    pos = base + length
                    if early_end:
                        break
            else:
                raise ValueError(f"model {code}: unterminated list")
            if code % 100 == 0:
                progress("models", (code - 69) * 100 // 2421)
        if count != 2421 or len(meshes) != 97116:
            raise ValueError("model catalogue incomplete")
        progress("models", 100)
        return meshes, recipes

    def atlas(self, recipes, out):
        initial, aliases, reps = {}, {}, []
        bytecount = exact_hits = 0
        ordered = sorted(recipes)
        for at, r in enumerate(ordered):
            if not (0 < r[4] <= 4092 and 0 < r[5] <= 4092):
                raise ValueError(f"invalid footprint {r}")
            key = self.hash_rect(r)
            if key in initial:
                index = initial[key]
                self.equal(reps[index], r)
                exact_hits += 1
            else:
                index = len(reps)
                reps.append(r)
                initial[key] = index
                bytecount += (r[4] + 4) * (r[5] + 4)
            aliases[r] = index
            if at % 200 == 0:
                progress("decode", at * 100 // len(ordered))
        progress("decode", 100)
        n = len(reps)
        rectangles = (C.c_int * (n * 12))()
        rectangles[: n * 6] = [v for r in reps for v in r]
        parents = (C.c_int * (n * 2))()
        savings = (C.c_int * n)()
        progress("merge", 0)
        merges = self.c.rr_codec_merge(rectangles, n, parents, savings)
        roots = {i: tuple(rectangles[i * 6 : i * 6 + 6]) for i in range(n + merges) if parents[i] == i}

        def root(i):
            while parents[i] != i:
                i = parents[i]
            return i

        progress("merge", 100)
        contents, rootkeys = {}, {}
        for i, r in roots.items():
            key = self.hash_rect(r)
            if key in contents:
                self.equal(contents[key], r)
            else:
                contents[key] = r
            rootkeys[i] = key
        keys = sorted(contents)
        sizes = (C.c_int * (len(keys) * 2))(*[v for k in keys for v in (contents[k][4] + 4, contents[k][5] + 4)])
        keybytes = b"".join(bytes.fromhex(k) for k in keys)
        placements = (C.c_int * (len(keys) * 5))()
        dimensions = (C.c_int * 32)()
        progress("pack", 0)
        npages = self.c.rr_codec_pack(len(keys), sizes, keybytes, placements, dimensions)
        dims = [list(dimensions[p * 2 : p * 2 + 2]) for p in range(npages)]
        placement = {k: list(placements[i * 5 : i * 5 + 5]) for i, k in enumerate(keys)}
        for p, (w, h) in enumerate(dims):
            with (out / f"page-{p}.r8ui").open("wb") as f:
                f.truncate(w * h)
        progress("pack", 100)
        for at, (key, r) in enumerate(contents.items()):
            p, x, y, _, _ = placement[key]
            self.c.rr_codec_place(os.fsencode(out / f"page-{p}.r8ui"), *dims[p], self.rect(r), x, y)
            if at % 25 == 0:
                progress("pages", at * 100 // len(contents))
        progress("pages", 100)
        mappings, checked = [], 0
        for at, (r, i) in enumerate(sorted(aliases.items())):
            original = reps[i]
            merged = roots[root(i)]
            p, x, y, w, h = placement[rootkeys[root(i)]]
            ox, oy = x + original[2] - merged[2], y + original[3] - merged[3]
            mappings.append(
                dict(
                    recipe=r, page=p, rect=[ox, oy, r[4] + 4, r[5] + 4], merged_recipe=merged, canonical_recipe=original
                )
            )
            self.c.rr_codec_verify(os.fsencode(out / f"page-{p}.r8ui"), *dims[p], self.rect(r), ox, oy)
            checked += (r[4] + 4) * (r[5] + 4)
            if at % 200 == 0:
                progress("verify-pens", at * 100 // len(aliases))
        progress("verify-pens", 100)
        files = {
            f"page-{p}.r8ui": dict(bytes=w * h, sha256=digest(out / f"page-{p}.r8ui")) for p, (w, h) in enumerate(dims)
        }
        sizing = dict(
            label="whole-ROM",
            recipes=len(aliases),
            unique_initial_contents=len(reps),
            exact_duplicate_hits=exact_hits,
            initial_padded_bytes=bytecount,
            overlap_merges=merges,
            merged_rectangles=len(roots),
            merge_saved_padded_bytes=sum(savings[:merges]),
            unique_final_contents=len(contents),
            page_count=len(dims),
            page_dimensions=dims,
            allocated_bytes=sum(w * h for w, h in dims),
            full_4096_page_bytes=len(dims) * 4096**2,
            content_bytes=sum((r[4] + 4) * (r[5] + 4) for r in contents.values()),
            packed_pen_texels_verified=checked,
            packing_seconds=0,
            files=files,
        )
        (out / "mapping.json").write_bytes(encode(mappings))
        (out / "sizing.json").write_bytes(encode(sizing))
        return sizing, mappings


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("roms", type=Path)
    p.add_argument("output", type=Path)
    p.add_argument("--codec", type=Path, required=True)
    a = p.parse_args()
    started = time.monotonic()
    out, here = a.output.resolve(), Path(__file__).resolve().parent
    out.mkdir(parents=True, exist_ok=False)
    progress("models", 0)
    compiler = Compiler(a.codec.resolve(), a.roms.resolve())
    meshes, recipes = compiler.models()
    sizing, mapping = compiler.atlas({(r[0], *r[2:]) for r in recipes}, out)
    progress("geometry", 0)
    pairs = (C.c_uint32 * (len(meshes) * 2))(*[v for m in meshes for v in (m["packet"], m["texel"])])
    compiler.c.rr_codec_meshes(os.fsencode(out / "meshes.f32"), pairs, len(meshes), math.ceil(len(meshes) * 8 / 1024))
    sources = sorted(here.glob("*.py")) + sorted((here.parent / "shaders").iterdir())
    identities = {p.name: digest(p) for p in sources if p.is_file()}
    finish(compiler, meshes, recipes, len(meshes) * 8, sizing, mapping, out, sources, identities)
    progress("geometry", 100)
    progress("complete", 100)
    print(
        f"[BUILD-RESOURCE] maxrss_kib={resource.getrusage(resource.RUSAGE_SELF).ru_maxrss} seconds={time.monotonic()-started:.6f}",
        flush=True,
    )


if __name__ == "__main__":
    main()
