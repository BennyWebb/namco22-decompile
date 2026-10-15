# Focused Milestone 3 depth/LUT/gap method

The existing optimized baseline is certified opaque-world shading plus invisible
HUD glyph omission, original dynamic texture LUT and original shadow rules.
Production/shared engine code and the compressed scene pack remain unchanged.

`audit_depth_order.py PACK OPACITY_PROOF REPORT` binds to the certified pack,
checks all source painter keys `(zsort, emission order)` against synthetic depth,
quantises all depths to D16, and verifies unique separated ranks. The actual
minimum is27 D16 steps. This is a CPU depth/rank audit, not a formal guarantee of
every proprietary raster interpolator; queried device precision (actual24 bits), runtime batch guards and full host
images are separate checks. No new candidate Mali capture was run before gate failure.

`build_depth_lut_probe.py NEW_DIRECTORY` builds private video/HUD copies derived
from the original probe. No UBO implementation is compiled. Independent flags:

- `ENG_NATIVE_FRONT=1`: copy the visible permutation, sort ascending synthetic
  depth only inside mode0 (ordinary, unprotected) groups, leave group boundaries,
  draws, source IDs, protected and near-clipped ordering unchanged. Runtime
  adjacent D16 separation must exceed8 steps. Original commands stay read-only.
- `ENG_NATIVE_FOLD=1`: palette entries contain their current LUT-adjusted RGB.
  A256x4 RGBA texture has the original LUT in row0 and three rows containing
  `LUT[floor(input * original_shadow_multiplier /256)]`, independently per channel.
  Shadows select a row; opaque text palette output needs no subsequent LUT reads.
  World gamma/fade still uses three original row0 reads. Any palette/LUT/multiplier
  change rebuilds both tables; shadow enable remains an original uniform. All166
  observed LUT revisions are retained. Extra GPU storage is3072 bytes; original
  fixed-byte HUD counter is historical and must be adjusted by that amount.
- `ENG_NATIVE_LOG=ram`: retain every formatted telemetry line in a fixed16 MiB
  buffer and drain after the measured lap. Overflow fails explicitly. Formatting
  CPU remains timed. Final drain wall time is reported separately; nothing is
  dropped or decimated. `sync` preserves the original stdio/file logging path.

All variants include the prior verified transparent-glyph omission. Default
front/fold flags are off. Every process uses the same binary, source packet,
opaque shaders and5 warmups. Component world/pen fences are diagnostic only;
only one final complete-world+HUD fence counts for the unchanged12/14 ms gate.

Per-picture `GAP` rows (3839 real end-to-next-start transitions) are retained in
RAM and emitted after measurement in both modes. They split GL error checking,
trace formatting/output and FIFO release from residual bookkeeping/instrumentation
and scheduling. Thread CPU and voluntary/involuntary context switches distinguish
blocking from execution; residual wall time is not an exclusive scheduler timer.
Next-frame FIFO acquisition/readiness is already inside complete-picture timing.
Every gap must equal the next frame's interval minus its completion span.
The final picture has no next-picture interval; complete lap wall and drain are
reported separately. Raw completion/interval outliers are never removed.

`check_folded_hud_revisions.py` freezes LUT/text while changing palette, mix and
shadow state to test table cache invalidation. Full host sequence comparisons,
the independent byte compositor oracle check host visual correctness. No game state is advanced, simulated or rescheduled by the probe.

`--linear` derives O(n) reversal after proving source depth is strictly descending
inside each eligible group. Runtime float separation must exceed0.0003; this
conservatively exceeds8 D16 bins. Offline validation proves it matches ascending
stable sort for every group. General qsort is retained only as a diagnostic.
Final candidate uses FRONT1/FOLD0/LOG ram; no extra component fences. Querying
`GL_RENDERBUFFER_DEPTH_SIZE` reports24 on H700. Final p95 failed15.300 ms; stop.
