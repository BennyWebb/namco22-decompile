# Focused Milestone 3 fragment/HUD method

Authorised scope: prove material opacity; diagnose pen, shadow and LUT costs;
implement only measured improvements and retain the full-picture12/14 ms gate.
Production game/engine sources remain unchanged. Prior M3 data stays historical.

`tools/audit_opacity.py PACK REPORT` verifies the v3 manifest and every file SHA,
scans actual padded material rectangles for alpha255, then checks every original
static/direct UV corner over all3840 pictures. Native float32 UV affine mapping
must leave at least one texel inside the certified rectangle. Positive-depth
clipping and perspective interpolation keep samples inside the corner UV hull.
The proof applies only to unchanged nearest/clamp pages, UVs and palette revision;
background page padding, unproved footprints or any cutout must keep discard.
The report binds flags to the exact pack manifest hash. This is a proof about
actual native RGBA assets, not a claim that all original ROM pens are opaque.

`tools/build_hud_probe.py NEW_DIRECTORY` creates private copies of the existing
video-layer/HUD implementation and an isolated renderer. With
`ENG_NATIVE_STAGE_SYNC=1`, world completes before HUD. With
`ENG_NATIVE_HUD_FENCES=1`, the original pen preparation/updates/draws finish before
final palette/LUT uploads and fullscreen composition. Pen wall and thread CPU
are recorded per picture. Final span is each picture's HUD-plus-finish minus
its pen span. These serial fences perturb Mali tile scheduling and are excluded
from headline acceptance.

HUD control0 preserves the full original compositor. Control1 disables shadow
selection; this changes the shadow branch to palette lookup, so it measures
sensitivity rather than exclusive shadow arithmetic. Control2 removes final LUT
reads while retaining text/world/shadow selection; actual166 LUT revisions are
still transported/uploaded. Neither omission may pass fidelity or headroom gates.
The normal renderer retains all source shadow multipliers, original palette and
all LUT changes. A distinct shadow-arithmetic control may refine attribution.

Only full original appearance with one final world+HUD `glFinish`, no image
capture, no swap and no file transfers qualifies for complete-picture headroom.
Matched comparisons use identical executable, scene pack, tick window1800–5639,
original HUD state and warmup policy; shader/control identities are recorded.
Keep main and producer CPU distinct, expose all3840 intervals and deadline misses,
and do not reinterpret offline capacity as simulation/audio/display throughput.

`tools/build_hud_candidates.py NEW_DIRECTORY` derives the probe with independent
`ENG_NATIVE_HUD_CULL=1` and `ENG_NATIVE_HUD_UBO=1` flags; both default off.
The cull path compares each current 128-byte glyph with all0xff (pen15 only),
including codes960–1023 backed by text RAM. It omits only those glyph tiles,
and blank/nonblank transitions invalidate cached geometry even with an unchanged
tilemap. Nonblank glyph content updates keep the existing upload/dirty logic.
The GL fixture in `tests/check_hud_font_invalidation.py` freezes text RAM, toggles
an actual blank glyph through pen0/pen1/transparent, and compares the original
and culled compositions byte for byte.

The UBO path uses `render/hud_lut_ubo.frag`: the same palette, shadow, priority,
rounding and dynamic LUT algorithm, with three indexed reads from a std140
256-vec4 (4096-byte) uniform block instead of three LUT texture reads. GLES3
block limits are queried. Every actual LUT change uploads exact normalised byte
values; no identity/constant-LUT assumption is made. The original texture-based
shader remains selectable. Existing texture LUT uploads are retained for matched
cache/control behaviour and tracked alongside the additional uniform uploads.

HUD control3 keeps shadow selection but bypasses only its multiplication/floor
arithmetic. It refines control1, whose palette-branch change is a confounder.
Controls1–3 remain appearance-changing diagnostics and are never acceptance.
`prepare_fragment_shaders.py PACK NEW_SHADER_DIR` reruns the complete opacity
audit before choosing `race_opaque.frag`, preserves `cutout.frag`, and selects
the original shader for the entire pack if any material is unproved. This actual
pack has no cutouts; no unimplemented mixed-material batching is assumed.
