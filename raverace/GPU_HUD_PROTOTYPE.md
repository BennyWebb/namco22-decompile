# GPU HUD prototype — RG40XX H

Prototype and five-pair benchmark completed, 2026-10-14. No commits. Production
executable and launcher remain preserved pending the requested physical
controller/audio check. World geometry and texture-cache policy are unchanged.

## Original representation and necessary work

System 22 character graphics are dynamic packed 4-bit character RAM: 1024
characters, 16×16 pixels, 128 bytes each. The first 960 characters occupy
`cgram[0x1e000]`; the last 64 read the same `text[0x2000]` bytes used by the
64×64 tilemap. They must not be treated as immutable ROM graphics.

Each big-endian tile word contains a ten-bit character index, X/Y flip bits
10/11 and a four-bit palette group. A tile pixel selects `(group<<4)|nibble`.
Nibble 15 is transparent. Mixer register 7 selects the 256-colour HUD palette
within planar RGB palette RAM. Shadow-enabled pens FC–FE select three RGB
multipliers; mixing truncates `world_channel * multiplier / 256`. Scroll uses
10-bit wrapping with the existing X register bias. World framebuffer alpha
marks polygons drawn over characters. Fade and gamma use the existing byte LUT.

Simulation still consumes board events and copies immutable source state into
bounded presentation slots. These are necessary for deterministic state and
selected-picture consistency. Rendering originally compared whole character,
tilemap, attribute, palette and mixer inputs, expanded every screen pixel when
anything changed, copied two 1,228,800-byte images into each prepared slot, then
uploaded both on a generation change. The pixel expansion, full image copies
and bulk derived-image uploads are redundant representations of compact state.

## Chosen representation

The first prototype directly sampled packed character/tile bytes in one GLES2
fragment shader. It matched 2,002 fixed replay pictures through tick 6006 and
46 frozen/synthetic cases. However its warmed compositor span was ~9.18 ms
versus ~3.30 ms for the old compositor in the measured frozen menu case.
Additional dependent reads and packed-byte selection were too expensive to
choose without testing an alternative.

The selected candidate uses **one glyph-quad batch and an indexed HUD image**:

1. A 512×512 LUMINANCE glyph atlas stores decoded nibble values. Only characters
   used by the visible tilemap are checked. Each changed character requires a
   256-byte upload; unused characters are validated/uploaded when first used.
2. Visible tile words produce at most 41×31 quads, including partially scrolled
   boundary tiles. A mesh is rebuilt only when tilemap or scroll changes. The
   ordinary unscrolled mesh is 144,000 bytes, versus 2,457,600 bytes for the old
   two-image upload. Both flip flags and palette group travel in the mesh.
3. One draw writes palette index into R and coverage into A of a 640×480 RGBA
   texture. Transparent glyph pixels are discarded after clearing that target.
   Dithering is disabled while encoding indices and its prior state restored.
4. The final compositor samples world and indexed HUD, resolves current palette
   or shadow multiplier, preserves world priority, and applies the original LUT.
   Palette/mixer changes do not require glyph rebakes or mesh reconstruction.

This requires no GLES3 migration. The quad pass uses one sampler; final
composition uses four fixed samplers, with no material-sampler selection.
The warmed final-pass span for frozen racing tick 2502 was 2.774 ms versus
3.276 ms for the original compositor. This excludes a dirty glyph/mesh redraw
and is not an end-to-end FPS forecast. Timer spans can include CPU feeding gaps.

GPU logical storage for the normal optimized path is about 3.18 MiB: world
RGBA/depth, indexed HUD RGBA, glyph LUMINANCE atlas, palette and LUT. The old
compositor requests about 4.10 MiB. Driver allocation/padding is not directly
measured. CPU glyph/source/mesh caches add approximately 0.3 MiB; the three
full-screen slot image pairs (7.03 MiB) are absent, and the original global pixel
arrays need not be touched. Verification intentionally retains old images.

## Thread ownership and fallback

`RR_GPU_HUD` is OFF by default. In an enabled build, runtime opt-in requires
`ENG_GPU_HUD=1`; `ENG_GPU_HUD_GEOMETRY=1` selects the glyph-quad candidate.
Without the geometry selector, the preserved packed-tilemap diagnostic runs.
Unset/off retains the original CPU HUD in both serial and threaded operation.

The preparation worker computes only palette/mixer/scroll/LUT metadata and
borrows character/tile pointers from its immutable selected snapshot. The slot
stays owned through GLES consumption. There are no simulation pointers in the
threaded path, GPU calls on the preparer or new CPU pixel buffers. The serial
path consumes state immediately on its existing rendering thread.

Only the GLES owner updates glyph textures, mesh buffers and palette resources.
Dirty comparisons use the last **actually uploaded** data, not a previously
prepared picture. Discarded snapshots therefore cannot lose updates. A character
change invalidates the indexed image even if tile words did not change; palette,
shadow and LUT changes are applied independently in the final pass.

## Correctness protocol and harness issue

`ENG_GPU_HUD_VERIFY=1` renders the original CPU-expanded HUD and candidate over
one identical world FBO, reads both final RGBA images, and rejects any differing
channel. Fixed presentation intervals also compare board/CPU endpoint JSON,
PCM/voice checksums, geometry digests, texture-miss records and final captures.

The preserved initial tilemap validation is:
`/userdata/system/rave-racer-gpu-hud-20261014/validation-20261014-125402-7958`.
It contains 14 frozen state/world/reference/GPU cases. The standalone probe
uses production compositor code and the original `build_text` function extracted
verbatim, then exercises 32 additional cases with flips, all nibble values and
palette groups, scrolling/wrap, character/text aliasing, shadows, palette/LUT
changes and priority masks. All 46 cases matched RGBA exactly for both designs.

An apparent early glyph-path discrepancy was isolated to **a competing
EmulationStation framebuffer writer**, not changed HUD sampling. The captured
`early-recheck-20261014-134018-13404/hud-129.gpu.rgba` visibly contains the Ports
screen. Its intermediate pen image matches the captured tile/character bytes
exactly, while final image alpha violates the compositor's opaque output.
`S31emulationstation start` launches its wrapper asynchronously; chained tests
checking only `pidof emulationstation` could miss a starting child and leave it
running. These runs are invalid graphics comparisons.

The benchmark now stops the frontend unconditionally, waits for both wrapper
and child to disappear, and rejects a competing frontend during monitoring.
Tests keep it stopped between correctness and benchmark gates; it is restored
when the complete gated sequence exits. No arbitrary pixel adjustment or
renderer synchronization workaround was introduced for this harness issue.

## Reproduction tools

Build using the current ARM64 playable configuration plus
`-DRR_PARALLEL_FRAME_PREP=ON -DRR_GPU_HUD=ON`. All unrelated experiments remain
OFF. Existing source snapshots and world resource policy are retained.

Build the standalone probe:

```sh
python3 raverace/tools/build_gpu_hud_probe.py /tmp/rr-hud "$KNULLI_HOST"
```

Run on Mali with `SDL_VIDEODRIVER=mali`, `XDG_RUNTIME_DIR=/var/run` and the
frontend confirmed stopped. `ENG_GPU_HUD_GEOMETRY=1 hud_probe CASE_PREFIX ...`
loads captured `.state`, `.world.rgba` and `.reference.rgba` inputs and runs the
additional synthetic cases. At racing case `hud-2502` it measures warmed spans
using elapsed queries. These deliberately synchronous isolated query results
are excluded from the full-game benchmark.

Case state format is little-endian four u32 values (magic `0x48554431`, X scroll,
Y scroll, shadow enable), character bytes 122880, tile bytes 8192, palette RGBA
1024, nine multiplier bytes, LUT RGBA 1024. Image files are 640×480 RGBA in GL
bottom-row-first order. Verification captures and tools are diagnostic only;
production gameplay does not depend on recorded cases.

The durable five-pair runner is `tools/RaveRacer-GPU-HUD-Benchmark.sh`. It compares
against `/userdata/roms/ports/rave-gles2/rr_gles2`, enables original HUD/audio,
uses replay ticks 1800–5639, records stage/replay/monitor data, alternates order
and checks every endpoint against the preserved reference. It does not add a
Ports launcher or alter the installed executable.

## Results — five alternating A/B pairs

One consistent method: recorded first lap ticks 1800–5639, actual deployed
multicore baseline, original HUD, audio on, 64 MiB world texture cache, frame
VBO, identical `--thread-profile`, `--profile-video`, replay-timing, buffered
world texture-miss trace and 1 Hz monitoring. No batching, predecode, wake-on-
snapshot experiment, forced caches, CPU affinity or governor changes.

Results directory:
`/userdata/system/rave-racer-gpu-hud-20261014/ab-20261014-135149-14295`.
Host copy `/tmp/rr-hud/performance`; repository summary
`benchmarks/gpu-hud-20261014-summary.json`. Numbers below are medians of the
five per-run results; CPU stage entries are medians of per-run means.

| Metric | Deployed baseline | GPU glyph quads |
|---|---:|---:|
| Rendered FPS | 40.420 | 52.714 |
| 1% low FPS | 19.134 | 25.151 |
| p99 presentation interval | 41.035 ms | 33.027 ms |
| Median of worst intervals | 85.466 ms | 79.755 ms |
| Worst interval across all runs | 101.361 ms | 93.601 ms |
| Median / p95 image age | 50.585 / 67.119 ms | 37.945 / 55.108 ms |
| Simulation Hz | 59.903 | 59.904 |
| Preparation CPU/picture | 14.787 ms | 3.775 ms |
| HUD preparation CPU/picture | 11.070 ms | 0.048 ms |
| HUD upload/update CPU/picture | 5.538 ms | 1.003 ms |
| Final composition CPU/picture | 0.858 ms | 0.294 ms |
| GLES-owner CPU/picture | 23.265 ms | 17.013 ms |
| Peak RSS | 293.836 MiB | 288.047 MiB |
| Audio underruns, whole process | 0 in all five | 0 in all five |

FPS improves **30.4%**, 1% low improves **31.4%**, HUD preparation falls
**99.6%**, and HUD upload/update CPU falls **81.9%**. Every pair improves FPS;
candidate runs span 52.556–52.868 versus baseline 40.186–41.121. Every
endpoint JSON matches the deterministic reference. No competing frontend was
detected. Remaining large hitches generally coincide with world texture loads
near tick 1965. One baseline maximum at tick 3722 has only one texture miss;
it must not be described as a texture-loading hitch. Candidate pair 1's first
texture burst is slower than that paired baseline (93.60 vs 87.02 ms), despite
better aggregate pacing. World texture preparation was not redesigned.

Image age is publication-to-swap-return, not physical controller-to-photon
latency. The candidate still does not sustain 59.904 rendered FPS. Its mean
GLES-owner CPU cost alone remains slightly over the 16.693 ms full-rate budget;
GPU/presentation pacing and peak workload headroom remain relevant.

CPU governor remained `schedutil`; measured frequency ranges include occasional
dips (baseline 792–1512 MHz, candidate 1200–1512 MHz). Measured thermal ranges
were about 53.7–71.7°C baseline and 57.4–69.3°C candidate. These are ordinary
runtime comparisons including different heat generation, not fixed-clock cycle
counts. Baseline pair 3 has one 19.58 ms resynchronisation; all candidate runs
have none. All ten audio counters are zero, including startup.

### GPU transfer breakdown

Candidate first-lap payload per presented picture, median across runs:

| Payload | Bytes/picture |
|---|---:|
| Changed glyph texels | 6.4 |
| Tile quad VBO | 134,830.2 |
| HUD palette texture | 0.0 |
| Fade/gamma LUT | 164.0 |
| Total texture + VBO payload | 135,000.5 |

The typical mesh is 144,000 bytes when changed. A dirty glyph is 256 bytes;
a changed palette or LUT is 1,024 bytes. Small uniform updates and driver
protocol overhead are not included. Palette changes are validated synthetically;
the selected HUD palette does not change in this measured racing window.

The baseline sends 2,457,600 derived image bytes on each HUD generation change,
plus LUT updates. Its existing binary does not count first-lap HUD payload
separately, so no invented exact first-lap average is claimed. Comparable
**whole replay 1–5639** measured averages are ~1,597,912 image bytes/picture for
the baseline (LUT excluded) and ~98,783 total texture+VBO bytes/picture for the
candidate. Do not confuse that scope with the candidate's first-lap table.

### GPU effects and validation

The isolated tick-2502 warmed command span is 3.276 ms reference versus
2.775 ms glyph-quads (100 alternating samples each); it excludes indexed-image
redraw. An attempted dirty-update query reported disjoint status and was
rejected. No dirty GPU execution figure or active GPU utilisation is claimed.
The retained design is justified by complete-game improvement, not an additive
speedup calculation combining isolated queries and CPU scopes.

Guarded fixed validation through 6006 matches all 2,002 RGBA pictures, final
PPM, board/CPU JSON, texture-miss sequence and geometry digest. PCM is
`0c24e8fca4b6400e`; voices `b76f90445ba87e18`; board `f9154a83`.
The guarded run is `geometry-validation-20261014-134641-14211`.

Adaptive presentation with normal cache selection and live audio verifies all
2,078 selected pictures through tick 5639, including 1,322 racing pictures:
`live-verify-20261014-141046-19437`. Simulation is 59.904057 Hz and audio output
has zero underruns, overruns or resets. The CPU fallback's matching normal replay
confirms 8,032,495 PCM samples / 90,224 intervals, PCM
`a18439c518e08e3d`, voices `c12e04d613b280fc`, final sound state
`a588e1f884d1c5f8`. Both endpoints match the recorded deterministic reference.
The fallback/audio and isolated GPU evidence is
`final-checks-20261014-141928-20309`.
The original single-threaded CPU fallback also matches the deployed binary
through tick 600 in endpoint JSON and final pixels with `--presentation-thread
off`: `serial-fallback-20261014-143405-21665`.

Equivalent selected resources are pixel-identical. Independent free-running
runs can choose different world bake tiers under the existing sticky cache
policy because their presentation histories differ; no cache equality was
forced and no world-resource correction was hidden in the HUD change.

## Deployment decision

The glyph-quad prototype meets the substantial performance and automated
correctness gates. Keep it independently switchable and OFF by default in
source configuration. Physical controller gameplay, menus, audible sound and
START+SELECT exit still require the user's requested spot check before playable
integration. The candidate remains outside Ports:
`/userdata/system/rave-racer-gpu-hud-20261014/bin/rr_hud_geometry_capture`, SHA-256
`fef8f66dc7e87c18668a32b5c43f473d14c98b9323cb9236d47380700ce32b52`.

The deployed executable remains SHA-256
`77536f56ad17de9cf8772175c73442d9cb246d87aeb6fbf6c8d7f07c3d6ab53e`;
the existing pre-parallel-prep backup remains
`ed27bc04b41c402e2181a1f36c4b3876becffea6224a9499bd9a181120dbedb1`.
There is one Rave Racer Ports launcher. No production binary replacement,
launcher modification or commit has been made. EmulationStation is restored
after tests. Preserve all diagnostic inputs and rejected harness captures.

