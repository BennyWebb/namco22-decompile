# G1 private host attempt — BLOCKED; not ready for hardware

Read ../../NATIVE_G1_REPORT.md before running anything. The full reference host
replay falls back for all4207 native-window pictures: missing direct-FIFO recipes.
Simulation checkpoints pass, but native packet/image gates do not. No ARM build or
hardware test was started. Do not mistake exit0 (working fallback) for native parity.

assets.py consumes the original extracted point/texture ROMs and shader templates,
never recordings or prior course-pack geometry/materials. It enumerates2421models,
compiles97116ordered quads and uses the existing exact-content/overlap packer.
The ignored ROM-hash/builder-hash cache has112MiB of pages and all21783 static recipes.
An existing cache is checked against its manifest. Interrupted caches are retained;
use a deliberately versioned builder, not an incomplete cache.

Reproduction, from repo root (existing run directories are deliberately refused):

    python3 raverace/native/g1/assets.py
    python3 raverace/native/g1/reference_identity.py
    python3 raverace/native/g1/build.py host --validation
    python3 raverace/native/g1/host.py whole-colour
    python3 raverace/native/g1/analyse_host.py

Shaders currently come from native/generated/c1e/axis/index; regenerate them via
C1d prepare.py and C1e prepare_axis.py if missing, using the existing documented
pipeline. Only shader text, not C1e atlas/material/geometry data, enters the G1 pack.
reference_identity.py reads the old reference pack solely to translate validation
IDs. It is not imported/called by assets.py. Unmapped IDs are explicit−1.

transform.py layers on the C1e/P3 derivation without changing shared game sources:
immutable RAM state fields, revision-independent exact-key index cache, larger
asset limits, per-picture fallback receipts and slot-owned live prewarm payloads.
These integration changes are INCOMPLETE/unvalidated beyond the host receipts.
ENG_G1_STATE_AUDIT=1 preserves fixed activation for the initial diagnostic only;
unset uses the provisional RAM gate and complete-picture compilation.
No performance or attract-mode acceptance is claimed. STOP; next scope requires approval.
