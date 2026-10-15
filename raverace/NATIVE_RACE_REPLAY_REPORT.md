> **Current follow-up (2026-10-15):** the user authorised controlled Milestone4A
> despite the M3 performance block. A live host prototype was built, then stopped
> at missing mesh code2028/packet1456601 on tick5655. M3 is **not passed or waived**.
> See [live-integration report](NATIVE_LIVE_INTEGRATION_REPORT.md). No H700 deployment
> or M4B work occurred. The M3 performance evidence below remains unchanged.

# Slow pictures / HUD bandwidth / Mali repeatability — M3 BLOCKED (2026-10-15)

**Focused investigation complete; STOPPED at the fixed repeatability gate.**
M2's seven exit checks remain complete. M3 fails the unchanged **<=12 ms median
and <=14 ms p95 ** complete world-plus-HUD requirement. No M4 work, commit, push
or production deployment. All earlier sections below are historical evidence.

## What causes the remaining slow pictures?

The previous candidate's69 pictures >16.693 ms are all accounted for in
`slow-analysis.json` and `frame-features.csv`; none is discarded as an outlier.
Their median visible quads/draws are**1528/181**, versus**951/105** across the lap.
Protected single-quad batches account for**151 of 181** slow-picture median draws,
versus 78 of 105 overall. Every protected group has exactly one quad. The current
order deliberately preserves these batches; merging/reordering them is not proved
safe by this investigation.

The p95 deficit extends beyond those 69 deadline misses:509 original candidate
pictures exceed14 ms; the slowest192 (top 5%) have median 1319 visible quads,
168.5 draws and 141 protected batches, with world submission5.957 ms and
main-thread CPU7.846 ms. This supports the same dense-world diagnosis for the
actual p95 cohort, not just the extreme deadline misses.

Completion correlation (Pearson) is 0.697 with visible quads,0.636 with draws,
0.684 with material-index transitions,0.630 with protected quads, and 0.910 with
main-thread CPU. In the original one-fence run, world submission median is
**6.965 ms** on the 69 slow ticks versus 2.942 ms overall; main-thread CPU is
8.865 versus 4.460 ms. Driver submission/processing is a substantial part of the
dense-picture cost, not just a long final GPU wait. Material-index changes are
per-instance texture lookups, not individual GL material binds; actual page
transitions medians are22 versus 19. These correlated workloads are not independent
causal coefficients.

A new full-race component run with explicit world and pen completion fences
confirms world completion median **13.040675 ms on those same69 ticks**, versus
8.653745 ms overall. World submission alone is 7.116812 versus 3.801915 ms; the
following world GPU/driver wait is 5.827245 versus 4.870681 ms. Final composition
is**4.051321 versus 3.989969 ms**, and pen completion1.768880 versus 1.761339 ms.
These fenced timings are diagnostic and cannot substitute for one-fence
acceptance. They isolate the dominant increase to world processing/submission;
they do not separately count shader-core cycles, cache misses or driver spin.

Near clipping medians 39 versus 38 quads (correlation-0.179) and glyph vertices1488
versus 1488 (correlation0.010) do not explain the deficit. HUD style/tilemap changes
show little correlation. A32x24 pixel-centre CPU coverage/depth simulation samples
155 pictures, including all 69 misses; coverage layers medians 2.520 slow versus
2.514 other, correlation-0.095. Front-to-back reduces sampled depth-passing work,
but elevated overdraw is not supported as the main explanation for the slow
cluster. This is an approximate raster/coverage model, **not a hardware fragment
counter or an exact full-resolution overdraw measurement**. All source geometry,
near clipping and group scissors are included in that model.

Thirty-three misses cluster at 3495–3567; other clusters and all individual ticks
remain in the evidence. Tick2296 is an isolated17.121 ms final completion wait
with2.038 ms world submission, unlike the main dense-world cluster. Its precise
hardware/scheduling cause is unresolved; the old943 ms inter-picture stall also
was not reproduced or claimed fixed. Remaining tail variability must be retained.

## HUD/fullscreen bandwidth investigation

The original fullscreen compositor retains world priority, text, shadow floor
arithmetic and three dynamic LUT channel lookups. All 166 observed LUT revisions
remain active; none of the 166 source tables is an identity LUT, so identity
bypass cannot be assumed. There is no blending or fragment discard in this final
pass, and it writes every channel of every 640x480 output pixel. Resource/state
inspection and bounded isolated tests address avoidable framebuffer work without
removing the original intermediate pen semantics. Previous UBO/folded LUT alternatives were **not retested**; the original texture
LUT remained selected in every run.

| Diagnostic experiment | Paired median benefit | Decision |
|---|---:|---|
| RG8 pen instead of RGBA8 |0.041560 ms pen /0.028704 ms final;0.167147 ms complete|Reject: below 0.2 ms threshold|
| Discard world depth after last use |0.219080 ms world /0.274006 ms complete|Provisional test; rejected after repeatability|
| Discard previous output color before fullscreen overwrite |0.073872 ms final /0.104264 ms complete|Reject: below 0.2 ms threshold|

The first two share a component baseline; the final-color test has its own matched
same-binary control. Component p95 improves18.715 to 18.106 ms for depth discard,
but that alone is not repeatability evidence. Fullscreen composition remains
roughly 3.9–4.0 ms. Reducing pen storage by614400 bytes does not produce enough
measured gain. Invalidating the output buffer's previous contents also produces
only a small benefit. These results do not establish framebuffer bandwidth as
the principal final-pass bottleneck. No further shader rewrite is justified here.

An exact CPU full-resolution pen-index comparison finds259 unchanged images in
3839 transitions (6.75%); seven of the 69 prior slow pictures are among them.
Caching could avoid some intermediate work, but has no isolated measured benefit
and is **not implemented or selected**. The dynamically changing HUD cannot be
omitted or treated as constant.

## Mali-G31 correctness and host validation

Actual Mali captures compare original native ordering with guarded linear
front-to-back ordering at**21 predetermined representative/difficult ticks**:
1800,1815,1936,1965,2296,2500,2643,2691,2969,3000,3457,3514,3546,3600,4200,
4800,4822,4823,4987,5400,5639. Selection includes worst completions and maximum
near/draw/protected/coverage/HUD workloads plus prior mirror/scene references.
**All world and composed RGBA bytes match**, including priority alpha and inset
scissored mirror regions. The independent original byte palette/shadow/LUT CPU
oracle matches every captured composed pixel.

A second Mali comparison validates the tested depth-discard candidate against
the same original-order reference at all 21 ticks, again byte-exact with the HUD
oracle passing. Invalidation occurs only after the last depth use; next picture
clears depth completely. World color/priority are retained. No depth values,
ties, geometry, batching, material state or source order contract is altered by
this change. The previous D16 audit (minimum 27 bins, zero ties) and runtime 8-bin
conservative guards remain; H700 actual queried renderbuffer precision is 24 bits.

Host combined RG8/depth-discard validation matches every world and composed RGBA
byte across **all 3,840 ticks and all 166 LUT revisions**. The independent HUD oracle
also passes 21 host frames;21-tick ASan/UBSan playback passes. Build/script syntax
checks pass. This is complete host coverage and sampled actual-device coverage,
not all-frame Mali capture or new user visual signoff.

Review actual Mali original-order-left/front-to-back-right images at:
`/home/bwebb/dev/namco22-decompile/raverace/native/generated/milestone3/tail-hud-review/index.html`.
Lossless raw images remain in ignored `tail-hud-device/visual-*` directories.

## Fixed alternating repeatability test and acceptance

B is the **current optimized renderer**, already using guarded front-to-back,
certified opaque shading, blank-glyph omission, original texture LUT/shadows and
buffered telemetry. C adds only dead-depth invalidation. RG8/final-color discard
remain off. Both paths use the same `probe-color` binary SHA256
`cee2c1fd92a0c035b1280deee8a994ddd606d3b04bb38a63044b363171257a0f`.
The sequence B,C,B,C,B,C was fixed before its first process, with no adaptive
retries or selection. Every process uses five warmups then all 3,840 ticks, the
same immutable capture/resources, stock schedutil configuration, no captures or
component fences, and exactly one final GPU completion fence per picture.

Complete world-plus-HUD completion, milliseconds:

| Process | Median | p95 | p99 | Worst | >16.693 ms |
|---|---:|---:|---:|---:|---:|
| repeat-1-baseline | 12.373 | 15.881 | 18.048 | 23.068 | 112/3840 |
| repeat-1-candidate | 12.112 | 16.139 | 18.446 | 22.815 | 136/3840 |
| repeat-2-baseline | 11.955 | 15.796 | 17.304 | 20.583 | 69/3840 |
| repeat-2-candidate | 11.961 | 15.507 | 17.451 | 21.895 | 77/3840 |
| repeat-3-baseline | 12.313 | 15.660 | 17.713 | 21.714 | 87/3840 |
| repeat-3-candidate | 11.912 | 15.489 | 18.969 | 22.662 | 89/3840 |

Complete end-to-end frame intervals, milliseconds:

| Process | Median | p95 | p99 | Worst | >16.693 ms |
|---|---:|---:|---:|---:|---:|
| repeat-1-baseline | 12.532 | 16.045 | 18.187 | 23.194 | 129/3840 |
| repeat-1-candidate | 12.279 | 16.324 | 18.632 | 23.006 | 149/3840 |
| repeat-2-baseline | 12.101 | 15.971 | 17.453 | 20.766 | 86/3840 |
| repeat-2-candidate | 12.115 | 15.691 | 17.622 | 22.082 | 88/3840 |
| repeat-3-baseline | 12.469 | 15.833 | 17.922 | 21.900 | 91/3840 |
| repeat-3-candidate | 12.064 | 15.655 | 19.164 | 22.855 | 100/3840 |

Paired per-tick reductions (positive favours candidate):

- Pair 1: paired median completion reduction **0.091671 ms**; interval reduction 0.090966 ms.
- Pair 2: paired median completion reduction **0.114132 ms**; interval reduction 0.114693 ms.
- Pair 3: paired median completion reduction **0.143260 ms**; interval reduction 0.140739 ms.

The paired median complete-picture gains are only 0.092–0.143 ms, below the
predeclared 0.2 ms useful-benefit threshold. Candidate completion deadline misses
increase in every pair (112→136,69→77,87→89), and p99 also regresses in every pair.
The first candidate process additionally regresses p95 from 15.881 to 16.139 ms.
**Depth invalidation is rejected for selection and remains disabled by default.**
The stronger component result did not reproduce as a sufficient one-fence gain.
The current front-to-back renderer with original HUD remains the selected
experimental path; no new optimization is accepted from this investigation.

Every process and all **23,040** timing samples are retained. Each process accounts
for 5,152,196 source commands (5,117,636 static +34,560 direct),3,677,980 visible
commands and 1,474,216 explicit existing culls. Draw counts/per-tick packets match;
no source content, frames or slow samples are removed. Original-order baseline
captures are diagnostic only and excluded from these timing results.

**The repeated candidate does not meet the fixed 12/14 ms gate.** A component
improvement cannot override the full-race failure, and a favourable median or
pooled statistic cannot hide a failed process. The depth-discard path remains an
isolated opt-in diagnostic; it is not an accepted production change. No further
performance experiments follow this fixed series. Offline replay capacity is
not paced 59.904 FPS and includes no live simulation/audio headroom validation.

## Memory, intervals and production protection

Nominal static world GPU storage remains 482570240 bytes (<512 MiB); dynamic world
229376 bytes; original requested-format HUD accounting 3336192 bytes. Tested
depth discard adds no resource allocation; rejected RG8 would save614400 bytes.
Actual 24-bit depth promotion means the old requested-D16 total is not a physical
allocation audit. Six-process measured RSS ranges **567104–567252 KiB**;
RSS includes shared/driver resources and must not be added to nominal GPU totals.
CPU diagnostic storage remains 16 MiB bounded trace,~1.84 MB gap records and 8 KiB
permutation scratch. Every trace line is kept; overflow explicitly fails.

All 3839 real gaps per process remain logged and reconciled with next-frame
interval minus completion. FIFO acquisition/preparation/uploads remain inside
completion. Trace drain is measured separately after the lap: **31.481–35.160 ms**;
full playback/load/CPU/gap metrics are retained in raw logs and `repeatability.json`.
The first interval uses first-picture start-to-completion; the remaining 3839
are actual consecutive completion intervals, consistently in both paths.
Interval misses are reported separately above; no scheduling stall is trimmed.

Final checks verify production executable/launcher/controller identity, six
original replay files, five rollback checks, world/compact-pack hashes and
restored EmulationStation. No production game is left running. Existing physics,
audio, saves and replay data are unchanged; no shared gameplay code is modified.
Generated source/binary hashes and raw/numeric evidence are archived separately
from ignored copyrighted asset/readback files.

## Exit gates

- [x] Prior M2 exit confirmed and M3-only scope maintained.
- [x] All 69 slow pictures analysed with source features and component evidence.
- [x] Bounded HUD bandwidth/intermediate investigation; unsupported changes rejected.
- [x] Actual Mali ordering and candidate parity at 21 difficult/representative ticks.
- [x] Fixed three alternating full-race pairs; all samples and intervals retained.
- [x] All 3,840 ticks/5,152,196 commands preserved per process; full host parity.
- [ ] Repeatable <=12 ms median and <=14 ms p95 complete world-plus-HUD rendering.
- [x] Production protected; documentation updated; stopped without M4 work.

Evidence: [native-tail-hud-20261015](benchmarks/native-tail-hud-20261015/).
Reproduction and limits: [TAIL_HUD_METHOD.md](native/scene/TAIL_HUD_METHOD.md).
Remaining bottlenecks are dense protected/world submission and world GPU work,
a roughly 4 ms original fullscreen compositor, and occasional unexplained tail
stalls. No acceptance concession or new implementation is authorised by this
report. Milestone 4 requires explicit user authorisation.

---

# Depth/LUT/inter-picture Milestone 3 continuation — BLOCKED (2026-10-15)

**STOPPED at the failed p95 gate.** Milestone 2's seven exit checks remain
complete. This investigation was authorised for M3 only; M4 has not begun.
No commit, push or production deployment. The previous reports below are history.

## Matched full-race acceptance result

Both runs use the same isolated ARM executable (SHA256
`b183fa0c59232ae21309b2fdd6ae759fd2a9a504674a956c644b525d49a506df`),
original texture LUT/shadows, certified opaque shader, transparent glyph omission,
RAM telemetry, five warmups, all 3,840 ticks (1800–5639), and **one final GPU fence**.
Only ordinary unprotected opaque batch ordering differs. Component-fence runs
are excluded from acceptance. Every slow sample is retained.

| Measurement (ms) | Current optimized baseline | Linear front-to-back candidate |
|---|---:|---:|
| World + HUD completion median | 12.416897 | **11.943376** |
| Completion p95 | 15.914152 | **15.300262** |
| Completion p99 | 18.292973 | 17.260225 |
| Completion worst | 24.301275 | 20.666297 |
| Completions >16.693 ms | 115/3840 | 69/3840 |
| Complete frame interval median | 12.573433 | 12.086391 |
| Interval p95 | 16.084062 | 15.458131 |
| Interval p99 | 18.453509 | 17.415428 |
| Interval worst | 24.431145 | 20.850789 |
| Intervals >16.693 ms | 126/3840 | 71/3840 |
| Peak RSS (KiB) | 567164 | 567192 |
| Post-lap trace drain (ms) | 33.731647 | 34.467247 |

The candidate passes **<=12 ms median** but fails **<=14 ms p95**. This single
matched pair establishes a measured benefit, not repeatable acceptance or paced
59.904 FPS. No further performance experiments or favourable-run selection were
performed after failure. Interval and completion statistics remain separate;
post-lap telemetry drain is explicitly reported rather than charged to a fictional
next frame. Full lap/playback wall times remain in the raw logs.

## World depth rejection: bounded improvement

`audit_depth_order.py` binds to the opacity-certified pack and checks all
**5,152,196** source commands, painter keys, synthetic ranks and conservative D16
quantisation. Minimum floating depth separation is0.000834047794342041,
minimum quantised separation **27 D16 steps**, **zero ties**, exceeding the8-step
guard. Actual H700 renderbuffer precision queries **24 bits** despite requesting
D16. The D16 audit is conservative; it is not a formal model of proprietary
interpolation/clipping precision.

Reordering is restricted to57,639 compatible ordinary, unprotected opaque groups.
Group boundaries, source IDs, draw calls, material/texture/depth/priority state and
protected/near order are preserved. All315,000 protected and157,931 near indices
stay in order. The source sequence is strictly descending inside every eligible
group, so linear reversal exactly equals ascending stable depth sort for the
entire corpus (3,176,880 permutation positions change). Runtime separation guards
fail rather than silently reordering unsupported data. No geometry is removed;
3,677,980 visible commands plus1,474,216 explicit existing culls account for all
5,152,196 commands. Original simulation, animations and capture packets are read-only.

A general qsort prototype saved0.178 ms paired median world time but added0.508 ms
preparation and regressed complete timing0.296 ms: rejected. The evidence-supported
linear variant adds0.0485 ms preparation, saves0.1568 ms paired median world time,
and saves0.1651 ms paired complete time in diagnostic component runs. Draws remain
105 median /172 p95 /241 maximum. The final one-fence pair above includes every
preparation cost and provides the actual acceptance result.

## LUT folding: exact tested output, insufficient benefit

`hud_folded.frag` and private generated HUD sources preapply the current dynamic
LUT to palette RGB and precompute three shadow-channel lookup rows using the
original integer floor/multiplier rule. World RGB still uses the original three
LUT reads. Palette, LUT or multiplier changes rebuild tables; shadow enable and
priority rules remain original. All166 observed LUT revisions are retained.
The rejected UBO implementation is **not compiled or revisited**.

Diagnostic baseline final composition median3.985649 ms versus folded3.962549 ms
is only about0.023 ms, below the declared0.2 ms useful-benefit threshold. It does
not establish a worthwhile improvement. **Folding is disabled in the final
candidate**; original dynamic texture LUT/shadow processing remains selected.
This avoids adding cache complexity for an unproven gain. The final fullscreen
composition remains a substantial bottleneck (~3.96 ms in fenced diagnostics).
Unfenced substage submission spans are not independent GPU completion timings.

## Inter-picture stalls: logging is demonstrated; historical long stall unresolved

All3,839 end-to-next-start gaps are split into GL checks, telemetry, FIFO release
and residual bookkeeping/instrumentation/scheduling, with thread CPU and context
switch counters. Each gap is checked against next interval minus next completion.
FIFO readiness acquisition remains inside complete-picture timing.

Matched synchronous/RAM logging diagnostics show logging wall p95 **6.720439 to
0.135661 ms**, while median is about0.075 ms in both. The worst synchronous gap
13.871776 ms includes13.783529 ms logging wall but only0.261375 ms logging CPU,
and a voluntary context switch: synchronous output blocked. RAM's worst gap is
0.768844 ms; FIFO release there takes0.582018 ms wall versus0.130958 ms CPU with
nine involuntary switches, evidence of scheduling interruption rather than an
inherently slow queue operation. FIFO release p95 stays about0.033 ms; residual
p95 about0.047 ms. RAM eliminates460 voluntary switches in those measured gaps;
remaining involuntary switches are retained. It improves intervals, not proven GPU
completion: the logging diagnostic completion medians12.110/12.314 ms fluctuate.

RAM logging keeps every formatted line in a16 MiB bounded buffer; overflow fails,
formatting remains timed, and2.76 MB is drained after the lap (~35 ms). Final
candidate gap median/p95/p99/worst is0.125412/0.215616/0.262115/0.615434 ms.
The previous943 ms historical gap did **not** reproduce. These results isolate a
real logging stall but do not prove every historical stall is fixed. Context
switch evidence is not an exclusive scheduler timer.

## Fidelity, validation and memory

Full source validation passes all3,840 packets and5,152,196 commands. The combined
qsort+fold full host sequence is **byte-identical in world and composed RGBA** to
the current optimized renderer at every tick, including all166 LUT revisions.
The independent CPU HUD oracle passes11 linear-candidate host samples. Six GL
fixtures freeze LUT/tilemap while varying palette, shadow enable and multiplier
0/128/255; folded/original outputs match byte-for-byte. ASan/UBSan runs over21
actual ticks pass for both implementations. The linear permutation is independently
proven equal to the fully compared sorted sequence on all3,840 frames.

**Limit:** no new candidate Mali image capture or user visual signoff was obtained
before the performance gate failed. Earlier11 Mali comparisons belong to the
previous optimized renderer. Host equality plus conservative depth proof supports
the implementation, but must not be represented as all-frame H700 image validation.
Further device raster validation would be required before accepting this path.

Persistent static world GPU storage remains482,570,240 bytes (<512 MiB);
world dynamic storage229,376 bytes; historical requested-format HUD accounting
3,336,192 bytes. No selected GPU allocation is added. The driver reports24-bit
depth, so requested-D16 byte accounting is not a physical driver allocation audit.
Rejected LUT folding would add3,072 GPU bytes beyond the old HUD counter.
CPU diagnostics add16 MiB trace capacity, about1.84 MB gap records and8 KiB visible
permutation scratch; measured candidate peak RSS is567,192 KiB. RSS includes
resident/shared driver resources and cannot be added to nominal GPU totals.

Production executable/launcher/controller hashes, all six replay files, immutable
world/compact-pack hashes and five recovery backup checks are verified unchanged.
EmulationStation is restored; production playable is not replaced or launched.
Physics, audio, saves and original replay inputs are not modified by this work.

## Exit gates and evidence

- [x] Prior milestone exit confirmed; M3-only scope retained.
- [x] Depth/ties/painter audit, constrained implementation and matched full race.
- [x] LUT folding investigated; rejected on insufficient measured benefit.
- [x] Logging/FIFO/scheduling gaps measured without sample exclusion.
- [x] Source/host validation, memory and distributions documented.
- [ ] New candidate device visual acceptance (stopped at performance failure).
- [ ] <=12 ms median **and** <=14 ms p95 complete world-plus-HUD rendering.
- [x] Production preserved; investigation stopped; no Milestone4 work.

Numeric/raw evidence: [native-depth-lut-20261015](benchmarks/native-depth-lut-20261015/),
particularly `final-pair.json`, `depth-proof-linear.json`,
`full-host-comparison.json`, `component-gaps-summary.json`, `linear-paired.json`,
`fold-cache-fixture.json`, sanitizer logs and `production-final.txt`.
Method/build switches: [DEPTH_LUT_GAP_METHOD.md](native/scene/DEPTH_LUT_GAP_METHOD.md).
Generated assets and full RGBA comparisons remain ignored under
`native/generated/milestone3/`; no extracted assets are added to Git.

---

# Fragment/HUD Milestone 3 continuation — BLOCKED (2026-10-15)

**Investigation complete; STOPPED at the failed full-picture gate.** The user
authorised this focused M3 continuation only. M2's seven exit checks remain
complete. No M4, commit, push, deployment or production changes.

The selected standalone candidate combines certified opaque-world shading with
omission of provably transparent HUD glyph tiles. The dynamic texture LUT,
shadows, palette and priority compositor remain original. An equivalent UBO LUT
prototype was measured and rejected because it was slower. Full one-fence
completion improves from **17.418/22.714 ms median/p95 to12.310/15.648 ms**, but
**both <=12 ms median and <=14 ms p95 targets still fail**. All3840 samples are
retained;95 completions exceed16.693 ms. No further tuning after this failure.

## Opacity proof and world result

`native/tools/audit_opacity.py` rechecks every v3 manifest/file hash, all5403 native
material padded rectangles (120,021,079 texels), and every original static/direct
UV corner across5,152,196 commands. All material alpha bytes are255; minimum
mapped-corner guard is2.499512 texels. With unchanged nearest/clamp sampling,
positive-depth clipping and perspective interpolation remain inside each UV hull.
Transparent page padding outside those bounds cannot be sampled. This is a proof
about the actual RGBA pack, not an assumption about original ROM transparency.

`prepare_fragment_shaders.py` reruns that proof and binds selection to the pack
manifest SHA. For this all-opaque pack it selects `race_opaque.frag`, removing only
`if(t.a==0.0)discard;`. Texture reads, RGB/brightness math, near clipping, synthetic
depth, priority alpha, source ordering/scissors and disabled-fog policy remain
unchanged. Original `race.frag` is kept as `cutout.frag`; any unproved material
conservatively selects the original discard shader for the entire pack. No mixed
material batching or unsupported future cutout policy is assumed.

Full-window split-fence world completion changes from12.773/17.531 ms median/p95
to**7.920/11.377 ms**. No world commands, materials or objects are removed. The
source packet counts, visible list and draw counts remain identical.

## HUD pass attribution and bounded candidates

`build_hud_probe.py` derives private copies of the original HUD/video layer;
production/shared sources are never edited. Controlled world and pen fences
separate pen preparation/upload/render completion from final palette/LUT uploads
and fullscreen composition. Split fences perturb Mali tile scheduling and are
not acceptance timing. Shadow/LUT omissions change the image and are only cost
sensitivity controls; they cannot satisfy the milestone.

| Full-window diagnostic | Pen median / p95 (ms) | Final median / p95 (ms) |
|---|---|---|
| Original HUD |2.785 /3.160|4.069 /4.520|
| Shadow selection disabled |2.826 /3.195|4.061 /4.525|
| LUT reads bypassed |2.776 /3.148|2.978 /3.384|
| Same candidate executable, both options off |2.838 /3.233|4.070 /4.525|
| Transparent-tile omission only |1.811 /2.204|4.015 /4.495|
| Equivalent LUT UBO only |2.850 /3.238|4.870 /5.762|
| Shadow arithmetic bypass only |2.840 /3.211|4.018 /4.489|

Shadow-disabled selection changes the branch to a palette lookup, so it is not an
exclusive shadow arithmetic timer. A refined control keeps shadow selection but
bypasses multiplication/floor; paired final reduction is only0.055 ms median.
The full baseline has about13,167–13,184 visible shadow pixels in eleven sampled
scenes (independent CPU compositor counts), and shadows remain enabled. Neither
shadow omission is an improvement. LUT bypass gives1.092 ms median paired final
reduction, justifying an isolated LUT transport test, not removal of dynamic LUTs.

`build_hud_candidates.py` supplies two independent, default-off switches:

- `ENG_NATIVE_HUD_CULL=1`: scan actual current glyph bytes; omit only tiles whose
  full128-byte glyph is0xff (all pen15, which the original glyph shader always
  discards). Sampled scenes have950–953 such tiles out of1200, retaining247–250.
  Native text, palette, scroll, shadow and LUT state remain intact. Blank/nonblank
  changes invalidate geometry even when the tilemap is unchanged; nonblank glyph
  changes retain the original dirty texture/pen updates. H700 pen median falls
  by about1.027 ms in the matched component test. **Selected.**
- `ENG_NATIVE_HUD_UBO=1`: GLES3/std140256-vec4 buffer holds the same normalised
  dynamic LUT bytes, with three indexed reads replacing three LUT texture reads.
  Block support/size is queried. All166 LUT styles remain active. Full host
  output matches, but H700 final composition becomes slower: paired reduction
  **-0.827 ms** median. **Rejected, disabled in the selected candidate.** Its
  code/shader is retained only to reproduce this failed isolated experiment.

These are native standalone options generated beside the original code; no live
HUD, emulator/scheduler or game integration is attempted. Original shaders and
option-off behaviour remain available. The selected candidate uses original
texture LUT composition, including every dynamic update and original shadows.

## Matched full-race one-fence acceptance — FAILED

Same candidate executable, immutable pack and source bytes, all3840 original ticks
1800–5639, five separate warmups, stock OS/governor, no transfers/readbacks or
presentation swaps. Baseline: original world shader, both HUD switches off.
Candidate: certified opaque shader, transparent-tile omission on, UBO off.
Only one final world+HUD `glFinish`; queue readiness waits are timed. Producer
read/inflate CPU is reported independently and overlaps main-thread work.

| Complete world+HUD | Median | p95 | p99 | Worst |
|---|---:|---:|---:|---:|
| Matched original path |17.418|22.714|24.228|27.469|
| Selected candidate |12.310|15.648|18.804|22.349|
| Candidate completion interval |12.636|19.711|22.195|965.300|

Candidate completions >14 ms:671/3840; >16.693 ms:**95/3840**, versus2190 baseline.
Intervals >16.693 ms:446 candidate /2392 baseline. Original interval median18.032,
p9525.076,p9929.175,worst176.077 ms. Candidate offline wall capacity73.979 pictures/s,
inverse mean slowest1% intervals19.352 pictures/s; baseline54.179/26.137. These are
**not displayed FPS, live gameplay throughput or consistent59.904 FPS**.

Every interval is retained, including candidate tick4439 interval965.300 ms versus
its21.756 ms measured completion. The943.544 ms gap is between prior completion
and the next picture start, where slot bookkeeping, synchronous telemetry output
and scheduling run. Filesystem logging is a plausible contributor, not an isolated
proven storage timer. Baseline has a152.600 ms analogous gap at5184. Do not hide
these hitches, claim smooth pacing, or blame them for the failed12/14 ms completion
gate: the completion distribution itself already fails. No telemetry redesign,
extra favourable-run selection or further optimisation was performed after failure.

Main-thread CPU median5.370→4.648 ms (candidate p957.618); world submission
3.546→3.113 ms median; final combined GPU/driver wait11.813→7.811 ms (candidate
p959.089). These are CPU/submission/completion spans, not exclusive GPU times.
Candidate worker CPU median2.059 ms,p953.282; readiness wait median0.0011 ms,
worst0.129. Transport starvation is not the dominant current measured cost.

## Memory, coverage and validation

Selected static world GPU bytes**482,570,240** (<512 MiB full-lap ceiling), world
dynamic allocation229,376 bytes, unchanged HUD fixed allocation3,336,192 bytes;
no UBO allocated in the selected run. Three512 KiB FIFO slots plus512 KiB compressed
scratch remain bounded. Blank-status cache adds1024 boolean entries and bounded
scratch; the original worst-case CPU glyph array remains. Normal world uploads
median35,840/max64,896 bytes and world draws105 median/172 p95/241 max are unchanged.
Actual HUD transfer totals fall from516,390,784 to107,414,440 bytes across3845
pictures including warmups. Selected glyph vertices range1476–1500; its
observed dynamic glyph VBO peak is30000 bytes, separate from fixed HUD bytes.
Rejected UBO adds4096 GPU bytes and its revision uploads; it is not charged to the
selected candidate. CPU/GPU resources and driver mappings overlap process RSS.

Peak RSS: baseline567320 KiB / selected567164 KiB / rejected UBO568396 KiB. Selected
startup including five warmups27.240 seconds, FIFO prefill4.122 ms; baseline27.180
seconds/4.162 ms. No in-lap static page/mesh decode, eviction, miss or upload.
Loading remains large and is explicitly outside steady-state completion timing.

Validation evidence in[the benchmark directory](benchmarks/native-fragment-hud-20261015/):

- Full source validation again passes all3840 ticks,5,152,196 source records,
  original transforms/materials/priority, all cull reasons and exact HUD bytes.
- All3840 selected host world/composed images compare against the previous native
  sequence. **Zero world priority-alpha changes; ten world and ten composed pixels
  differ total**, one each in ten frames. Isolated world raster/depth edge outcomes
  change with the no-discard shader; this is visually negligible, not bit identity.
  Expanded edge views were inspected; all other pixels agree.
- Selected transparent-tile omission with original texture LUT is byte-identical
  to the original HUD over all3840 host pictures on identical opaque-world frames.
  Both HUD options together also match all3840 host frames/all166 LUT styles,
  although the slower UBO remains rejected.
- Actual GL cache fixture freezes tilemap and changes glyph32 through transparent,
  pen0,pen1,transparent states. Culled vertex counts1488,7068,7068,1488,1488,7068;
  all six full compositions match unculled output exactly. This covers glyph
  invalidation independently of text changes.
- ASan/UBSan host playback2490–2510 for two passes (42 real frames plus warmups)
  reports no memory/undefined behaviour finding. Host/ARM builds are warning-clean;
  Python syntax and shell syntax checks pass.
- An independent byte-level palette/shadow/LUT oracle first passes all pixels of
  eleven original Mali captures. Its initial diagnostic implementation had a
  uint16 glyph-offset overflow above code511; widening before multiplication fixes
  it, and the original Mali oracle now passes. Only corrected results are evidence.
  Selected Mali image validation is recorded below after the diagnostic capture.

No later course/replay is invented. All actual animated source instances/direct
geometry remain represented; unsupported point-RAM/solid cases remain explicit
failures outside this captured window. All copyright-bearing outputs are ignored
local artifacts; only numeric/source/hash evidence is suitable for Git.

## Final Mali fidelity and production verification

The selected visual diagnostic executes all3840 ticks and reads back world plus
composed pictures at1800,1965,2500,2643,3000,3546,3600,4200,4800,5400,5639.
All eleven world and composed images are **byte-identical to the prior native
Mali captures**, including priority alpha. The independent CPU palette/shadow/LUT
oracle matches every pixel in all eleven selected compositions. Original-left /
selected-Mali-right contact views were inspected for scenery, traffic, mirrors,
shadow/HUD layering and continuity; no visible regression is present. This is
sampled device fidelity plus full host coverage, not all-frame Mali readback or
a new user subjective signoff. Its readback timings are excluded from acceptance.
Open `native/generated/milestone3/fragment-hud-review/h700/index.html` for review.

Production executable SHA remains
`6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`;
launcher/controller identities match, all six original replay files match initial
copies, all five backup checks pass, immutable pack/page checks pass again.
HUD mode`default`, replay`off`; sole Ports launcher remains, EmulationStation
restored, no production game running. Source build identities and final device
hashes are archived. Shared gameplay/engine/audio sources have no diff; HEAD and
production are unchanged. **No commits, pushes or deployment.**

## Reproduction and stop boundary

```sh
python3 raverace/native/tools/prepare_fragment_shaders.py raverace/native/generated/milestone3/race-v3 /tmp/rr-certified-shaders-new
python3 raverace/native/tools/build_hud_candidates.py /tmp/rr-hud-build-new
cc -O2 -Wall -Wextra -DRR_GPU_HUD -Iraverace/include -Iengine /tmp/rr-hud-build-new/probe.c -o /tmp/rr-hud-new $(pkg-config --cflags --libs sdl2 glesv2) -lm -lz -pthread
SDL_VIDEODRIVER=offscreen ENG_NATIVE_HUD_CULL=1 /tmp/rr-hud-new raverace/native/generated/milestone3/race-v3 /tmp/rr-certified-shaders-new 1800 5639 1 -
python3 raverace/native/tools/analyse_compact_race.py raverace/benchmarks/native-fragment-hud-20261015/baseline-one-fence-1.txt raverace/benchmarks/native-fragment-hud-20261015/candidate-one-fence-1.txt --out /tmp/rr-fragment-summary.json
```

ARM uses the existing H700 Buildroot compiler with`-lSDL2 -lmali -lm -lz -pthread`.
Device BASE:`/userdata/system/rave-racer-native-m3-fragment-hud-20261015`.
Runner:`sh BASE/run_fragment_hud_h700.sh BASE NEW_RUN WORLD CONTROL SPLIT CULL UBO
EXE [off|on]`; WORLDoriginal/opaque, CONTROL0full/1shadowoff/2LUTbypass/3shadowmathbypass,
SPLITnone/world/hud. Final pair uses same`BASE/candidates`, CONTROL0,SPLITnone,
CULL0/1,UBO0. Only CONTROL0,SPLITnone,no capture is headroom-eligible. The analyser
now marks split/omission diagnostics ineligible rather than printing a gate pass.

Data/review root remains
`/home/bwebb/dev/namco22-decompile/raverace/native/generated/milestone3/`.
Selected full host frames:`fragment-hud-selected-sequence/`; selected handheld
readbacks:`fragment-hud-device/selected-visual/`; certifier outputs:
`fragment-hud-certified-shaders/`. Raw readbacks and viewport comparisons are
visual diagnostics only, never timing evidence. No images depend on host`/tmp`.

**Remaining blockers:** dense-world submission/completion tail and the original
final composition cost still leave insufficient headroom; inter-picture stalls
also need attribution. The equivalent UBO path is rejected, not a future default.
A next bounded M3 proposal should test front-to-back ordering only within proven
opaque, compatible, unprotected batches, first proving quantised-depth/tie safety
and preserving every source ID. Current compilation retains painter-derived order
inside compatible groups; measure whether early depth rejection reduces the dense
world tail without extra draw calls. Also measure folding the current LUT into
palette/shadow lookup resources while keeping world gamma/fade exact, before any
compositor change. Separately isolate telemetry/scheduling gaps without deleting
their evidence.
No such work is implemented or authorised by this stopped iteration. Do not start
M4. Production identities/rollback checks are recorded in the final verification.

---

# Focused Milestone 3 continuation — profiling complete; BLOCKED

## Focused Milestone 3 continuation — profiling complete, BLOCKED (2026-10-15)

The user authorised profiling and a conditional indexed-buffer rewrite. The
investigation is complete; **M3 remains BLOCKED and STOPPED**. M2's completed
seven-check gate remains valid. No M4, commit, push or production deployment.

Controlled full-window diagnostics isolate world completion at **12.213 ms median /
17.335 ms p95**, and subsequent HUD at **5.918 / 6.415 ms**. A cheap-fragment
control reduces world to6.101/9.156 ms. Removing all vertex texture gathers and
using four indexed ordinary corner invocations gives11.759/17.119 ms; its median
per-tick paired world saving is **-0.055 ms** (slightly slower). That does not
support a full persistent indexed-buffer rewrite as the next fix. The CPU-prepared
vertex control is diagnostic only, not a new accepted native backend.

The unchanged one-fence reference still fails: world+HUD **17.553 ms median /
22.435 ms p95 /23.767 ms p99 /27.180 ms worst**, against <=12/14 ms.
2197/3840 pictures exceed16.693 ms. No acceptance criterion was relaxed.
All3840 ticks,5,152,196 commands and exact material/transform/HUD source bindings
validate again. Original renderer/shader sources remain hash-identical; prior
full-sequence fidelity evidence remains valid. Six full-window diagnostic runs
and a full host vertex-control lap are accounted for. The incomplete controls
are not performance/visual acceptance evidence.

See [current race report](NATIVE_RACE_REPLAY_REPORT.md),
[profiling method](native/scene/PROFILE_METHOD.md), and
[logs/distributions](benchmarks/native-race-profile-20261015/).
Production executable/launcher/controller, six replay files and five rollback
checks are unchanged; HUDdefault/replayoff, EmulationStation restored. Shared
simulation, physics and audio were not modified or linked to the diagnostic.

Next proposal, **not implemented or authorised**: a bounded M3 fragment/HUD
investigation, first proving recipe opacity and measuring an opaque shader path
without alpha discard only where source coverage allows, while preserving an
alpha-tested path for cutouts. Profile the unchanged HUD's pen pass and final
LUT/shadow composition separately before proposing any equivalent compositor
change. Revalidate all ticks and fidelity, then a matched one-fence full-race
<=12/14 ms gate. M4 remains unauthorised; do not begin it.

### What was measured

The authorised continuation was bounded to attribution, a conditional indexed
native backend only if justified, and the unchanged <=12 ms median / <=14 ms p95
complete world+HUD gate. No speculative backend rewrite was undertaken. Existing
v3 uses persistent original mesh/material textures and bounded compressed FIFO;
all original scene records and authoritative state remain unchanged.

The derived `build_profile_race.py` executable adds thread CPU spans to the
unchanged renderer. `ENG_NATIVE_STAGE_SYNC=1` finishes world work before the
unchanged GPU HUD; the normal run retains one final whole-picture `glFinish`.
All3840 ticks 1800–5639 execute in each process, with five separate warmups. No
capture/readback, file transfer, presentation/swap, simulation or audio occurs
in timed H700 laps. The stock `schedutil` governor and OS/clock policy are kept;
these are not locked-frequency confidence intervals. Each control is one full
independent process, not a selected subset or repeated frozen frame.

| Full-window diagnostic | World span median / p95 / p99 / worst (ms) | HUD span median / p95 (ms) |
|---|---|---|
| Original shaders + split fences |12.213 /17.335 /18.584 /22.354|5.918 /6.415|
| Rasterisation discarded |0.737 /1.491 /1.925 /12.185|5.278 /6.020|
| World draw calls skipped |0.624 /1.067 /1.592 /4.126|5.678 /6.282|
| Cheap world fragment, active UV/brightness varyings |6.101 /9.156 /11.105 /15.633|5.398 /5.912|
| No vertex texture gathers; indexed ordinary corners |11.759 /17.119 /18.249 /21.477|5.818 /6.391|

World span is each picture's world submission plus world fence. HUD span is
subsequent original composition plus final fence. These are serial wall spans,
not exclusive GPU timer queries. Controlled fences force tile completion and
change scheduling; diagnostic sums are not one-fence acceptance predictions.
Raster discard may eliminate unobservable vertex work. The cheap-fragment
shader keeps source UV/material and brightness varyings active, but changes
image reads, alpha discard, shading and depth/overdraw semantics. Its5.785 ms
median **paired per-tick** world reduction demonstrates substantial sensitivity
to that combined fragment/visibility workload, not a precise texture-only cost.

The no-gather control streams CPU-transformed original positions and UVs through
attributes, with the same original fragment shader/depth/priority/HUD. Ordinary
quads use a persistent six-index buffer and four unique corner invocations;
near clipping uses the original GPU clipping algorithm. It removes all source,
instance and material vertex texture fetches. Extra CPU preparation and upload
remain in the logs. It deliberately repeats CPU geometry work and therefore is
**not the requested persistent native-mesh backend** or a deliverable that can
pass M3. It is a cost sensitivity control, not a mathematical upper bound: its
attribute traffic/driver behaviour differs from any future indexed implementation.
The marginal world medians differ by0.453 ms, but the median paired difference
is-0.055 ms; no repeatable speedup or statistical confidence interval is claimed.
It does not justify a rewrite aimed at closing a5–8 ms complete-picture deficit.

### CPU, completion and driver attribution

For original split-fence rendering, world group-loop thread CPU median3.356 ms
(p956.112), world fence CPU0.327 ms, HUD function CPU0.987 ms and final fence
CPU0.250 ms. Original world fence wall wait median8.282 ms and final HUD fence
wait4.894 ms (see logs for exact distributions). Draw-skip retains state handling
but gives world group CPU0.209 ms; raster-discard gives0.439 ms. Thus draw/driver
feeding and its graphics backpressure are measurable, while the bulk of the
serial completion delay is outside main-thread CPU execution. CPU spans include
application checks and proprietary driver calls; wall-minus-CPU is not an
exclusive GPU timer. No unsupported claim of a separate exact driver-only or
GPU-only duration is made. Source preparation and queue starvation are small.

The no-gather control's group CPU3.091 ms / p955.100 ms offers no compelling
submission-cost advantage. It also increases vertex attribute traffic. The
HUD remains around5–6 ms in all split-fence controls even when the world is
suppressed. The actual HUD LUT has166 distinct revisions in this window and
is not an identity RGB LUT; skipping it would change the game image.

### Complete-picture headroom result — still failed

Only complete original rendering with one final fence counts here. The same
instrumented executable in normal mode is compared with the prior v3 run under
the same completion definition, full input window, warmup and capture policy.
Additional profiling clock calls are present in the new reference. This is a
reference repeat, **not an indexed-backend speedup comparison**.

| One-fence complete world+HUD | Median | p95 | p99 | Worst |
|---|---:|---:|---:|---:|
| Previous v3 reference |17.418|22.527|23.882|32.617|
| Current normal reference |17.553|22.435|23.767|27.180|
| Current completion intervals |18.154|25.099|28.907|33.360|

Current normal completion exceeds14 ms on3423/3840 pictures and16.693 ms on2197;
intervals exceed16.693 ms on2419. Offline capacity54.343 pictures/s, inverse mean
slowest1% intervals33.030 pictures/s. Main-thread CPU median5.436/p958.238 ms;
worker CPU2.558/3.450 ms overlaps. Queue wait median0.0014 ms / worst0.018 ms.
The combined final fence wait median11.834 ms includes outstanding world+HUD.
These are not live FPS, display cadence or proof of eventual59.904 FPS.

### Resources and integrity

Unchanged v3 static GPU bytes482,570,240 (460.21 MiB), bounded world dynamic GPU
allocation229,376 bytes, three512 KiB raw FIFO slots plus512 KiB compressed
scratch, one reusable inflater, and separate unchanged HUD allocations. Original
HUD fixed GPU allocation3,336,192 bytes. Current normal peak RSS567260 KiB,
startup25.706 seconds including five warmups, initial FIFO prefill7.637 ms.
Normal world upload median35,840 bytes / max64,896. HUD transfers remain
516,390,784 bytes across3845 pictures, as in the prior revision. All static
mesh/page/material files pass the existing manifest SHA checks after profiling;
none is decoded/reallocated/uploaded during race playback.

The no-gather diagnostic retains original GPU resources for controlled comparison,
adds a458752-byte streamed attribute buffer and six-byte persistent index buffer
(total nominal world dynamic allocations688134 bytes), and retains the2,015,232
byte CPU source mesh shadow. Peak process RSS567280 KiB; RSS includes mapped
resources and must not be added to GPU bytes as a disjoint cost. Its raw logs
report the increased uploads rather than hiding them.

An actual-data indexed-design sizing audit finds170418 source-instance/mesh/material
prototypes, about21,813,504 bytes for four32-byte native vertices each plus a
5,455,872-byte prototype lighting-state table. It estimates248 median /431 p95 /
567 maximum draws after splitting ordinary groups by instance (vs105/172/241
currently). This is a design estimate, not an implemented format or proof of a
collision-free live entity identity; source list IDs are not persistent entities.
It exposes increased driver/state traffic and a roughly27 MB additional resource
cost without a measured vertex bottleneck. No such asset pack/backend was built.

All3840 source packets/5,152,196 original records validate again with zero source,
material, priority or transform errors; immutable data/HUD bytes and every explicit
cull reason are checked. All existing renderer/shader hashes equal the pre-task
snapshot. The unchanged reference preserves the previous3840-frame host parity
and eleven actual Mali comparison frames. Profiling normal mode at tick2500 is
byte-identical to its host reference. The no-gather diagnostic executes a full
host lap and eleven sampled captures differ by0–13 pixels each, mean RGB error
at most0.000609/255, consistent with CPU float/affine clipping order. No new user
visual signoff or all-frame hardware readback is claimed. Incomplete controls
are explicitly excluded from fidelity acceptance.

### Reproduction and stop boundary

From repository root, with the ignored existing v3/source assets:

```sh
python3 raverace/native/tools/build_profile_race.py /tmp/race-profile.c
cc -O2 -Wall -Wextra -DRR_GPU_HUD -Iraverace/include -Iengine /tmp/race-profile.c -o /tmp/race-profile $(pkg-config --cflags --libs sdl2 glesv2) -lm -lz -pthread
python3 raverace/native/tools/build_vertex_control.py /tmp/vertex-control-new
cc -O2 -ffp-contract=off -Wall -Wextra -DRR_GPU_HUD -Iraverace/include -Iengine /tmp/vertex-control-new/control.c -o /tmp/vertex-control $(pkg-config --cflags --libs sdl2 glesv2) -lm -lz -pthread
python3 raverace/native/tools/analyse_race_profile.py raverace/benchmarks/native-race-profile-20261015/staged-full.txt raverace/benchmarks/native-race-profile-20261015/staged-vertex-control.txt --out /tmp/profile-summary.json
python3 raverace/native/tools/analyse_compact_race.py raverace/benchmarks/native-race-profile-20261015/reference-normal.txt --out /tmp/reference-summary.json
```

H700 builds use the existing Buildroot ARM64 compiler and`-lSDL2 -lmali -lm -lz
-pthread`; the no-gather control additionally uses`-ffp-contract=off`. Device BASE
is`/userdata/system/rave-racer-native-m3-profile-20261015`. Run
`sh BASE/run_profile_h700.sh BASE NEW_RUN MODE SPLIT [SHADER_DIR [EXE]]`.
Modes0/1/2 are full/raster-discard/draw-skip; SPLIT0/1 selects one/two fences.
Cheap fragment uses original vertex shaders plus`race_profile.frag` renamed to
`race.frag` in a private shader directory. No-gather uses its generated shader
directory and executable. Exact build, shader, input and production hashes are
archived in[profiling evidence](benchmarks/native-race-profile-20261015/).

The raw generated captures are durable ignored files under
`native/generated/milestone3/profile-vertex-visual/`; their log and eleven frames
are diagnostics. Existing reviewed original/native images remain under
`native/generated/milestone3/review/` and`v3-h700-review/`. No copyrighted assets
are added to tracked evidence. Numeric logs, distributions, original-source
validation and hash evidence are in the benchmark directory.

**Decision:** investigation complete, no evidence-supported full indexed rewrite,
headroom still failed. No further optimisation campaign. A new bounded M3
proposal should investigate verified opaque materials/alpha-tested overdraw and
separately measure the HUD pen/final-LUT/shadow passes, retaining exact cutouts,
priority and changing LUTs. Only a separately authorised implementation may test
that proposal against the same full-picture12/14 ms gate. M4 is not started.
Production, launcher, controller, saves/replays and rollback remain protected;
EmulationStation is restored and the game is not left running. **STOPPED.**

---

# Historical revised Milestone 3 result

# Revised Milestone 3 — BLOCKED (2026-10-15)

## Revised Milestone 3 — BLOCKED at measured headroom gate (2026-10-15)

The authorised focused revision is implemented and tested. M2's seven exit checks
remain complete. **M3 is not complete: STOPPED at the failed H700 performance
gate; M4 is not authorised or started.** See the current
[race report](NATIVE_RACE_REPLAY_REPORT.md), [v3 protocol](native/scene/COMPACT_FORMAT.md)
and [raw/numeric evidence](benchmarks/native-race-revised-20261015/).

All 3840 ticks and 5,152,196 original commands validate. All 3840 revised host
pictures are byte-identical to prior native pictures. Eleven actual Mali captures
cover the full race and preserve the reviewed track, traffic, mirror and HUD
relationships. Static assets remain resident: 482,570,240 bytes (<512 MiB).
Compressed command payload is 107,235,244 bytes; buffering is a bounded three-slot
FIFO. ASan/UBSan two-pass host playback and malformed-stream checks pass.

Clean H700 world+HUD completion: **median 17.418 ms / p95 22.527 ms / p99 23.882 ms /
worst 32.617 ms**, versus the prospective <=12/14 ms median/p95 gate.
2203/3840 pictures exceed 16.693 ms. Intervals: median17.914 / p95 24.190 /
p99 28.476 / worst39.587 ms; offline capacity54.933 pictures/s, 1% low33.403.
These are not live FPS. Queue wait median0.0012 ms rules out packet starvation
in this run; final combined GPU wait median11.812 ms remains substantial.
One final fence is used, unlike the historical two-fence run; no matched-control
speedup claim is made. The failed gate stopped further performance tuning.

Production executable/launcher/controller hashes, default HUD/off replay modes,
all six replay files and five rollback checks remain unchanged; EmulationStation
is restored and no game is running. No commits, push, deployment, live simulation
integration or OS/clock changes. Proposed next work requires separate authorisation:
attribute world/HUD GPU costs, then consider a bounded native indexed-vertex
backend to replace repeated vertex-texture gathers. Do not start M4.

Review actual handheld comparisons in the durable local directory
`native/generated/milestone3/v3-h700-review/` (original left, Mali right).
The full 3840-frame browser review remains `native/generated/milestone3/review/index.html`;
v3's host output is proven identical to that review's native side.

### Revised implementation and reproduction

`compact_race.py` deduplicates per-picture source instance state, immutable HUD
font/styles and material UV state. `compact_protocol.py` bounds decoding;
`validate_compact_race.py` compares every original source record and independently
proves conservative cull reasons. `race_v3.c` plus its regular/near vertex shaders
uses six ordinary vertices, shared instances and an isolated three-slot producer
FIFO. The existing production GPU HUD source is included without edits. There
is no simulation/audio linked to this diagnostic executable.

Run from the repository root (local generated assets required):

```sh
python3 raverace/native/tools/compact_race.py raverace/native/generated/milestone3/race-identity raverace/native/generated/milestone3/capture-identity raverace/native/generated/milestone3/race-v3-new
python3 raverace/native/tests/validate_compact_race.py raverace/native/generated/milestone3/race-identity raverace/native/generated/milestone3/race-v3-new /tmp/compact-validation.json
cc -O2 -Wall -Wextra -DRR_GPU_HUD -Iraverace/include -Iengine raverace/native/render/race_v3.c -o /tmp/rr-race-v3 $(pkg-config --cflags --libs sdl2 glesv2) -lm -lz -pthread
SDL_VIDEODRIVER=offscreen /tmp/rr-race-v3 raverace/native/generated/milestone3/race-v3 raverace/native/render 1800 5639 1 -
python3 raverace/native/tools/analyse_compact_race.py raverace/benchmarks/native-race-revised-20261015/h700-timing1.txt --out /tmp/compact-timing.json
```

ARM uses the existing H700 Buildroot compiler and `-lSDL2 -lmali -lm -lz -pthread`.
The already-executed device command was `sh run_compact_h700.sh BASE BASE/timing1
1800 5639 off`, with BASE `/userdata/system/rave-racer-native-m3-revised-20261015`.
That directory is diagnostic only. Its five compact pack files and nine reused
immutable world files passed SHA checks before execution. Build/shader hashes
and protected production identities are archived with the numeric logs.

| Evidence | Revised result |
|---|---|
| Source/material/priority/transform/HUD validation | 3840 ticks, 5,152,196 commands, zero errors |
| Explicit offscreen culls, with original records retained | 1,474,216; 3,677,980 visible |
| Transport | 107,235,244 compressed bytes vs 1,276,401,984 source payload bytes |
| Maximum raw/compressed packet | 95,588 / 43,959 bytes |
| Font/style revisions | 1 / 166, exact original HUD byte parity |
| Static GPU / dynamic GPU allocation | 482,570,240 / 229,376 bytes; HUD separate |
| Startup including five warmups / initial queue prefill | 25.466 s / 4.090 ms |
| Peak process RSS | 567,252 KiB (includes driver/HUD, not just static assets) |
| Draws median / p95 / maximum | 105 / 172 / 241 |
| World dynamic upload median / maximum | 35,840 / 64,896 bytes |
| Main-thread CPU median / p95 | 5.334 / 8.398 ms |
| Worker CPU median / p95 | 2.537 / 3.441 ms, overlaps main thread |
| Read / inflate median | 0.107 / 2.385 ms, producer side |
| World submit / HUD submit / final combined wait median | 3.589 / 1.251 / 11.812 ms |
| Completion >14 ms / >16.693 ms | 3401 / 2203 of 3840 |
| Interval >16.693 ms | 2380 of 3840 |

A second complete compiler run produced byte-identical manifests and all output
file hashes, including the compressed stream. Four malformed fixtures reject unknown stream/frame versions, bad zlib checksums
and oversized inflation. ASan+UBSan actual renderer playback covered ticks
2490–2510 for two passes, including FIFO wrap/pass transition, without findings;
this is not a ThreadSanitizer proof. Full-window FIFO execution also verifies
strict consecutive ticks and all command counts. Host/ARM builds are warning-clean.

All 3840 host composed images are byte-identical to corrected v2 native images.
Mali readbacks cover 1800,1965,2500,2643,3000,3546,3600,4200,4800,5400,5639,
world and composed. The original-left/Mali-right contact sheet and individual
PNGs were inspected for continuity, mirrors, scenery, traffic and HUD layering.
Host/Mali mean RGB error is 0.040–0.166/255; differing pixels include subpixel
brightness/raster edges, with maximum channel errors up to255 at boundary pixels.
This is visual fidelity evidence, not pixel identity or an all-frame Mali diff.
The hardware capture process executed all3840 ticks, but only eleven were read
back. Its timing is explicitly diagnostic and excluded from acceptance.

### Failed gate and bounded next proposal

Only one clean full-window timing process was required to establish this failure;
no repeated-pass campaign was used to select a favourable result. Queue readiness
is effectively immediate, so further storage compression cannot close this gap.
Final GL completion wait and world submission remain substantial, but these
spans cannot separate exclusive world/HUD GPU costs. Historical32.729 ms results
used two fences; current17.418 ms results use one. A same-fence v2 control was
prepared but not run after failure; **do not claim an apples-to-apples speedup**.

A separately authorised M3 revision should first measure world/HUD GPU costs
with diagnostic stage fences outside headline timing. If vertex work dominates,
replace repeated source/instance/material texture gathers with persistent indexed
native vertex buffers and per-instance transform attributes; validate identical
source identities and all-frame visual coverage before another fixed12/14 ms
gate. If HUD/fill dominates, return a measured proposal rather than assuming that
vertex changes will solve it. This is a proposal only; no additional architecture
or M4 work has begun. Current M3 remains BLOCKED and stopped.

The complete original blocked investigation below is historical evidence.

---

# Native race playback — Milestone 3

**Status: BLOCKED — full H700 playback fails the headroom exit gate. STOPPED.** User authorised Milestone 3 only.
Milestone 2 exit gate confirmed: seven revised checks complete, numeric H700
acceptance preserved, and user reviewed the two frozen comparisons as visually
indistinguishable. M4/live integration is not authorised. No commit/deployment.

The actual replay window is ticks 1800–5639 inclusive. Source capture and native
commands account for all 3840 pictures,5,117,636 static source quads and34,560
direct polygons. Source projections/UV/brightness match20,154,714 valid original
corners. No point-RAM or palette-solid polygons occur; unsupported cases reject.
The actual palette is constant. Original-model animation/camera/scissor changes,
resolved lighting, fog, direct geometry, priority and complete HUD state are
carried per picture. Frame-local instance IDs are source display-list addresses;
they are not persistent traffic/entity IDs.

See [protocol/reproduction](native/scene/FORMAT.md). All ROM-derived assets,
packets and review images remain under ignored `native/generated/milestone3`.
The separate GLES3 backend keeps original local meshes and consolidated RGBA
pages resident. Geometry attribute textures feed a native vertex shader, including
near clipping; fragment shading has one ordinary RGBA sample. This is not the
rejected dependent fragment texture-map/pen/palette pipeline or a projected-
triangle replay. Static world resources:482,471,936 bytes, below an explicit512 MiB
full-lap ceiling. The larger full-lap footprint does not satisfy/redefine the
M2 frozen-scene128 MiB target. Peak one-packet bytes454453, dynamic world upload
311760 bytes. The complete3840-packet stream is read sequentially through bounded
buffers, not retained in RAM. No live command bridge, simulation or audio is
linked. Current GPU transforms use float32/floor rather than integer64 source
math; visual sampling/subpixel differences are allowed, source fields are exact.

Before H700 measurements: retain M2's world median 12ms/p95 14ms as a useful
headroom reference, and evaluate complete native world+HUD, including packet
reads/uploads, against16.693ms. No retrospective FPS-only pass: report p50,
p95,p99,worst and all misses, with the missing simulation/audio budget explicit.
Five startup warm-ups are reported separately; no actual race ticks are dropped.
Readback, image capture and file transfer must be disabled during timed laps.
A failure to demonstrate credible headroom stops M3 with a focused redesign.

Host validation already passes: all packet/frame hashes, every source binding,
original mesh coordinates, transforms, resolved brightness and painter priorities;
all 5403 material recipes/120,021,079 footprint+gutter texels and27,015 scalar
address samples. Nine sampled native HUD states are byte-identical to original
production compositor states. Unknown protocol, truncation and oversized count
fixtures reject. A modified palette receives a different content revision.
All source/header/compiled format versions and packet bounds are checked.

Capture's private source hook initially used a replay-cleared RR_* variable,
then incorrectly relied on invalid behind-eye oracle UV fields. These diagnostics
were corrected before acceptance; final capture uses ENG_NATIVE_CAPTURE and
original raw UV/resolved lighting. Early incomplete streams are not evidence.
Explicit instance fields were added before final v2; all rendering fields across
3840 packets remain identical to the provisional host renderer's full lap.

The input recording's54 shared simulation/board/audio checkpoint records through
5400 match the original recording; the5639 endpoint matches untouched headless
and original GLES runs byte for byte, including PCM/voice digests. Original input
sidecars are read only. Reference captures are diagnostics, not benchmark runs.

Full-reference sequence capture initially had two paced reference omissions at
3001/3002; every native command/picture existed. A private every-picture original
reference relink recovered those two pictures, with identical complete replay
checkpoints. The reference review set now has all 3840 pictures, with the two
recovery frames explicitly identified. These are original-renderer scene outputs,
not duplicated neighbouring pictures. No gameplay state was altered.

The full-window RGB scan found an unintended polygon-fog difference near2663.
The initial native fragment shader applied captured fog, whereas production
`engine/quad_gles2.c` explicitly bypasses polygon fog. Final baseline rendering
therefore keeps fog state but disables that effect. Earlier fog-on captures and
timings are superseded diagnostics, not final acceptance evidence. The shader
correction is a fidelity fix; it does not change protocol/mesh/material data.

Review interface: `native/generated/milestone3/review/index.html`, original left,
native right, all 3840 ticks. JPEGs are browser previews, not exact-pixel evidence;
lossless raw frames and nine PNG comparisons are preserved. Browser playback
speed is not a renderer benchmark or display cadence measurement.

## Final H700 result — BLOCKED; STOPPED at the performance gate

The final corrected shader and final executable completed all 3840 required ticks
on the actual Mali-G31/GLES3.2 v1.r20p0, with 8192 maximum texture size and16 vertex
texture units queried. Five startup warm-up pictures precede the lap. No actual
ticks or timing samples were discarded. No transfers, image readbacks, simulation,
audio, display swap or vsync ran during measurement. EmulationStation was stopped
only for the diagnostic and restored by the test runner. Stock OS/clock policy
was unchanged; this is not a locked-frequency test.

| Final single complete H700 process | Median | p95 | p99 | Worst |
|---|---:|---:|---:|---:|
| Complete picture completion |32.729ms|39.595ms|42.690ms|57.123ms|
| Completion-to-completion interval |33.136ms|39.826ms|43.062ms|57.184ms|
| Packet read |13.191ms|14.089ms|18.536ms|25.419ms|
| Dynamic world upload submission |0.769ms|1.121ms|1.329ms|2.775ms|
| World draw submission |3.527ms|7.227ms|10.096ms|14.380ms|
| World completion including read/upload/feeding |26.856ms|33.596ms|36.948ms|50.858ms|
| Subsequent HUD completion span |5.938ms|6.487ms|6.930ms|13.482ms|
| Thread CPU, complete picture |7.282ms|11.608ms|14.962ms|20.084ms|

The lap took123.214 seconds for 3840 pictures: **31.165 pictures/s offline capacity**,
with 20.924 pictures/s for inverse mean slowest1% completion intervals.
**3834/3840 completion spans exceed16.693ms.** These are offline throughput and
completion intervals, not displayed FPS or actual playable pacing. CPU submission
and world/HUD completion spans are not exclusive GPU timers. The existing
~52.9FPS production measurement is a live, different workload configuration;
it is not an apples-to-apples capacity comparison with this standalone player.

Tick1965 completes in27.847ms (packet read7.303ms, world22.366ms, HUD5.481ms),
with no static texture decode/upload/cache miss. The worst picture is3546:
2157 primitives,297 draws,25.047ms packet read,50.858ms world and57.123ms total.
This is a general command-I/O/rendering limit, not the incumbent texture burst
reappearing. Draw counts are median 146, p95 233, p99 260, maximum 300. All admitted
commands remain accounted for, including offscreen polygons that native clipping
rejects; no visibility omissions were used to improve the number.

The residency cache is preload-only:5085 RGBA contents/5403 recipes, eight pages,
480,456,704 page bytes plus2,015,232 immutable mesh bytes. All remain resident,
with zero in-lap static-world misses/evictions/uploads. The driver accepted the
resources. Dynamic world GPU storage 655360 bytes; fixed CPU frame buffers919577
bytes. Initial resource load25.438 seconds; first warm-up44.074ms, fifth18.469ms,
reported separately. The unchanged HUD reports 516,390,784 transfer bytes across
3845 pictures including warm-ups; these are separate from world uploads.
Peak process RSS 567188 KiB (553.9 MiB), including mapped driver
resources; do not add GPU allocations to this RSS as disjoint costs. Memory
before/after and all stage logs are archived with the numeric evidence.

Removing the measured file-read span arithmetically still leaves median 19.917ms
and p95 25.802ms. This is a derived diagnostic, not an independently measured
all-cached run; it shows why faster file transport alone cannot establish the
required headroom. **The architecture as measured fails the exit gate even
before simulation/audio.** A further repeat would not turn this into evidence
of credible headroom. No additional optimisation or repeated benchmark campaign
was started after the corrected failure was established.

Numeric acceptance/blocker records:
[H700 summary](benchmarks/native-race-20261015/h700-summary.json),
[complete raw final run](benchmarks/native-race-20261015/h700-final-run.txt),
[source audit](benchmarks/native-race-20261015/capture-audit.json),
[packet validation](benchmarks/native-race-20261015/packet-validation.json),
[material validation](benchmarks/native-race-20261015/material-validation.json),
[artifact identities](benchmarks/native-race-20261015/artifact-hashes.json).
The preliminary fog-on run is retained explicitly as superseded evidence.

## Fidelity coverage and limits

The final **host llvmpipe** reference/native sets each contain all 3840 lossless
640x480 composed pictures. Final RGB scan: median per-frame mean absolute RGB
error2.684/255; worst 7.679/255 at 2643. Final composed alpha is unchanged across
all 3840 pictures. Final composed-alpha agreement is not a separate certification
of every underlying world-priority pixel. The original CPU/GPU HUD verification
passes throughout reference capture; sampled native HUD inputs are byte-identical.
Full source priority/order fields and original local mesh/instance bindings pass.

Nine complete views and the highest-error mountain cases were visually inspected
for road/traffic positions, animated model selection, scenery, mirrors, camera,
HUD and atmosphere. Remaining intended differences are fresh full-resolution
source sampling versus capped/history-dependent incumbent bakes, float native
projection versus1/16-pixel source quantisation, and native near clipping. No
missing model/packet was found. Browser review JPEGs introduce compression and
are not exact-pixel data; lossless raw captures and sampled PNGs are retained.
The final H700 run validates every tick and GL execution but collected **no
Mali frame readbacks**, to keep timing clean. Real-device sequence visual signoff
therefore remains open; the visual exit checkbox is conservatively left unchecked.
The performance failure is decisive regardless of that remaining visual review.

## Historical focused M3 proposal — subsequently authorised and implemented above

Keep the source-capture contract, immutable source meshes, ordinary source-derived
RGBA pages, single-sample fragment shader and original HUD. Change only transport
and native command feeding in one bounded revised M3 experiment:

1. Deduplicate per-picture instances by captured display-list ID. Store each
   matrix/camera once, reference immutable mesh/material records, and encode only
   changing quad priority/brightness/visibility. Retain every original command ID
   and explicit cull reason; do not hide polygons. Move the actual constant CG
   font/palette/CZ data to immutable versioned resources and transport real text,
   mixer/LUT/direct changes. Measured revisions: CG1, palette 1, CZ1, attributes 1,
   text 3573 and mixer 166. Preserve future revision handling, not constant-state
   assumptions for arbitrary recordings.
2. Use a bounded prefetched command ring and compact/compressed sequential
   transport; charge decompression and waits to playback. As offline sizing only,
   zlib level1 reduces the current1,276,401,984 payload bytes to266,636,575 bytes,
   maximum compressed packet94800 bytes. No compressed runtime or H700 inflate
   measurement has been implemented; this is not a promised speedup.
3. Reduce native vertex work: six vertices for ordinary quads, a distinct bounded
   near-clipped path, and compact instance attributes instead of repeated full
   command-texture fetches for twelve vertices. Explicitly account for source-
   invisible primitives and protected boundaries. Reuse and profile the unchanged
   HUD, including its5.94ms completion span; do not defer that cost from the gate.
4. Revalidate all 3840 ticks, source IDs, palette/geometry events and full sequence.
   Keep the512 MiB preload bound and fixed buffer caps. Measure transport, world,
   HUD, tail intervals and loading separately. A prospective revised target is
   complete world+HUD median<=12ms and p95<=14ms, with p99/worst/misses exposed,
   as headroom evidence rather than live-game success. Stop at another failure.

This proposal is a focused continuation of **Milestone 3**, not permission to
start M4/live integration. It needs separate user authorisation.

## Final production and exit state

Executable, launcher and controller hashes are unchanged:
`6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e`,
`5358c57fe1a1d73b717c05a32e10b1683613db3771fc78b1ebad4ec6c22e3fad`,
`355aef433cc4a6d11381b1b218319e35978f5b23f1396a0ccfef0049a94e3284`.
All six original recording/sidecar files match the initial local copies; all five
backup checks pass. GPU HUD mode remains`default`, replay mode`off`.
EmulationStation restored (PID 28879); no production game left running. Shared
engine/gameplay/physics/audio/scheduler sources have no diff. No commits, pushes,
deployment or OS/clock changes occurred. HEAD remains`8fa1331`.

Diagnostic files only: device
`/userdata/system/rave-racer-native-m3-20261015/`, final run`final-run1/run.txt`;
local durable assets/review under
`/home/bwebb/dev/namco22-decompile/raverace/native/generated/milestone3/`.
No review images rely on host`/tmp`. GPU resources fit, but command transport and
native world/HUD completion do not meet the deadline. M3 remains **BLOCKED**;
M4 has not started. **STOPPED.**
