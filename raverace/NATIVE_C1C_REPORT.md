# C1c — Index-atlas fidelity attribution

| Item | Result |
|---|---|
| Gate | **BLOCKED / NOT PASSED; STOPPED. Do not begin C1b device Part 2.** |
| Normalised integer sampler | Implemented; same 852,400 differing pixels as C1b |
| Palette precision attribution | Matching RGBA's lowp qualifier removes 846,527 original differences (99.31%) |
| Revised compact comparison | 5,875 differing pixels across 36 images; alpha exact |
| Exact-layout control | 22/36 exact; 426 differing pixels; 5,244 shared texels have incompatible palette constraints |
| Proven adjacent-source class (a) | 0 established; residual adjacency is unproven |
| Host state / command checks | All three new replays pass 21 sets / 4,207 adjusted receipts |
| Review / device | All 36 side-by-sides + overlays generated and reviewed; no device contact |

## Method and sampler attribution

Read HANDOVER and C1b report; started clean on `feature/knulli-h700`, HEAD
`65028074a7d3393712d957c72725f36d56c26ec3`. All changes are private `native/c1c`
plus report/evidence/HANDOVER. Existing C1b, P3, game logic and production are unchanged.

C1c derives the same host validation build and reference compact atlas as C1b.
`ENG_C1_SAMPLER=texture|fetch` retains both modes; texture uses
`texture(usampler2D_image, texcoord).r` with GL_NEAREST and the existing fresh
normalised UV computation. `ENG_C1_PALETTE_PRECISION=highp|lowp` changes only the
palette sampler qualifier. Receipts record both settings before rendering.

Two complete 6,006-tick host replays use normalised texture sampling, first with
C1b's highp palette sampler, then with lowp. Both pass all 21 authoritative sets /
63 domain records and all 4,207 adjusted pre-batching command receipts. This is
host validation, not a device performance result. No timed run or device is used.

The normalised/highp output is byte-identical to all 36 C1b fetch/highp images.
Thus the hand-computed integer address is not the cause of any observed C1b
difference on this host. This does not establish sampler parity on Mali.

The RGBA shader declares an unqualified `sampler2D` (default lowp), whereas C1b
explicitly declared its palette sampler highp. Holding all other inputs/shader
source fixed, lowp removes **846,527** original differing pixels. These removed
pixels have original maximum channel error **7/255**. This proves a shader
precision mismatch in the tested host path; it is not missing source texels or
a change to palette RAM. It does not establish a portable GPU quantisation model.
The qualifier switch is diagnostic here; no C1b/production shader is replaced.

## Exact-layout control: incompatible colour aliases

The RGBA atlas deduplicates **colours**, not pen indices. Several logical materials
share exactly the same physical rectangle despite requiring different pen values
under their respective palettes. A pen atlas cannot always retain those aliases.

The control audit preserves every RGBA page dimension and rectangle placement,
decodes the original pens including gutters, then tests palette constraints.
A first-pen representation fails 6,690 material-texel lookups at **5,244 unique
physical coordinates**. At every such coordinate, exhaustively testing all
256 candidate pens finds **no pen satisfying every associated original palette**.
This is a static representability failure, independent of sampler, timing or batching.

Concrete witness: page 4, texel `(1927,3897)`, stored RGB `(38,51,76)`:

| Material | Recipe `(bank,group,cmode,u,v,w,h)` | Shared rectangle | Only matching pen |
|---|---|---|---:|
| 635 | `(7,1,0,3072,1167,11,7)` | `(1925,3897,15,11)` | 3 |
| 4664 | `(10,2,0,3072,1167,11,7)` | same | 4 |

No R8UI byte at that shared texel can be both 3 and 4. Colour equivalence in the
RGBA pack is valid; retaining that physical alias in an index pack is invalid.
3,438 of the5,244 conflicting coordinates lie inside at least one footprint
interior; these are not solely unused outer padding.
The full constraint audit, complete failure coordinates and palette-hash witness
are retained. This does **not** invalidate C1b's compact atlas: it keeps distinct
pen contents and already passes all source-texel/palette comparisons.

A literal first-pen layout control is run with these conflicts explicitly logged:
**22/36 images exact; 426 RGB pixels differ across the other14, max error145.**
All21 state sets and4207 original raw packet hashes pass, keeping geometry, UVs
and batching identical to RGBA. This confirms the control image gate fails; the
static audit proves incompatible aliases, without attributing every rendered
control pixel to a specific material. No conflicted pack is labelled valid.
Achieving a source-exact control requires separating the incompatible
aliases into independent resources, or adding a palette-remapping mechanism.
Either changes the literal exact shared-layout control; a decision is required.
The mandatory exact-control gate is therefore not satisfied.

## Pixel classification and remaining uncertainty

Every original differing pixel has a retained location and conservative attribution:

| Original C1b pixel population | Count | Maximum original channel difference | Classification |
|---|---:|---:|---|
| Removed by normalised sampler alone | 0 | 0 | None |
| Removed by matching palette precision | 846,527 | 7 | (b), explained precision mismatch |
| Still differs after precision match | 5,873 | 162 | (b), provisionally unclassified; adjacency not proven |
| New residual after precision match | 2 | revised error 2 | Additional unclassified pixels |

The revised comparison has **5,875** residual pixels, maximum error **162/255**;
all alpha bytes remain exact. Each of the 36 images has 88–238 residual pixels.
Full per-image/per-category counts, maxima, example coordinates and **every pixel's
location** are in the evidence. Coordinates use bottom-origin framebuffer space.

Class (a) requires proving the actual sampled pen is adjacent to the RGBA path's
sample in the same source footprint. Neighbouring screen colours or small image
errors do not prove that. No pixel is promoted to class (a) by a tolerance or
visual judgement. The 5,875 residuals may be boundary choices, but their source
texel provenance is not measured. Missing geometry, incorrect palettes or wrong
texels are not declared acceptable. The requested exhaustive adjacent-source
classification remains incomplete following the failed exact-control prerequisite.

## Visual review and recommendation

All 36 revised images have full-resolution PNG triptychs (RGBA / index-lowp /
magenta difference mask), separate translucent difference overlays and six contact
sheets. Reviewed all six sheets covering all36 world/composed images: the scene,
vehicles, mirrors and HUD look closely matched, with sparse residual dots rather
than a broad colour shift or obvious additional missing objects. This is static
host image review, not proof of geometry identity or boundary provenance, and not
a user approval of unexplained differences. No claim about motion/Mali is made.

Files are in:
`/home/bwebb/dev/namco22-decompile/raverace/native/generated/c1c/review/`.
Example: `tick-2500.composed.comparison.png`; overlays use `.overlay.png`.
[Evidence README](benchmarks/native-c1c-20261016/README.md) indexes all image hashes.

**Do not proceed to the unchanged C1b device go/no-go yet.** The size case remains
promising, and palette precision explains almost all previous differences, but
neither mandatory fidelity gate passes. Recommend authorising a revised control
that separates incompatible colour aliases while preserving each material's page
dimensions, placement and original UV affine, followed by exact source-texel GPU
provenance for the residual pixels. Keep the C1b device acceptance criteria unchanged.
No such control redesign or further attribution implementation is begun here.

## Final state

Host-only throughout: no SSH, handheld transfer, clock/service change, ARM build,
device replay, commit, push or production deployment. Production/EmulationStation
were not contacted or altered; no new device hash/restoration claim is made.
All prior evidence and these host runs remain retained. Branch HEAD is unchanged.
**STOPPED at the exact-control design blocker; further work requires authorisation.**
