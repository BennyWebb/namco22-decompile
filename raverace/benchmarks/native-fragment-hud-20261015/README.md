# Focused M3 fragment/HUD investigation — BLOCKED, 2026-10-15

Read the current [race report](../../NATIVE_RACE_REPLAY_REPORT.md) and
[method](../../native/scene/FRAGMENT_HUD_METHOD.md).

All H700 logs cover3840 consecutive ticks1800–5639 with five separate warmups.
`baseline-one-fence-1.txt` and `candidate-one-fence-1.txt` are the matched
acceptance pair. Same executable; candidate uses certified opaque shading,
transparent-glyph omission, original dynamic texture LUT and shadows. The
candidate fails12/14 ms median/p95; no further optimisation or favourable-run
selection followed. All samples and inter-picture gaps remain visible.

All other timing logs are diagnostics. `hud-full`, `hud-no-shadow`, `hud-no-lut`,
`world-opaque`, `hud-cull`, `hud-native-baseline`, `hud-ubo`, `hud-shadow-math`
are in chronological order before the final pair. Split pen/world fences and
shadow/LUT bypasses cannot pass performance/fidelity acceptance. The UBO is
slower and rejected. The final `selected-visual` run reads eleven world/composed
frames after gate failure solely to finish fidelity validation; its timings
are excluded. No transfers ran during measured laps; stock governor retained.

`one-fence-summary.json` holds complete distributions, CPU, waits, counts, RSS,
load and miss counts. `*-interval-gaps.json` exposes large inter-picture gaps;
these are not removed or called exclusive storage timings. Component summaries
use per-picture serial spans and paired sensitivities, not exclusive GPU timers.

`opacity.json` binds full-rectangle alpha and all-command UV hull proof to the
exact pack hash. `source-validation.json` accounts for every source command.
`selected-full-host-comparison.json` scans all3840 world/complete pictures;
`selected-hud-host-parity.json` isolates the selected HUD against the original.
Mali selected capture metrics are byte-identical to prior native Mali captures,
and `selected-h700-hud-oracle.json` independently checks all sampled palette,
shadow and changing LUT pixels. Glyph-invalidation fixture and sanitizer scope
are explicit. Protected file/input/build identities accompany these results.

ROM-derived pages, frames, captures and images remain ignored under
`native/generated/milestone3/`. No assets or binaries belong in Git.
