# Skipped-presentation geometry: opt-in A/B experiment

`--skip-geometry-on-dropped-frames on|off` defaults to **off**. It applies only
to independent timing. Coupled rendering, emulated clocks, interrupts, the
four-step catch-up/resynchronisation policy, controls, texture cache (64 MiB),
ordered GLES2 draws and contiguous VBO streaming are retained.

## Dependency audit

The existing profiling audit in [INDEPENDENT_PROFILE.md](INDEPENDENT_PROFILE.md)
identified that `rr_tick` called `rr_gl_prepare` before the SDL host chose a
presentation. Consequently every tick walked the list and built/sorted geometry,
even when the subsequent host call discarded the picture.

| Operation | Every simulated tick after this change? | Reason |
| --- | --- | --- |
| Translated 68K, DSP and sound execution, serial and board I/O | Yes | Hardware-visible execution and device/IRQ state |
| DSP PDP command execution and polygon/point RAM writes | Yes | In `rr_dsp.c` and translated programs, outside host list walking |
| PDP begin/refresh callbacks, direct-command collection | Yes | DSP callbacks update `rr_scene` state and its raw command queue |
| `rr_scene_frame(slave_active)` | Yes | Advances `cur_frame`, applies PDP visibility and clears refresh exactly once |
| Direct-command consumption | Yes | Drops this tick's raw commands, including on skipped pictures; never accumulate stale direct polygons |
| Palette conversion and generation bookkeeping | Yes | Conservative retention preserves cache invalidation across skipped palette transitions |
| Mixer/background latch, `g_eng_frame` cache clock | Yes | Current tick's background and unchanged texture aging |
| Decode raw direct polygons into quads | Presented ticks only when enabled | Host image construction, no board write/IRQ acknowledgement |
| Polygon list/point-ROM decoding, transforms, projection, near-plane clipping | Presented ticks only when enabled | Host reads of current polygon/point RAM; no emulated-device advancement |
| Sort geometry | Presented ticks only when enabled | Host ordered draw construction |
| Screen clipping, UV/fan expansion, texture lookup/decode/upload, VBO/draw/swap | Presented ticks only, as before | Already inside `eng_gles2_draw`, not on skipped ticks |

`eng_walk_list` initializes camera/list state locally on each walk. It reads
polygon RAM through `poly_word`; point-ROM/point-RAM reads are pure accessors.
`geo_hw_object` resets per-object lighting, and the list supplies current view
state before decoding. Its other retained counters are host diagnostics, not
emulated state. Skipping a walk intentionally stops incrementing diagnostic
geometry-rejection counters for discarded images.

## Minimal implementation

The opt-in path chooses presentation after the completed simulation tick and
before preparation. It uses the existing monotonic clock, input polling during
waits, forced captures and unchanged `rr_clock_render` algorithm. Baseline keeps
the original post-preparation decision point. Moving the decision necessarily
changes real-time presentation choices; it does not change the target period
`814 * 525 / 25600000 = 16.693359375 ms` (about 59.904 Hz).

`rr_gl_prepare_gles2(slave_active, geometry)` always converts the palette,
advances cache age, latches background and runs `rr_scene_frame`. It clears
`qn/qorder` every tick. If geometry is unwanted, it consumes direct commands and
returns before direct decoding, list walking and sort. Presented ticks execute
the original decoding/sort path against that tick's RAM and raw commands,
before consumption. No previous picture or delayed list is reused. CPU quad
capacity is retained for reuse; no new transient geometry buffer is allocated
for skipped ticks. No geometry, texture, scissor or blend-order rewrite is made.

Input is still polled each tick and during clock waits. Existing queued coin/
START transitions remain in the same input implementation. Simulation/DSP/sound
and input register sampling are not omitted when presentation is skipped.

`[GEOMETRY]` reports effective mode, tick/walk/direct-consumption counts and an
ordered digest of each tick's walk gate/direct-command count. `[GEOMETRY-RATE]`
reports resyncs and discarded lag specifically from `--profile-from`, avoiding
mixing warm-up resyncs with the city range. Existing `[STAGES]` rendered/skipped
cohorts and `[STAGES-RATE]` actual wall rates remain the main attribution.

Affected implementation files: `src/rr_main.c`, `src/rr_gl.c`, `src/rr_gles2.c`,
`include/rr_gl.h`, `include/rr_gles2.h`, and CLI help in `src/rr_headless.inc`.
The shared decoder, scene/device implementation, texture backend and GLES2
GPU submission code are unchanged.

## Correctness procedure

Real independent timing chooses different pictures depending on CPU/driver
speed; two final-frame captures alone can also have different intervening
texture-cache histories. `--test-present-every 3` is a validation-only override:
it runs independent pacing but presents exactly every third emulated tick plus
any requested capture. Both A/B paths make this decision before preparation,
giving identical presented ticks and cache histories. **Never benchmark with
this override.** It requires independent timing; ordinary game launches do not
use it.

The regression compares full headless JSON, byte-identical PPMs and ordered
video-tick digests for menu frame 600, transition frame 1400, city frame 1700,
and frame 1700 with a 4-MiB eviction-heavy cache. It checks zero traps, unmapped
accesses, ROM writes, DSP/sound faults, zero geometry on optimised skipped
cohorts, plus unchanged coupled-mode pixels/state with the flag on/off.

```sh
SDL_VIDEODRIVER=offscreen MESA_GLES_VERSION_OVERRIDE=2.0 \
  python3 raverace/tools/test_gles2_skipped_geometry.py \
  /tmp/rr-gles2-native/rr_gles2 /tmp/rr-headless-native/rr_headless \
  raverace/extracted /tmp/rr-skip-validation
```

Existing headless, texture, timing, clock and virtual-controller regressions
must also pass. These tests validate selected scenes, not every possible game
state or pixel across differently timed live runs.

## Build and deploy

Use the existing native/Buildroot configurations from [GLES2.md](GLES2.md):

```sh
cmake -S raverace -B /tmp/rr-gles2-native -DRR_GLES2=ON \
  -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/rr-gles2-native --target rr_gles2 -j4
cmake -S raverace -B /tmp/rr-gles2-arm64 -DRR_GLES2=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/raverace/cmake/knulli-aarch64.cmake" \
  -DKNULLI_HOST="$HOME/dev/knulli-scarab/output/h700/host"
cmake --build /tmp/rr-gles2-arm64 --target rr_gles2 -j4
file /tmp/rr-gles2-arm64/rr_gles2
readelf -h -l -d /tmp/rr-gles2-arm64/rr_gles2

KNULLI_IP=192.168.1.69
scp /tmp/rr-gles2-arm64/rr_gles2 \
  "root@$KNULLI_IP:/userdata/roms/ports/rave-gles2/rr_gles2-skip-geometry"
scp raverace/tools/RaveRacer-GLES2-Skipped-Geometry-Benchmark.sh \
  "root@$KNULLI_IP:/userdata/roms/ports/"
ssh "root@$KNULLI_IP" \
  'chmod +x /userdata/roms/ports/rave-gles2/rr_gles2-skip-geometry /userdata/roms/ports/RaveRacer-GLES2-Skipped-Geometry-Benchmark.sh'
```

The separate filename preserves the working interactive executable. Reuse its
existing `extracted` directory. If necessary use `ssh/scp -F /dev/null` to bypass
broken host SSH configuration. No re-extraction or package installation is needed.

## Repeatable handheld A/B benchmark

With no other game running:

```sh
ssh "root@$KNULLI_IP" 'cd /userdata/roms/ports
/etc/init.d/S31emulationstation stop
trap "/etc/init.d/S31emulationstation start >/tmp/rr-skip-es-restart.log 2>&1" EXIT
./RaveRacer-GLES2-Skipped-Geometry-Benchmark.sh'
```

This runs three city A/B pairs (1700 total ticks, measured 1400–1700) and three
longer scripted-race pairs (3600 total ticks, measured 1400–3600). Trial two
reverses on/off order. All runs are paced independent timing, textured/frame
VBO/64 MiB, same coin/gas/steering script, fresh process, no capture or verbose
input logging. The race comparison is repeatable scripted progression, **not a
physical interactive play test**. Interactive testing was deferred at the
user's request. Existing controller functionality is retained and regression tested.

Each new persistent `rave-gles2/skipped-geometry-<date>-<pid>/` directory contains
logs, complete final state, exit codes, manifest/binary SHA and state hashes.
`--profile-memory` records RSS high-water and system memory at exit; it is not
continuous system-memory sampling or physical GPU memory accounting.

```sh
scp -r "root@$KNULLI_IP:/userdata/roms/ports/rave-gles2/skipped-geometry-*" /tmp/
python3 raverace/tools/summarise_gles2_skipped_geometry.py /tmp/skipped-geometry-RESULT_DIRECTORY
```

The host summariser validates every run's full state, frame range, sample
counts and zero skipped geometry, emits detailed `comparison.tsv`, and prints
median rates/cohort preparation/lag/memory for each workload and mode.

Manual longer physical play, when an operator is available:

```sh
cd /userdata/roms/ports/rave-gles2
SDL_VIDEODRIVER=mali ./rr_gles2-skip-geometry extracted --interactive \
  --timing independent --skip-geometry-on-dropped-frames on \
  --vbo-upload frame --texture-budget-mib 64 --profile-stages --profile-memory \
  > interactive-on.json 2> interactive-on.log
```

Repeat with `off` and separate filenames. START+SELECT exits. Release/restore
EmulationStation as above. These physical-input runs are responsiveness checks;
different progression is not a strictly matched benchmark.

## Measurements and recommendation

Native desktop and matching Buildroot ARM64 builds passed. The ARM64 ELF uses
`/lib/ld-linux-aarch64.so.1`, Linux 4.9 and SDL2/GLESv2/EGL/libmali, without
`libGL`. `-fno-strict-aliasing` and `-fwrapv` remain enabled.

Headless, texture/four diagnostic mode, timing, fixed-clock and virtual-input
regressions passed. The new deterministic-presentation A/B test passed all four
scene/cache cases and the coupled control. The video digest matches on/off:
1700 ticks, 1604 eligible list walks, 9094 consumed direct commands,
`9d3d73f76d1ba139`. Native frame-1700 full state SHA is
`b2a926aa3c0990c3ad27a512f7eda27abc9f22438b743487416e869a46821f1a`;
3600-frame headless reference SHA is
`ed0f4680d04ac73f7e642a2dc625856df545bc81bff0c26dfb932772a4e59ca3`.
Validation captures/logs are retained in `/tmp/rr-skip-validation/`.

Physical RG40XX H frame-1700 A/B captures also matched byte for byte, with
PPM SHA `869d718ce31dc2aa86c858a24d3f2f67e438d9396ce7b9dd7156e84c17956eb4`.
Both hardware states matched native/headless, and the video-tick counts/digest
matched exactly. Files are in `/tmp/rr-skip-h700-captures/` and persistent
`/userdata/roms/ports/rave-gles2/skip-check-{off,on}.{ppm,json,log}`.
This is a deterministic-presented-tick correctness test, not a speed benchmark.

## Physical RG40XX H performance results

All twelve paced independent-mode runs completed on the physical RG40XX H,
three A/B pairs per workload, with reversed order in trial two. The same
experimental executable was used for both options. It has SHA-256
`8cf64860e4dbe7ec7faf383db5b9c8fea1ebb3fab93a58965f8a9cb5d3d62736`.
These are **scripted independent-timing wall-clock rates**, not coupled/unpaced
throughput or operator-controlled play. No capture or validation-only schedule
was enabled during timing. Each cell below is the median of three runs.

### Measured ranges

| Range / mode | Steps | Rendered / skipped | Simulation Hz | Render FPS | Wall seconds | Resyncs | Discarded lag ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| City 1400–1700, off | 301 | 92 / 209 | 46.12 | 14.01 | 6.526 | 52 | 1491.100 |
| City 1400–1700, on | 301 | 82 / 219 | 57.14 | 15.58 | 5.268 | 7 | 242.572 |
| Race 1400–3600, off | 2201 | 1037 / 1164 | 53.38 | 25.15 | 41.233 | 175 | 4493.264 |
| Race 1400–3600, on | 2201 | 1170 / 1031 | 58.70 | 31.20 | 37.496 | 25 | 756.431 |

City simulation ranges were 45.83–46.30 Hz off versus 56.89–57.18 Hz on;
long-race ranges were 53.30–53.46 versus 58.68–58.73. Render FPS ranges were
14.00–14.10 versus 15.57–15.69 (city), and 24.24–25.36 versus 30.95–31.40 (race).
The improvement is consistent across execution orders, well beyond this spread.

Whole-run medians, including boot/menu/warm-up:

| Run / mode | Total steps | Rendered / skipped | Simulation Hz | Render FPS | Resyncs | Discarded lag ms |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 1700 ticks, off | 1700 | 1122 / 578 | 50.44 | 33.26 | 143 | 5298.095 |
| 1700 ticks, on | 1700 | 1126 / 574 | 57.01 | 37.76 | 54 | 1423.031 |
| 3600 ticks, off | 3600 | 2067 / 1533 | 52.64 | 30.22 | 265 | 8281.522 |
| 3600 ticks, on | 3600 | 2214 / 1386 | 58.04 | 35.70 | 70 | 1914.698 |

These warm-up-inclusive rates must not be substituted for the measured city
rates. The longer range includes lighter scenes; its near-target average does
not establish full speed throughout demanding city geometry.

### CPU cohorts and rendering

Milliseconds per tick of the indicated cohort:

| Range / mode | Rendered simulation | Skipped simulation | Rendered preparation | Skipped preparation |
| --- | ---: | ---: | ---: | ---: |
| City off | 12.516 | 12.538 | 4.948 | 4.927 |
| City on | 11.797 | 12.310 | 4.920 | 0.227 |
| Race off | 12.440 | 12.604 | 2.861 | 3.644 |
| Race on | 12.215 | 12.518 | 2.807 | 0.228 |

The city skipped cohort's list decode (2.978 ms) and sort (1.706 ms) become
exactly zero, as does direct decode. No quads are constructed. Approximately
**4.70 ms / skipped city tick (95%)** and **3.42 ms / skipped race tick (94%)**
are eliminated. The remaining ~0.23 ms is mostly palette conversion retained
for correctness (~0.22 ms), plus the frame latch/consume/bookkeeping.

Selected milliseconds per *presented* tick:

| Range / mode | Texture lookup/decode | Texture upload | Vertex upload | Draw calls | Submission other | Swap | Avg draws | Avg triangles |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| City off | 2.788 | 1.001 | 0.196 | 5.329 | 1.504 | 0.471 | 800.0 | 1919.5 |
| City on | 3.051 | 1.121 | 0.195 | 5.288 | 1.497 | 0.479 | 795.4 | 1909.1 |
| Race off | 1.314 | 0.350 | 0.080 | 2.241 | 0.634 | 0.346 | 312.4 | 704.7 |
| Race on | 1.198 | 0.316 | 0.076 | 2.130 | 0.598 | 0.347 | 294.5 | 661.0 |

No additional geometry algorithm or texture work was added to presented frames.
Preparation is essentially unchanged in the city. Real-time A/B cohorts
naturally contain different presented ticks; per-presentation means are not
matched polygon microbenchmarks, and small simulation/scene differences must
not be attributed to an additional core/renderer optimisation. The complete
TSV retains core/device, clipping/build, cache and submission subdivisions.
Texture and GPU work remain zero for every skipped cohort.

Median range texture uploads/misses were 256 off versus 234 on (city), and
1115 versus 1124 (race). Race on presents more frames; these are different
presentation/cache histories, not a decoder/cache-policy improvement. Cache
capacity/policy remain unchanged. Every logged texture storage peak stayed
within the 64-MiB application-owned payload budget.

Median process RSS high-water was 195.9 versus 197.9 MiB (city) and 228.4 versus
229.4 MiB (race); the maximum across all runs was 229.7 MiB rounded up. Minimum
observed **at-exit** MemAvailable was 643.7 MiB. No allocation failures occurred.
These figures include driver/cache lifetime differences and do not imply a new
geometry allocation or continuous memory-pressure measurement.

### Correctness and retained results

All twelve exit codes were zero, with no traps, unmapped accesses, ROM writes,
DSP or sound faults. All 1700-frame states matched the native reference; all
3600-frame states matched their native headless reference. Ordered video-tick
counts/digests also matched across every actual timed run, despite different
presentation choices. At 3600 ticks the counters were 3504 eligible walks and
26194 consumed direct commands, digest `ef5caaff82673995`.

Persistent results:
`/userdata/roms/ports/rave-gles2/skipped-geometry-20261012-003346-614/`.
Retrieved logs and generated `comparison.tsv`:
`/tmp/rr-skip-h700-results/skipped-geometry-20261012-003346-614/`.
The original `rr_gles2` executable was preserved and EmulationStation was
confirmed running after the benchmark's restoration trap completed.

## Recommendation and next opportunity

The measurements support enabling this optimisation: city simulation rate
improves **23.9%**, rendering **11.2%**; the longer race improves **10.0%** and
**24.1%**, respectively. Range discarded lag falls about 83% and resyncs about
86%. Simulation and rendering gains are distinct: city on presents fewer images
per 301 emulated ticks, yet more images per wall-clock second because those
ticks execute faster. The long race recovers enough headroom for more images
per simulated interval as well.

The emulator is closer to 59.904 Hz, but **does not sustain full arcade speed**
on this workload: measured city rate is 57.14 Hz, and even the longer average
is 58.70 Hz. Clock frequency/catch-up policy have not been adjusted to hide lag.

**Recommend making it the default after review and a brief physical interactive
sanity check.** The implementation remains **off by default**, as requested;
physical interactive testing was deferred by the user in favor of scripted
long-race tests. Use `on` explicitly for the next playable handheld run.

Core/device execution is now the dominant fixed floor: weighted simulation
cost is ~12.16 ms/tick in the optimised city and ~12.35 ms/tick over the race.
At 59.904 Hz that alone consumes roughly 73–74% of this serial thread's time.
The 68K/scheduler residual accounts for roughly 6.3–6.7 ms/tick, with DSP,
sound and C352 sample advancement accounting for most of the remainder.
The highest-value next investigation is **targeted profiling of translated
68K/scheduler hotspots**, followed by a small correctness-tested change only
where a specific hotspot warrants it. Keep every emulated cycle/device effect;
muted audio is not justification for skipping sound execution/sample generation.

Presented city geometry still costs ~4.92 ms and driver draw submission ~7 ms
per presentation; these are the next host-only candidates if core profiling
finds no safe gain. Existing timers cannot distinguish asynchronous GPU time
from every driver stall. Swap is only ~0.48 ms and palette work ~0.22 ms/tick,
so neither is the highest-value next target here. No additional optimisation,
cache change, batching change or commit was performed.
