# GLES3 renderer

`gles3/src` is a direct C implementation built by CMake, with no Python relinkers,
private source rewriting, cached objects or captured-pack build dependencies.
The board/simulation interfaces remain shared with desktop and the other games.
The GLES3 scheduler contains only its headless path; desktop's entry point is main's
implementation after formatting. Both compositions use src/rr_mem.c and src/rr_audio.c.
RR_AUDIO_RESAMPLER_TELEMETRY gates the three GLES3 audio-metric additions; desktop
keeps its original audio behavior. One src/rr_headless.inc uses small platform hooks.
The GLES3 composition contains lifted execution only, with strict compatibility hooks
for inactive checker seams; desktop retains its readable replacements/checker.
The lifted 68K and C25 compile at O2; the sole strict C74 translation compiles at O1.
Hand-written GLES3/platform/test sources compile with -Wall -Wextra, without -w.
Only the three generated CPU/DSP/sound sources retain -w. .clang-format and the
Black configuration set120 columns; READABILITY_RENAMES.csv lists all renames/removals.
Only test executables link the sound interpreter. CE5B/CEF4 dispatch targets come
from their two ten-entry ROM tables; missing translation or guarded-RAM mismatch
traps loudly. tests/fixtures/sound-dispatch.json lists51 unresolved indirect
variants and CB49's mutable DPR+$0130 dispatch; these remain known follow-ups.

Simulation publishes immutable snapshots through a bounded FIFO. Preparation
walks the real display list, applies exact B1 projection/B2 conservative culling,
resolves ROM/material identities, then assigns painter depth and draw groups.
There is no legacy world renderer, RGBA baked-page path, overload replacement,
substitute geometry or per-picture renderer fallback. Unsupported data stops.
WRAM and C352 inactive-voice fast paths preserve their general-path results.

The whole-ROM compiler owns deterministic pen deduplication, overlap merging and
page packing. The checked-in files are compiler/shader source, never texels, XYZ,
layout witnesses or baked packets. Locally generated metadata and a small C
first-launch decoder reconstruct identical R8UI pages/meshes from user ROMs.
Dynamic pages use bounded allocation, exact-key material caching and immutable
slot-prefix reuse; palette changes do not invalidate pen pages.

Each quad is an independent six-vertex instanced triangle list (near projection
uses twelve). Adjacent protected quads can share a draw when page, scissor and
mode match; mode fixes the program and attribute layout. All instances retain
order, geometry, protection, sorting keys, painter ranks and depth. Protected
modes also retain the no-reversal rule. A historical reordered zero-area case
used triangle lists too, so primitive type alone was no proof; exact host and
Mali readbacks established this adjacent-only change. No degenerate is removed.

Boot/ROM-check pictures use the GPU text/palette/LUT compositor over the clear
world. Shaders, atlas, buffers and all three GPU copies pre-warm off-screen before
the first presentation. Validation checks159 boot pictures against CPU decoding.

Tests use committed controller recordings, verified by SHA256, and a ROM-booted
factory EEPROM. Hash-only fixtures cover21 reference sets, fixture states,
final states,4207 adjusted command
packets, semantic receipts,36 reference images,26 bonus images and20 attract
samples. Normalized commands expand group state per visible command; only draw
boundaries are ignored. Stable ROM identities replace capture-era validation
IDs; full accepted packet equality establishes that identity-only adjustment.
The older semantic digest manifest is retained as hash-only historical evidence.
Direct ROM provenance is a separate pen/U/V/palette proof, class(c)=0.

The bonus compiler tail remains a follow-up: one17.413ms preparation sample at
11906 contained12.690ms compilation and4.647ms walking, with no panel repeat.
This cleanup changes composition/naming, not that algorithm or its geometry.

Validation has a separate core and test-only shaders. Product rr_gles3 excludes
provenance/palette capture, command dumps/receipts/image hashes, fixed capture ticks,
object/cull dumps, batching diagnostics and the forced-unsupported fixture.
Strict unsupported exit86 remains in the product. tests/audit_product.py checks
both link maps and symbols/strings/compile flags against the validation executable.
