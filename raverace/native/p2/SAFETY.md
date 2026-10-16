# P2 exact-output reasoning

These changes operate only on the immutable slot in the single rr-prepare owner.
Original simulation/audio and FIFO ownership remain P1b's. Rendering resources,
shaders, near/protected batches and depth encoding retain the existing contract.

The material cache key is all seven signed32-bit recipe fields, compared exactly
on a candidate hit. Hash collisions probe other entries and cannot produce a hit
without that comparison. The revision belongs to the recipe's128-way palette
group. Three exact256-byte planar comparisons detect changes; a revision advances
before resolving any command. Revision wrap clears entries and gather state.
Full-table fallback uses the original resolver; an unsupported material remains
unsupported. No palette digest is substituted for exact equality.

Selective gathering first identifies every group referenced by captured world
commands and direct FIFO commands. A used group is transposed whenever its
current revision has not been gathered, including a group changed while unused.
Background/HUD/LUT use original snapshot data, not this cached world palette.

Offline metadata decodes every existing(code,packet) from the manifest's original
ROM identities and verifies every position/UV against the existing GPU mesh bytes.
Only static bank, mode and UV bounds become cached recipe components. Palette
group, brightness, transforms, depth, clips and gameplay state remain live.
Loader checks the metadata's identity at each immutable mesh texel index.

The experimental culling reuse substitutes only original walk Z, only for valid,
non-near, unprotected quads. The sum of absolute transform terms /32768 plus
absolute translation plus1 must be strictly below INT32_MAX. This proves both the
intermediate shift/cast and translated original integer depth did not overflow;
the value equals the compiler's original integer shift+translation exactly.
All error terms, float32 enclosure corners, zoom and clipping tests remain the
original calculation. Ordinary integer screen projections cannot replace those
error enclosures; X/Y and unproven, near/degenerate/protected cases retain them.
Whether this small reuse is worth selecting is a measurement decision.

Index sorting uses the same comparators on original commands. Painter ties use
unique source order. Assigned depth steps are1.8/(n+1), n<=4096: much larger than
float32 ULP across[-.9,.9], so batch-depth ties cannot be introduced. Packet order,
instance dedup order and visible/group indexing follow the sorted permutation.
No protected batch is merged or reprioritised. Exact complete packet comparisons
and sampled Mali comparisons remain mandatory; this reasoning does not waive them.

Validation builds keep SHA/image receipts and renderer consistency scans. Play
builds remove those scans and digest calls, retaining format/capacity failure
handling needed for bounded operation and the existing unsupported fallback.
Main authoritative checkpoints remain enabled in both. Ordinary cache key hashing
is lookup work, not a packet/image validation digest. Profiling builds are separate,
with frame pointers/symbols and no inlining; never use their timings as results.
