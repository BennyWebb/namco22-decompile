#!/usr/bin/env python3
"""Audit reachable history and source tree; inherited main artifacts stay unchanged."""

from pathlib import Path
import argparse, subprocess, json, re

p = argparse.ArgumentParser()
p.add_argument("--base", default="0837a39")
p.add_argument("--output", type=Path, required=True)
a = p.parse_args()
root = Path(__file__).resolve().parents[3]


def git(*args):
    return subprocess.check_output(["git", *args], cwd=root)


def tree(ref):
    out = {}
    for line in git("ls-tree", "-rl", ref).decode().splitlines():
        meta, path = line.split("\t", 1)
        mode, kind, oid, size = meta.split()
        out[path] = (oid, int(size))
    return out


base = tree(a.base)
head = tree("HEAD")
commits = git("rev-list", a.base + "..HEAD").decode().splitlines()
introduced = {}
violations = []
for commit in commits:
    for path, (oid, size) in tree(commit).items():
        if path in base and base[path][0] == oid:
            continue
        introduced.setdefault(path, (oid, size))  # rev-list visits newest versions first
        if "/benchmarks/" in "/" + path or "/native/" in "/" + path or "knulli-gles-test" in path:
            violations.append((commit, path, "retired/evidence path"))
        if Path(path).suffix.lower() in (
            ".rgba",
            ".r8ui",
            ".f32",
            ".vertices",
            ".indices",
            ".rec",
            ".tgz",
            ".png",
            ".jpg",
            ".wav",
            ".pcm",
            ".bin",
        ):
            violations.append((commit, path, "payload extension"))
        if size > 1024 * 1024:
            violations.append((commit, path, "new blob over1MiB"))
        if path.startswith(
            ("raverace/gles3/", "raverace/platform/knulli-h700/", "raverace/docs/")
        ) and not path.endswith(("/HISTORY.md", "/hygiene.py")):
            body = git("cat-file", "blob", oid)
            if re.search(rb"ENG_(?:NATIVE|N1|P1B?|G2)_|RR_GLES2\b|rr_native\b|native/generated", body):
                violations.append((commit, path, "retired runtime dependency/name"))
# The audit tool quotes rejected names solely as blacklist patterns.
# Inspect every reachable historical path as well; report inherited findings.
all_objects = git("rev-list", "--objects", "HEAD").decode().splitlines()
historical_payloads = []
for row in all_objects:
    if " " not in row:
        continue
    oid, path = row.split(" ", 1)
    if "/benchmarks/" in "/" + path or Path(path).suffix.lower() in (
        ".rgba",
        ".r8ui",
        ".f32",
        ".vertices",
        ".rec",
        ".tgz",
        ".wav",
        ".pcm",
    ):
        historical_payloads.append(dict(path=path, oid=oid))
# Main artwork, font and BIOS are intentionally unchanged.
protected = [
    x
    for x in base
    if x.startswith("docs/images/") or x.endswith(".png") or x.endswith("/c71_bios.inc") or x.endswith("/font_data.h")
]
assert all(head.get(x) == base[x] for x in protected)
object_ids = [row.split(" ", 1)[0] for row in all_objects]
meta = (
    subprocess.check_output(
        ["git", "cat-file", "--batch-check=%(objectname) %(objecttype) %(objectsize)"],
        cwd=root,
        input=("\n".join(object_ids) + "\n").encode(),
    )
    .decode()
    .splitlines()
)
all_large = [
    dict(oid=oid, bytes=int(size))
    for oid, kind, size in (line.split() for line in meta)
    if kind == "blob" and int(size) > 1024 * 1024
]
result = dict(
    reachable_large_blob_versions=len(all_large),
    base=a.base,
    head=git("rev-parse", "HEAD").decode().strip(),
    new_commits=len(commits),
    tracked_files=len(head),
    changed_paths=len(introduced),
    added_paths=sum(x not in base for x in introduced),
    new_bytes=sum(size for _, size in introduced.values()),
    violations=violations,
    inherited_historical_payload_findings=historical_payloads,
    inherited_large_files=[dict(path=x, bytes=size) for x, (oid, size) in head.items() if size > 1024 * 1024],
    protected_main_paths_unchanged=len(protected),
)
a.output.write_text(json.dumps(result, indent=2) + "\n")
print(
    json.dumps(
        {
            k: v
            for k, v in result.items()
            if k not in ("inherited_large_files", "inherited_historical_payload_findings")
        },
        indent=2,
    )
)
assert not violations
