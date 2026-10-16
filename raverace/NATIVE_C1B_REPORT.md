# C1b — Compact palette-free index atlas

| Gate / comparison | Result |
|---|---|
| Overall | **NO-GO as implemented: host image fidelity fails; STOPPED** |
| Part 1 sizing | **PASS**: whole ROM 112 MiB; reference 58.69 MiB |
| Reference reduction, including one palette | **7.79×** versus 458.35 MiB RGBA; target ≥3.5× |
| Source texel parity | **PASS**: 5,407 materials / 120,048,149 texels, zero differences including gutters |
| Host authoritative replay | **PASS**: both paths, 6,006 ticks / all 21 checkpoint sets |
| Host command checks | RGBA original 4,207 packets exact; 4,207 canonical command receipts equal between paths |
| Host image fidelity | **FAIL**: 0/36 exact; 852,400 differing RGB pixels; alpha exact |
| H700 performance / measured GPU memory | Not run: correctness stop before hardware testing |
| Production | Hash unchanged; stock clocks and EmulationStation verified |

## Method and ROM-only inventory

Read HANDOVER and C1 report. Started clean on `feature/knulli-h700`, HEAD
`1482183fffd0e3ac2b17e8ff50111050f5c607c3`. C1 bank flattening remains rejected.
Design A is a comparison only; it is not implemented or retested.

`native/c1b/size_atlas.py` uses the existing `asset_tool.ROM` decoder and packer.
It reads the user's ROM ZIP, validates chip sizes, and enumerates the established
point-ROM model table `[0x45,2490)`: **2,421 models, 97,116 quad packets, 21,783
palette-free footprints**, zero model errors and zero oversized padded footprints.
The existing decoder's model topology/UV checks pass. This is the known Rave ROM
object-table boundary, not a discovery algorithm for arbitrary ROM sets.

Keys are `(bank, cmode, u, v, w, h)`, dropping palette group. Map orientation is
resolved into the texels using the original flips-before-transpose rules; it
cannot be ignored when decoding. Both observed cmodes (0 and 1) are preserved.

For each footprint, decode final 8-bit pens plus two source-aware gutter texels
on every side. Gutters sample the next original coordinates modulo 4096, not
clamped edge colours. Exact deduplication uses dimensions + SHA256 and compares
complete bytes on every hash hit, so a digest collision cannot alias contents.
Palette groups never enter content identity. Representative coordinates remain
bound to every deduplicated recipe.

Merge strictly overlapping rectangles only within the same bank and cmode,
choosing the greatest padded-area saving first, with deterministic ties. A merge
must also reduce unpadded bounding-rectangle area and remain within 4096 including
gutters. Re-decode the union from ROM; preserve each child's integer offset.
Finally deduplicate merged content again. This greedy policy is measured, not
claimed globally optimal. It preserves wrap and the original source texels.

Packing calls the existing nonrotating 4096-extent MaxRects rules unchanged:
short-side-fit ordering, contained-free-rectangle pruning, last-page dimensions
trimmed/aligned to four texels. Fresh UVs retain the existing half-texel affine.
No mipmaps or filtering changes. Outputs/cache, decoded ROMs, mappings and pages
remain under ignored `native/generated/c1b`; no ROM-derived texture is committed.
No recordings are read during sizing/building. The existing reference pack and
its palette lookup supply only the requested reference inventory and parity oracle.
The whole-ROM footprint enumeration and index contents depend solely on ROMs.

## Sizing and exact source validation

| Representation | Pages / dimensions | Index or texture payload MiB | With one 128 KiB palette MiB |
|---|---|---:|---:|
| Existing reference RGBA | 9 existing pages | 458.349 | already palette-baked |
| B: reference index atlas | 3×4096² + 2736×4096 | 58.688 | 58.813 |
| B: reference, conservative full pages | 4×4096² | 64.000 | 64.125 |
| B: whole-ROM index atlas | 7×4096² | 112.000 | 112.125 |
| A: prior oriented dictionary + map | prior layout; not implemented | 20.0625 | 20.1875 |
| C: rejected bank flattening | 16×4096² | 256.000 | 256.125 |

Even allocating all four reference pages at full extent gives **7.15×** reduction
including one palette, so the reference gate does not depend on trimming.
Three rotating palette resources in the prototype add 384 KiB total: reference
59.063 MiB, whole-ROM 112.375 MiB. These are payload/API storage calculations,
**not measured Mali residency** or total process memory.

| Population | Footprints | Exact initial contents | Overlap merges | Final contents | Content MiB |
|---|---:|---:|---:|---:|---:|
| Reference pack | 5,407 | 5,026 | 4,499 | 527 | 53.171 |
| Whole static ROM | 21,783 | 19,769 | 17,726 | 2,040 | 109.378 |

Reference sizing takes about 10.41 s; whole-ROM sizing about 115.42 s on this
host, including decoding, merging, packing, verification and page writes. These
are offline tool durations, not handheld startup/cache load measurements.
Every original footprint is checked back through the packed page, including
gutters: 120,048,149 reference and 564,081,621 whole-ROM pen texels match.

For every reference material, apply its exact palette from `live.lookup`, then
compare the complete allocated rectangle with the existing RGBA page. **All
120,048,149 RGBA texels match byte-for-byte; gutter differences =0.** No colour
approximation, palette invention, tolerance or sampling decimation is introduced.

## Unresolved static coverage

All 97,116 enumerated ROM packets have fixed UVs resolvable by the decoder:
static enumeration errors/unresolved UV packets =0. This does not prove runtime
coverage. Three reference footprint keys are absent from that static set:
`(0,0,0,0,1,1)`, `(0,0,4080,4080,1,1)`, `(9,1,2304,2304,1,1)`.
These are `(bank,cmode,u,v,w,h)`, not counts of objects or picture occurrences.
Their reference texels are included and pass parity; their absence alone does
not classify their provenance as direct, solid or dynamically generated.

One known point-RAM object selector, code 5, and the direct-polygon FIFO format
remain runtime inputs. Their full-game object/polygon occurrence counts and any
dynamically modified UV states **cannot be counted from ROM data alone**; they
are unknown, not zero. No recordings are used to invent such counts. Existing
fallback/unsupported handling is unchanged; no new dynamic-format support added.

## Selectable host prototype and correctness stop

The private build derives from selected P3 b2, preserving simulation/audio,
three-stage ownership, queue 2, valve OFF, O2/stack protection and default priority.
`ENG_C1_TEXTURES=rgba|index` selects the path at startup; default remains RGBA.
`ENG_C1_ATLAS` explicitly names the index pack. Startup receipts confirm both
paths. The same validation binary is used for the clean RGBA/index comparisons.

Index pages use GL_R8UI/nearest/clamp. The fragment shader fetches the pen using
`texelFetch` at fresh atlas UVs, then fetches its RGBA8 palette colour; original
shade/priority calculation remains. Reference world materials are proven opaque;
the no-discard shader is used. A discard shader source is retained separately,
but no cutout material is claimed tested or newly supported.

Palette-group revisions come from P2 tracking and are copied into each immutable
slot. Presentation reads that slot's palette RAM, never live state, and uploads
only groups changed relative to each of three rotating GPU palette textures.
The existing upload-ring fences protect resource reuse. Compiler material lookup
bypasses palette-byte matching on index mode, preserving the remaining P3 compiler.
Warmup converts only old page-group runs to the new page mapping off screen.
No architectural or gameplay change is made.

Both clean host replays complete all 6,006 ticks and match all 21 authoritative
sets/63 domain records exactly. RGBA retains all 4,207 original packet hashes and
all 36 prior P3 host image hashes. New atlas pages alter material UV metadata,
page IDs and compatible batching order; raw index packets therefore differ.
Canonical receipts hash each pre-painter command array with only the page field
zeroed. All 4,207 receipts match, retaining mesh/material identities, transforms,
scissors, priorities, brightness and culling flags. This is an explicitly adjusted
command check, not a claim of raw packet identity for index mode.

**All 36 index images differ from their RGBA counterparts.** The 852,400 changed
pixels are RGB-only; alpha is exact. Most changed channels differ by 1–2, but
maximum absolute channel difference reaches **162/255**. This is not accepted as
rounding noise. All images and difference locations are retained. The viewed
tick-2500 composite looks close, but visual similarity cannot waive exactness.
Source texel parity does not establish sampler/interpolation or batch equivalence.
Fresh UV placement/normalised sampling versus integer fetch and shader sampler
precision are candidates for attribution, not proven explanations for every pixel.
The exact cause remains unresolved; no bias, tolerance or speculative fix is added.

The standing correctness STOP rule applies. **No ARM build, device validation,
smoke, timing, fence or 420 MHz diagnostic is started.** GPU p95, pacing, compiler
savings and measured GPU memory reduction remain unmeasured. This is NO-GO for
the current implementation, not evidence against the compact atlas's sizing.
Recommendation: retain the ROM-only atlas tool; separately authorise focused
host sampling/precision attribution before any further hardware experiment.

## Evidence, setup history and final state

[Evidence README](benchmarks/native-c1b-20261016/README.md) indexes exact counts,
per-material parity, all image statistics, checkpoints, command receipts, source/
binary identities and setup logs. Side-by-side PNGs for **every** sampled image:
`/home/bwebb/dev/namco22-decompile/raverace/native/generated/c1b/image-review/`.
Each PNG is RGBA left, index centre, magenta differing pixels right. Raw RGBA and
per-pixel difference arrays remain ignored locally; their review hashes are public.

Setup errors were fixed and retained: unsigned gutter coordinates needed modulo
4096; a diagnostic buffer declaration prevented one build; the first comparison
used a stale binary and hit a shutdown image-export buffer limit; the clean-run
checker then referenced an old directory name. Analysis was corrected without
another replay. The two clean game processes exit 0; their exported evidence is
complete. Validation timings are never performance evidence. No failed attempt
or differing sample is removed.

Read-only final SSH verifies production SHA256
`6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`.
EmulationStation PID14605 is running; no diagnostic game/sampler/perf remains.
All CPUs are schedutil/min480000/max1512000 kHz, GPU simple_ondemand/min=max0.
No device settings or services were changed, so no restoration was needed.
Device profiling time =0; no soak. No production, launcher, settings, saves,
replays, controls or shared engine files changed. No commit/push/deployment.
**STOPPED. Further fidelity work requires explicit authorisation.**
