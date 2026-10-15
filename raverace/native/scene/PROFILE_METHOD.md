# Focused Milestone 3 attribution method

`tools/build_profile_race.py OUTPUT_C` derives an isolated diagnostic source from
the unchanged v3 renderer. It adds thread CPU spans and two explicitly incomplete
world controls. No shared game or HUD source is edited. The native source snapshot,
binary and shader hashes must accompany results.

`tools/run_profile_h700.sh BASE NEW_RUN MODE SPLIT_FENCES` executes every original
tick 1800–5639 with five startup warmups, the same compressed transport, native
assets, visibility list, sorting/scissors, uploads and unchanged GPU HUD. It refuses
a running production game, checks the executable identity and restores ES on exit.
No readbacks, swaps, simulation, audio, clock changes or transfers run concurrently.

- Mode 0: complete rendering, normal v3 shaders and original draw calls.
- Mode 1: `GL_RASTERIZER_DISCARD` surrounds world draws; vertex inputs/state and
  issued draws remain, but world fragments/output are suppressed. Drivers may
  optimise unobservable vertex work; this control is not an exclusive vertex timer.
- Mode 2: world draw calls are suppressed; world resource updates, grouping/state
  calls, clear and the original HUD still execute. This isolates the combined
  draw/vertex/fragment dependency from remaining CPU state handling and HUD.

Split-fence runs finish world work before HUD submission and then finish HUD work.
Per-picture `world_submit_ms + world_sync_ms` and `hud_submit_ms + finish_ms`
attribute these serial spans. Split fences alter GPU scheduling/tile work, so their
sum does not predict normal one-fence throughput. Omitting world work can also
change the compositor's branch mix; HUD costs must be checked across controls.

`PROFILE` records thread CPU for the world group loop, world fence, HUD function,
and final fence. CPU includes application code and proprietary driver work; it
cannot identify exclusive driver time. Blocking/wall spans are not exclusive GPU
timers. Never subtract marginal medians to claim an exact stage saving. Compare
per-tick paired spans/distributions and retain all tails. Only mode 0 with one
final fence, no readbacks, and the full original picture is a headroom test.

`tools/analyse_race_profile.py LOG... --out REPORT_JSON` requires all3840 consecutive
frames and the original source/visible totals. `analyse_compact_race.py` remains
the completion/interval headroom analyser for unchanged full-render logs.

The cheap-fragment control (`render/race_profile.frag`) retains live UV and
brightness varyings so the compiler still needs source UV/material and shading
vertex work. It removes world image sampling, alpha discard and original fragment
shading, so it changes depth/overdraw semantics; it is a cost sensitivity test,
not a clean additive fragment timer or fidelity candidate.

`tools/build_vertex_control.py NEW_DIRECTORY` creates an additional diagnostic
that reads immutable source coordinates on the CPU and streams transformed
positions/UVs as attributes. Ordinary quads use a persistent six-index buffer
(four unique corner invocations); near clipping remains the same GPU algorithm.
There are no vertex texture gathers. The original fragment shader, complete
visible geometry, depth, priority and original HUD remain. This deliberately
repeats CPU transforms/static data transport to isolate GPU cost sensitivity;
it is **not the requested reusable indexed native backend** and cannot pass M3.
CPU preparation, larger uploads, driver attribute changes and memory must remain
visible in its logs. Floating arithmetic/affine clipping order can change edge
pixels; it is not a replacement fidelity reference. The actual shader/binary
hashes identify each variant independently of the common FRAME log schema.
