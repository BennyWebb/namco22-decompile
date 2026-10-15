# Milestone 3 focused slow-picture/HUD/Mali investigation, 2026-10-15

Status: investigation complete, M3 BLOCKED. Fixed targets remain <=12 ms median
and <=14 ms p95 complete world-plus-HUD rendering. No production change,
commit, push, deployment or Milestone 4 work. Prior M2 exit checks remain complete.

`repeatability.json` is authoritative for the fixed B,C,B,C,B,C series. B is the
current optimized front-to-back renderer with original HUD; C adds dead-depth
invalidation. All six processes use the same binary, five warmups, all 3,840 ticks,
RAM telemetry and one final GPU fence. Every timing sample is retained. Completion
and frame intervals are reported separately; no pooled pass replaces a failed run.
Each process accounts for all 5,152,196 source commands and identical draw/geometry
counts. Corresponding `repeat-*.txt` and memory snapshots preserve raw evidence.

All candidate p95 results fail: 16.139230 / 15.506663 / 15.488718 ms. Paired median
benefits 0.091671 / 0.114132 / 0.143260 ms are below the declared 0.2 ms useful-benefit
threshold. Candidate deadline misses increase in all pairs. **Depth discard is
rejected and stays disabled**, along with RG8 pen/output-color discard controls.
The selected experimental renderer remains front-to-back + original HUD/LUT.

Evidence guide:

- `slow-analysis.json`, `frame-features.csv`, `p95-cohorts.json`, `draw-breakdown.json`:
  all 69 previous deadline misses, full-race correlations and protected draw cost.
- `tail-components.json`, `components.json`, `component-paired.json`: independently
  fenced world/pen/final costs. These runs are diagnostic, not acceptance timings.
- `color-components.json`, `color-paired.json`: bounded final-framebuffer load test.
- `coarse-overdraw.json`, `overdraw-summary.json`: approximate CPU coverage/depth
  sampling, explicitly not hardware fragment counters or full-resolution raster proof.
- `pen-output-redundancy.json`, `lut-style-audit.json`: unchanged intermediate images
  and actual LUT identity audit; no new caching/LUT alternative selected.
- `mali-order-comparison.json`, `mali-selected-comparison.json`: byte-exact world,
  priority, inset mirror and composed comparisons at 21 difficult/representative ticks.
- `mali-*-hud-oracle.json`, `host-bandwidth-oracle.json`: independent original byte
  palette/shadow/LUT CPU checks against actual captured world/output.
- `host-full-comparison.json`: all 3,840 host world/composed RGBA frames match the
  existing optimized renderer, with all 166 LUT styles retained.
- `sanitizer.txt`, `validation-summary.json`: meaningful graphics/source checks.
- `production-before.txt`, `production-final.txt`, `integrity.json`: production,
  launcher/controller, all six replay files, rollback/resource integrity and device state.
- `build-sha256.txt`: build/source snapshots including the original diagnostic
  binary; `final-source-sha256.txt`: final helper/method identities after documentation
  and strict analyser checks were completed. Both tested binaries' identities are
  verified again in `production-final.txt`.

Unfenced world/pen/final subspans inside one-fence logs overlap GPU work; do not
interpret them as independent GPU completion. CPU and context-switch correlations
cannot uniquely identify proprietary shader/cache/driver internals. Historical long
stalls did not reproduce and remain unresolved. No new user visual signoff or
all-frame Mali readback is claimed. Live simulation/audio acceptance belongs to M4.

Generated asset packs and lossless captures remain ignored under the durable home
repository's `raverace/native/generated/milestone3/`. Actual Mali review gallery:
`/home/bwebb/dev/namco22-decompile/raverace/native/generated/milestone3/tail-hud-review/index.html`.
No extracted game assets are stored in this evidence directory.
