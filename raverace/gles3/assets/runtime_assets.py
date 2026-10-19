#!/usr/bin/env python3
"""First-launch ROM compiler, standard-library only; atomic hash-verified cache."""

import pathlib, hashlib, json, subprocess, time, shutil, os, sys


def digest(p):
    h = hashlib.sha256()
    with p.open("rb") as f:
        for b in iter(lambda: f.read(1048576), b""):
            h.update(b)
    return h.hexdigest()


def ensure(app, rom, log):
    recipe = json.loads((app / "asset-kit/recipe.json").read_text())
    plan = app / "asset-kit/layout.bin"
    assert digest(plan) == recipe["plan_sha256"]
    print("[ASSETS] checking original ROM hashes", file=log, flush=True)
    for name, v in recipe["sources"].items():
        assert (rom / name).stat().st_size == v["bytes"] and digest(rom / name) == v["sha256"], name
    key = hashlib.sha256(
        json.dumps(dict(recipe=recipe, builder=digest(app / "rr_asset_builder")), sort_keys=True).encode()
    ).hexdigest()
    out = app / "cache" / key
    manifest = out / "cache.json"
    if manifest.exists():
        j = json.loads(manifest.read_text())
        assert j["key"] == key
        for n, v in recipe["files"].items():
            assert (out / n).stat().st_size == v["bytes"] and digest(out / n) == v["sha256"], n
        print("[ASSETS] verified cache hit " + key, file=log, flush=True)
        return out, False
    out.parent.mkdir(exist_ok=True)
    temp = out.with_name(key + ".building")
    if temp.exists():
        shutil.rmtree(temp)
    temp.mkdir()
    t = time.monotonic()
    print("[ASSETS] first launch: compiling atlas pens and meshes from your ROMs", file=log, flush=True)
    subprocess.run([str(app / "rr_asset_builder"), str(rom), str(plan), str(temp)], stdout=log, stderr=log, check=True)
    for p in (app / "asset-kit/template").iterdir():
        shutil.copyfile(p, temp / p.name)
    for n, v in recipe["files"].items():
        assert (temp / n).stat().st_size == v["bytes"] and digest(temp / n) == v["sha256"], n
    (temp / "cache.json").write_text(
        json.dumps(
            dict(key=key, roms=recipe["sources"], files=recipe["files"], build_seconds=time.monotonic() - t), indent=2
        )
        + "\n"
    )
    os.replace(temp, out)
    print("[ASSETS] cache complete in %.3fs" % (time.monotonic() - t), file=log, flush=True)
    return out, True


if __name__ == "__main__":
    print(ensure(pathlib.Path(sys.argv[1]), pathlib.Path(sys.argv[2]), sys.stdout))
