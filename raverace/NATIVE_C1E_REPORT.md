# C1e — Compact index atlas: H700 go/no-go

| Gate / result | Measurement |
|---|---|
| **Overall: NO-GO; STOPPED** | Fenced world+HUD p95 exceeds14ms at both240 and420MHz |
| Stock index play, two runs | **PASS:** repeats0/0; settled underrun frames0/0; main gaps>50ms0/0 |
| Stock fenced completion | Index median16.046 / p9516.271 / p9916.585ms; **FAIL** |
| 420MHz diagnostic | Median16.053 / p9516.263 / p9916.585ms; **FAIL**, no conditional GO |
| Mali fidelity | **PASS:** classes(a/b/c)=0/819/0 across36 images |
| Validation | All21 authoritative sets and4207 adjusted receipts exact in each full capture replay |
| Atlas memory | **PASS:**459.34→58.76MiB measured atlas allocation, **7.817×** reduction |
| Production / restoration |47 protected files unchanged; stock clocks/RT defaults/EmulationStation restored; no game running |

## Scope and implementation

Read HANDOVER/C1b/C1c/C1d; branch `feature/knulli-h700`, unchanged HEAD
`eb9a4ac5910dab65217573c660b4c42e27bc8c4a`. Private derivation under `native/c1e/`.
Same binary selects `ENG_C1_TEXTURES=rgba|index`; index uses compact R8UI pages,
normalised nearest `texture()` sampling and the C1d lowp palette qualifier.
Slot-owned palettes, three rotating upload resources, changed-group uploads,
opaque/cutout shader variants and existing solid path are retained. Reference
materials are opaque; C1e does not establish cutout or broader game coverage.
No simulation, physics, audio logic, production/shared-engine, fallback or launcher changes.

Debug provenance uses separable X/Y coordinate textures rather than large2D grids
for safe RGBA peak memory. Host proof:108/108 readbacks byte-identical to full-grid
provenance. Four bounded capture slots total28.125MiB; low-priority writer, no
capture-file writes on rr-present. Both changes are diagnostic-only.

## Fidelity and determinism

Four initial full host replays and two axis-control replays pass6006ticks/all21sets;
normal images retain C1d/C1c results. C1b source-texel proof remains5407materials /
120048149texels exact. On Mali, full RGBA/index colour and both provenance replays
pass21sets (63 domain records) and4207 adjusted canonical receipts each. Per-mode
raw packets equal the corresponding host packets; RGBA retains original packets.
Index page/UV metadata and compatible batching differ: canonical receipts compare
pre-painter commands with the page field zeroed, as documented in C1b; raw index
packets are not claimed identical to RGBA. Play builds perform no packet hashing.
All completed play/fence replays also pass all21 authoritative sets.

Fresh RGBA36images exactly match P3 Mali hashes. Index residuals:819pixels,
**all class(b)**,409 horizontal/410 vertical adjacent texels in the same original
material/bank/UV rectangle; class(a)=0,class(c)=0. No material/bank mismatch anywhere
in18 world frames. Composed residuals coincide with world residuals. All36MRT colour
proofs and11059200 sampled ROM/palette colours are exact. No tolerance/exemption.
All36 side-by-sides/overlays and six contact sheets reviewed: no missing geometry,
wrong palettes, HUD corruption or displaced features. User observation: “Looks good
to me” in response to the colour replay audio/motion question; exact run unspecified.
Review PNGs: `native/generated/c1e/review/`; machine evidence: `mali/` under evidence.

## Matched play timings (ms)

Full6006ticks; native1800–6006, dense3550–3590. Order index1→RGBA→index2.
CPU1512MHz, GPU240MHz, queue2, default priorities/affinity off, valve off; asynchronous
logging, pre-warm, gradual release, O2/P3B2 unchanged. Every switch-dependent timing
has a preceding1900-tick smoke receipt. Nearest-rank percentiles, no outlier removal.
Legacy release completes2112; settled counts are counter(6006)−counter(2112).

| Play run | Native prep med/p95/p99 | Dense prep med/p99 | Compiler native/dense median | Repeats / settled UR / main gaps |
|---|---|---|---|---|
| Index1 |8.937/11.584/13.079|13.079/14.197|6.023/8.914|0/0/0|
| RGBA |8.667/11.175/12.779|12.688/13.417|5.757/8.400|0/0/0|
| Index2 |9.007/11.561/13.114|13.059/13.773|6.076/8.925|0/0/0|

Full-native underrun frames828/851/897 respectively, all before settled window.
Index preparation does **not** improve: per-quad recipe binary search replaces
RGBA's usually successful P2 revision-cache hit. Native compiler median increases
~0.27–0.32ms; dense preparation ~0.37–0.39ms. No compiler optimisation undertaken.

| Run | GL CPU median | Draw wall median/p95/p99 | Swap wait median/p95/p99 | Swap interval median/p95/p99/worst | Snapshot→swap med/p95/p99/worst |
|---|---|---|---|---|---|
| Index1 |4.271|3.862/5.851/6.754|12.342/13.321/13.559|16.746/16.839/17.111/18.999|33.347/33.463/33.726/43.096|
| RGBA |4.632|4.203/6.321/7.103|12.004/13.226/13.492|16.747/16.842/17.074/18.892|33.358/33.468/33.675/44.609|
| Index2 |4.285|3.851/5.711/6.496|12.318/13.297/13.556|16.747/16.844/17.129/18.742|33.354/33.466/33.720/45.015|

All stage med/p95/p99/worst, swap intervals and per-tick samples retained in summaries.
Snapshot→swap is a return-time proxy, not measured physical scanout age.

## Separate one-fence diagnostics and remaining bottleneck

| Path / GPU | Completion median/p95/p99/worst | World submit median | HUD submit median | Final wait median/p95 |
|---|---|---|---|---|
| RGBA240 |16.051/16.292/16.549/35.857|1.534|0.960|13.122/13.921|
| Index240 |16.046/16.271/16.585/36.207|1.370|0.925|13.239/14.047|
| Index420 |16.053/16.263/16.585/22.613|1.380|0.921|13.233/14.007|

Each full run has4207 complete one-fence records and zero log drops. Diagnostics
perturb pacing: inferred repeats27/53/78, settled underrun frames8/7/601; these are
not headline play results. Completion includes GPU and driver/surface dependency:
~13ms wait cannot be called pure fragment time. Raising GPU240→420 changes p95 by
only0.008ms. Fencing largely moves waiting from swap to draw completion; actual
GPU execution versus presentation dependency remains unresolved. No subtraction
of presumed vsync time or relaxation of14ms gate. Index versus palette fetch costs
cannot be independently identified here; measured combined two-read path is close
to RGBA. Palette upload/check wall median0.033ms,p950.045,p990.096,worst4.229(index1);
384rows/393216bytes uploaded in total (initial128groups×3copies), no later changes.

## Memory, cache loading and clocks

Fresh-context atlas probe, baseline80Mali pages for both: RGBA delta117591pages,
index15043pages (4KiB/page):459.34→58.76MiB,7.817×. Three palette textures add0.375MiB
API storage. API atlas payload458.349→58.6875MiB; whole-ROM112MiB is host sizing only.

| Run | RSS high-water MiB | Minimum MemAvailable / CmaFree MiB | Settled total Mali median / native peak MiB |
|---|---|---|---|
| Index1 |365.76|500.40/1.57|95.00/195.82|
| RGBA |756.51|109.29/2.25|539.10/595.77|
| Index2 |365.56|499.47/20.05|95.90/196.60|
| Fence RGBA240 |757.99|109.07/2.24|537.87/595.62|
| Fence index240 |366.67|499.99/25.26|93.63/195.66|
| Fence index420 |364.82|500.16/43.79|91.44/194.30|

Total settled GPU reduction5.62–5.67×; legacy-overlap peak only~3.0×. Atlas gate
passes; do not claim3.5× reduction for the whole transition peak. RSS/GPU may overlap.
Every run, including capture/smoke/failed attempts, retains10Hz memory/clock samples.
Index timing cache reads2727.784/2724.596ms, upload submission153.168/140.617ms,
GL-ready3314.374/3293.822ms; warm420read111.705ms/GL-ready539.805ms. RGBA read20724.171ms,
GL-ready23542.134ms. Cache warmth uncontrolled; these are startup receipts, not disk benchmarks.
All six timed/fence runs start46.961–52.064°C; CPU1512 throughout, GPU240 or420 as
labelled, both cooling states0; peak native temperature62.594°C. No clock/thermal flags.

## Evidence, limitations and stop

[Evidence/methods](benchmarks/native-c1e-20261016/README.md) retain all samples and failed
setup attempts. An oversized verbose fence log invalidated one GPU distribution;
replacement compact-log diagnostics preserve GL/compiler/fence placement and retain
all4207records. A cancelled partner run, invalid combined-context memory probe and
unsupported-governor smoke abort remain recorded, excluded for explicit setup reasons.
420 uses simple_ondemand min=max420MHz (only available governor), restored by EXIT trap.
Final verification:47/47 protected files exact; production SHA256
`6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e` unchanged;
all4CPU schedutil480–1512MHz, GPU simple_ondemand bounds0/0, RT defaults restored;
EmulationStation running, no diagnostic/game/sampler left. Device session~68.6minutes.
Recommend a separately authorised bounded GPU-execution/driver-synchronisation
attribution before choosing further texture changes. Compact atlas fidelity/memory
are promising, but C1e remains NO-GO and M3/P3 gates are not retrospectively passed.
No soak, production replacement, commit or push. **STOPPED; further work needs approval.**
