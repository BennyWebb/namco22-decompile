# G1 — Whole-ROM index assets and state activation

| Item / exit gate | Result |
|---|---|
| **Overall** | **BLOCKED at host native packet/image gate; STOPPED before device tests** |
| Adopted format / policy | Compact index atlas adopted; ≤14ms fenced-completion gate retired for vsync-paced runs |
| ROM-only static assets | **PASS:**2421models,97116quads,21783footprints, seven pages /112MiB |
| Packed atlas parity | **PASS:**564081621 padded texels compared with ROM decoding, zero differences |
| Asset build |115.553s on host; ignored ROM+builder-hash cache, no recording input |
| Reference simulation | **PASS:**6006ticks, all21sets/63 CPU/RAM, board/video and audio records exact |
| Reference native commands | **FAIL:**0/4207 adjusted receipts; all4207 pictures fall back on direct materials |
| Reference provenance | Not reached:0/36 native images; class(c)=0 is **not established** |
| Material cache / activation | Private implementation built; timing benefit and activation fidelity unvalidated |
| Device reference / attract / residency | **Not run:** required host correctness gate did not pass |
| Production | Unchanged SHA256;47/47 protected paths exact; stock clocks and EmulationStation verified |

## Decisions and starting state

Read HANDOVER/C1e, inspected clean `feature/knulli-h700`, HEAD
`6026f57e13172eb23ef7eb7cbf0461aa7351493c`. C1b–C1e compact index atlas is now
adopted by explicit user decision. C1e's historical fenced result remains accurate;
its ≤14ms gate is retired for vsync-paced acceptance, not retrospectively passed.
Pacing, settled audio, long gaps and preparation tails remain acceptance measures.
No new texture design, fence experiment or GPU clock change was undertaken.

Changes are private `native/g1/`, evidence and documentation. Shared engine,
original simulation/physics/audio/control logic, production, saves and replays unchanged.
Host-first requirement and standing STOP-on-correctness-failure rule apply.

## ROM-only assets: completed

`native/g1/assets.py` reads the user's original extracted point/tile-map/texel ROMs.
It never opens recordings, course material catalogs, palettes or captured geometry.
Shader text is reused from C1e; no generated scene data enters the pack.
All2421models in the established `[69,2490)` point-ROM object table validate;
all97116quad packets preserve code, original packet address, corner order, XYZ and
UVs. No lighting packet references or static model decode exclusions occur.
Meshes occupy12435456bytes (11.859MiB) of float32 texture storage, padded to1024width.
All source24-bit coordinates are representable exactly as float32.

21783unique static palette-group/footprint recipes resolve to21783 palette-free
footprints in this ROM set. Exact-content deduplication and same-bank/cmode overlap
merging use the C1b packer:19769initial contents,2043merged rectangles, seven4096²
R8UI pages,117440512bytes (112MiB). Every original footprint including two-texel
source-aware gutters is compared against ROM decoding:564081621texels, zero errors.
All21783 materials have affine UVs, page and palette-group metadata; no palette RGB
content participates in identity. `assets.json` records ROM hashes, cache key,
file hashes, packing statistics and build duration115.553s.

Cache: ignored `native/generated/g1/rom-edfbde03b0d5e68d18bf8041fe2f31786a4063af6bd47e200fd09d5261221108/`.
Key covers all source ROM hashes, format and builder hash. Existing completed cache
loads verify every manifest hash. Game GL load time / actual Mali residency remain
unmeasured.112MiB atlas plus11.859MiB mesh data is not a measured≤200MiB total-GPU pass.

## Integration changes: implemented, not accepted

Private derivation lifts material/mesh inventory limits to65535/131072 respectively;
packet material IDs remain16bit. Index lookup reuses P2's exact-key open-addressed
cache, with palette revision ignored only for index mode. Collisions compare all
seven recipe integers; palette group remains an index. No measured speedup claimed.

Main snapshots four original big-endian RAM words into each immutable slot:
main `0x10008DA0`, substate `0x1000A32E`, screen `0x1000A332`, course `0x1000A046`.
Source anchors: `src/rd/rd_funcs.c` FUN_0000ae04 dispatches main state;
`rd_b1.c` describes the substate/attract state machines; `rd_b2.c` uses course state.
Provisional boot guard rejects main0/outside1..5; otherwise compilation decides
complete-picture coverage. Original tick1800 gate survives only behind explicit
`ENG_G1_STATE_AUDIT=1` for the initial diagnostic. It is unset in the whole-ROM run.
Live slot payloads supply the three offscreen warm pictures; no recorded warm packet
is read by G1. Native failure returns to the existing legacy path and logs the reason.

Reference state audit: main1 at160, main2 at803, main3 at1336; screen/course words
also transition, so the signal is not merely a renamed tick1800 test. Whole-ROM run
has three warm pictures and seven covered early pictures. This is **not** visual
validation of those screens or proof of the final activation policy. Attract-state
validation is outstanding. Snapshot ownership remains main→prepare→present.

## Host gate failure and its cause

The full whole-ROM replay exits0 with no traps; all21 authoritative sets exactly
match C1b/C1e. Logging reports zero dropped records/errors and complete drain.
However all4207 pictures1800–6006 take legacy fallback:
`direct material/palette absent from immutable M3 pack` (legacy diagnostic wording).
There are zero adjusted native receipts and zero native image captures. Successful
fallback is not packet parity, and absent images cannot pass provenance.

Static models omit three existing reference recipes `(bank,group,cmode,u,v,w,h)`:

| Existing reference material | Recipe |
|---|---|
|551|`0,0,0,4080,4080,1,1`|
|771|`9,32,1,2304,2304,1,1`|
|4879|`0,0,0,0,0,1,1`|

All nine direct quads in the existing tick1800 control use those materials: six551,
one771,two4879. This control is read **only for failure attribution**, not asset
building. Comparing the complete old reference catalog with the new static catalog
finds exactly these three absent recipes. Their texels are decodable from texture
ROM; their direct-command footprints are not enumerated by point-ROM model walks.
Adding replay-discovered special cases would undermine G1's ROM-only coverage claim.
A program-ROM/static-direct catalog or bounded runtime direct-material resolution
needs a defined approach; neither has been implemented or silently substituted.

Full-replay coverage: boot159; prewarm3; covered7; direct-material fallback5837.
Every native-window picture is included, no outliers removed. `coverage-by-state.json`
groups counts by raw main/sub/screen/course; course identifiers are not guessed names.
One stderr receipt was split by shutdown stdout; analysis reassembles its intact
continuation and records that fact. No game samples were discarded.

## Device work, G2 input and stop

Stopped at the host native-validation failure under the standing rule. No ARM G1
build, asset transfer, timing run, provenance replay, attract run or device clock
change was made. No device-time performance budget was consumed by game tests.
Reference pacing/preparation p99 versus C1e RGBA13.417ms, actual GPU allocation,
cache GL load time, and native attract repeats/audio/memory all remain **unverified**.
The requested10-minute attract fallback counts cannot be supplied from this host replay.

Required follow-up scope: cover existing direct-command material needs without
recordings; then rerun host packets/provenance before hardware. Full coverage also
needs point-RAM code5, solid/objectflag modes, direct/dynamic footprints, boot/menu
screens and observed attract variants; existing conservative fallbacks must remain.
No inference that2421 static models establish full game coverage or all failure reasons.

Final read-only device check: production SHA256
`6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`,47protected paths exact;
CPU schedutil480000–1512000kHz all4cores, GPU simple_ondemand bounds0/0;
EmulationStation PID5200, no game/probe/sampler. Nothing required clock/service restoration.
[Evidence](benchmarks/native-g1-20261016/README.md) includes the failed host population,
cache manifest, state/coverage receipts and final device check. No commit/push/deploy.
**G1 exit gate NOT PASSED. STOPPED; scope decision required before further work.**
