# Mali-G31 allocation/upload/lifetime investigation

Read ../../MALI_TEXTURE_ARRAY_DRIVER_INVESTIGATION.md for the decision,
methodology, limitations and all metric tables. Production is retained;
the bounded-retention candidate is rejected.

Accepted normal racing-window comparisons are rounds 1, 3, 4, 5, 6, five per
configuration. The complete raw first matrix includes the excluded round 2;
its entire three-way round is replaced, not only its bad candidate. Round-one
production has whole-process audio underruns and pre-window simulation gaps:
this is explicitly disclosed, not labelled zero audio or racing-only.

- `summary.json`, `sections.csv`: established analysis across accepted normal
  first-lap windows; three configurations, thirteen 300-tick sections each.
- `details.json`: tick-1965 scopes, lifetime/occupancy and memory samples.
- `report-tables.md`: generated tables, no additive GPU/CPU savings estimates.
- `*-payload-summary.json`, `synthetic-final-summary.json`,
  `pressure*-final-summary.json`: 120 samples per shape/case, corrected final
  probes; full median/p95/p99/max API-wall distributions. No GPU-only claim.
- `actual-burst-summary.json`: exact fixed-history decoded bytes, 78 requests.
  Earlier `burst-summary.json` is a separate 75-request synthetic control.
- `*-trace-summary.json`, `operation-sections.json`: diagnostic API timings,
  not performance samples. Final identity traces bind fence/invalidation
  events to the actual affected cohort.
- `raw-exploratory.tgz`: early text logging, initial synthetic/occupancy/burst
  controls and initial retention proof. They are preserved with limitations.
- `raw-host-final-validation-and-payloads.tgz`: native/TSan/order correctness,
  actual decoded payloads, requests and provenance.
- `raw-final-mali.tgz`: all normal runs (including excluded round 2), final
  controlled Mali probes, correctness and resource-identity traces.
- `raw-low-overhead-full-trace.tgz`: full-lap buffered operation trace. Its
  array run has resync/audio anomalies; not an accepted FPS/audio benchmark.
- `raw-accepted-replay.tgz`: selected normal runs, reproducible analysis input.

To reproduce report data, extract the accepted archive, then run from the
repository root:

    python3 raverace/tools/mali_texture_driver/analyse_benchmark.py /tmp/rr-mali-array/accepted 1800 5639
    python3 raverace/tools/mali_texture_driver/analyse_details.py /tmp/rr-mali-array/accepted
    python3 raverace/tools/mali_texture_driver/write_tables.py /tmp/rr-mali-array/accepted

`manifest.json` records source/device identities and selection rules.
`SHA256SUMS` hashes every archived artifact and summary.
