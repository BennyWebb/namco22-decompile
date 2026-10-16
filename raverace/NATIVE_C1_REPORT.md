# C1 — Original-size indexed textures / host design blocker

| Item | Result |
|---|---|
| Disposition | **BLOCKED / STOPPED: specified flattening layout needs a design decision** |
| Reference RGBA page payload | 480,614,144 B (458.35 MiB) |
| One 4096×4096 R8UI plane | 16,777,216 B (16 MiB); cannot directly flatten all banks |
| Reference pack bank inventory | 15 distinct planes, 240 MiB before palette |
| Complete ROM map, direct flattening | 16 distinct planes, 256 MiB before palette |
| Reference direct-bank reduction | 1.909× including palette; below required 10× |
| CPU decoder spot checks | 65,536 exact scalar/vector comparisons, all eight orientations |
| Host replay / Mali fidelity / timings | Not run: stopped before renderer integration |
| Production | Hash unchanged; EmulationStation running; stock governors verified |

**No stock-GPU GO or CONDITIONAL GO is established.** The proposed direct
flattening fails its memory premise before shader/performance testing. This is
not a measured rejection of two-lookup indexed rendering in general. A different
packing/addressing design could still be viable; it requires an explicit decision.

## Starting point and scope

Read HANDOVER, P3 report, GPU_NATIVE_TEXTURE_INVESTIGATION and assets/FORMAT.
Branch `feature/knulli-h700`, starting HEAD
`c7f9a4375356d60c3959ea627dd5a062de10d3d5`, clean before C1.
P3 remains performance-failed; its selected P2 b+B1+B2 implementation is unchanged.
Only `native/c1`, C1 evidence/report and this milestone's HANDOVER entry are added.
No simulation, compiler, shader, threading, fallback, asset pack or production
source is changed. The standing rule says to stop for matters requiring a decision.

## Bank-addressing evidence

The 16 MiB CG ROM payload is a tile dictionary, not a flattened world image.
The map has **1,048,576 tile entries**, each addressing a 16×16 region. Its virtual
expanded address space therefore contains **268,435,456 texels**. Texture bank
selects one of sixteen 4096×4096 planes:

```
u = source_u & 4095
v = (source_v & 4095) | (bank * 4096)
map_index = ((v & 0xfff0) << 4) | ((u & 0xff0) >> 4)
```

This matches the existing ROM asset decoder and previous GPU investigation.
Map attributes extend the tile number, flip x/y before transpose, and supply
zero for out-of-range CG references. The C1 audit decodes all sixteen planes
in 64-row chunks and hashes their complete bytes. **All sixteen hashes differ.**
No plane or ROM payload is written to the repository/cache by this audit.

The reference `race-live-v2/materials.json` contains 5,407 recipes spanning
banks 0–14. Its fifteen bank plane hashes also all differ. Bank 0 includes
placeholder/solid recipes: even omitting that entire bank leaves fourteen
planes (224 MiB), still far from a 10× reduction. This inventory is not a new
claim of full-game live coverage; the complete map expansion itself is 256 MiB.

An explicit conflict between two nonzero-bank reference recipes demonstrates
why simply dropping bank selection cannot preserve texels:

| Recipe `(bank,group,cmode,u,v,w,h)` | Common UV | Raw pen |
|---|---|---:|
| `(1,43,0,2944,3456,17,129)` | (2944,3456) | 140 |
| `(12,28,0,2639,2880,593,704)` | (2944,3456) | 226 |

Both use ordinary 8-bit cmode 0, so these are also their final pen indices.
There are 2,162 differing raw texels in their overlapping rectangles. A single
unbanked `texelFetch(image, original_uv)` returns one value at this coordinate;
it cannot satisfy both recipes. Per-material palette groups do not restore the
missing bank-dependent index. This is an addressing counterexample, not an
assertion that these two palette colours necessarily differ in every picture.

An arbitrary remapped atlas is not ruled out by this counterexample. It would
need a new mapping/packing scheme that preserves each primitive's sampling,
wrap, boundaries and orientation; it is not the specified direct map flattening.

## Reproducible host audit

[Audit source](native/c1/audit_flattening.py) reads the user's ROM ZIP directly,
validates chip sizes, records the ten texture ROM hashes and never requires a
recording to decode the banks. The existing reference pack is used only to
inventory material recipes and RGBA bytes, as permitted for correctness audit.

```
python3 raverace/native/c1/audit_flattening.py roms/raverace.zip \
  raverace/native/generated/milestone4a-coverage/race-live-v2 \
  raverace/benchmarks/native-c1-20261016/flattening-audit.json
```

The vector decoder is independently checked against scalar addressing at 4,096
coordinates per bank, 65,536 total, covering all eight flip/transpose combinations.
All checks pass. Complete-plane hashing and conflict search take about 3.51 s
on this host, including those checks. **This is audit duration, not first-start
cache build/load time.** API allocation, GPU padding and driver residency were
not measured; all size figures above are exact uncompressed payload arithmetic.
Palette payload is another 131,072 B (128 KiB).

The existing nine RGBA pages occupy 480,614,144 B. Direct fifteen-bank flattening
plus palette would be 251,789,312 B, only 1.908795× smaller. Even removing bank 0
cannot approach the maximum 48,061,414 B needed for 10× reduction against this
reference. Full-map flattening plus palette would be 268,566,528 B (256.125 MiB),
not the proposed roughly 16–20 MiB complete-game representation.

## Decision required / recommendation

Do not implement or benchmark a bank-expanded replacement under the original
16–20 MiB / 10× premise. It fails that premise regardless of GPU timing.
Choose a revised representation before further work:

- **Compact oriented-tile dictionary plus map:** existing investigation measured
  approximately 20.19 MiB including palette. This retains map→index→palette
  indirection, so it does not deliver C1's proposed two chained lookups. Its old
  measured shader was slow; no claim that it meets the current native budget.
- **Bank-expanded index array:** direct two-lookup semantics are straightforward,
  but approximately 256 MiB full-map payload fails C1's memory gate. Testing it
  would require accepting a different memory objective; not recommended here.
- **A separately designed compact mapping or ETC2 per-course alternative:** requires
  explicit design/fidelity/performance investigation. No memory or frame-budget
  success is assumed. No such rewrite was begun.

Recommendation: revise the compact addressing design first. Keep C1 blocked,
retain RGBA/P3 unchanged, and authorise a bounded design investigation if desired.
Do not label this as a completed integrated NO-GO benchmark: compiler savings,
world+HUD p95, palette updates, resource rotation and sampling fidelity remain
unmeasured for any replacement shader.

## Unperformed gates and final integrity

No C1 shader/runtime switch, first-start cache, per-slot palette upload or
material-format change is implemented. Consequently the all-material baked-texel
comparison, 6,006-tick host replay, 21 sets, adjusted 4,207 packets and 36 image
comparisons are **not newly validated**. Prior P3 validation remains intact.
No H700 game, smoke, validation, timing, fence or 420 MHz run occurred. Device
profiling budget consumed is zero. No soak is required or performed.

A read-only final SSH check verifies production SHA256:
`6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`.
EmulationStation PID 14605 is running; all four CPU governors are `schedutil`,
GPU governor `simple_ondemand`. No C1 clocks or services were changed, so no
restoration action was necessary. The first SSH attempt hit a local system
configuration permission error; using `ssh -F /dev/null` fixed that setup issue.
No production files, launcher, settings, saves, replays or controls were touched.
No commits, push or deployment. **STOPPED at the host design blocker.**
