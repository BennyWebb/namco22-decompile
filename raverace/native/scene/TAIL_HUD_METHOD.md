# Focused M3 slow-picture, bandwidth and ordering investigation

Authorised 2026-10-15; M2 remains complete, M3 only. Production is protected.
Existing certified opaque rendering, blank-glyph omission, original LUT and
shadows are retained. No previous LUT alternative is revisited.

`analyse_slow_frames.py` accounts for the previous candidate's 69 deadline misses
and associates every tick with scene commands, draws, material/page transitions,
near/protected groups and HUD activity. Summed clipped screen bounding boxes are
explicitly a density proxy, not hardware overdraw. Timing correlations alone do
not establish cause. Component fences separately expose GPU completion costs.

`build_tail_hud_probe.py OUT --ticks LIST` derives a private linear-order probe.
Its default path matches the previous optimized renderer. `ENG_NATIVE_PEN_RG=1`
uses an RG8 pen target instead of RGBA8: red retains original index, green retains
original coverage. Final shader reads green coverage; world priority, palette,
shadow arithmetic and all dynamic LUT entries are identical. FBO completeness
and GL errors are checked. GPU logical saving is 614400 bytes. Green is written
in both paths and ignored by the RGBA baseline compositor.

`ENG_NATIVE_DISCARD_DEPTH=1` invalidates only the world FBO depth attachment
immediately after its last draw, before diagnostic fences or framebuffer switches.
No later operation reads depth; next picture clears it completely. Color/priority
are retained. This tests avoidable tile store bandwidth rather than simplifying
geometry. Default flags are off. Neither experiment changes source packets.

Predeclared experiment order: three full-race component runs (current front1,
RG8 only, dead-depth discard only); keep a change only with >=0.2 ms paired median
completion benefit and no meaningful p95 regression, plus exact visual checks.
Otherwise retain the current original HUD. Component runs do not count as
acceptance; unfenced submit spans are not isolated GPU costs.

Mali ordering comparisons use the same executable and original HUD, front0/front1,
at 21 predetermined ticks: previous hardest completions, maximum draws/near/
protected/density/HUD activity and existing representative scenes/mirrors.
Capture runs are excluded from timing evidence. Compare world/composed RGBA and
priority alpha, plus independent CPU HUD oracle. Any experimental bandwidth
change selected also requires Mali output comparison before final timing.

Repeatability is a fixed, nonadaptive six-process series: B,C,B,C,B,C, five
warmups then all3840 ticks each, one final fence, RAM telemetry, original immutable
pack/shaders, stock device configuration. B is the current optimized guarded linear front-to-back renderer; C uses the
same ordering plus only proven bandwidth improvements. Original ordering is
separately validated in the Mali visual pair. Evaluate the fixed
series as a single repeatability investigation; retain every run/sample, even if
an individual run misses. Stop at its exit gate; no subsequent tuning/retries.
Report per-process completion/interval distributions and all deadline misses,
never hide failures behind pooled statistics. Required gates remain12/14 ms.

All measurements include FIFO acquisition, preparation, uploads and GPU completion.
End-to-next-start gaps and post-lap log drain are reported separately. The16 MiB
trace explicitly fails on overflow and excludes no frames. No simulator/audio
is linked and production executable, launcher, controls, saves/replays stay intact.

Component decision before repeatability: RG8 paired median completion improvement
0.1671465 ms fails the0.2 ms threshold (local pen/final benefits only0.04156/
0.02870 ms); disabled. Dead-depth discard improves paired median world0.219080 ms
and complete0.274006 ms, diagnostic p95 improves18.715 to18.106 ms. Retain as a
provisional candidate requiring Mali parity and the fixed six-process final test.
No claim that component results alone establish one-fence or repeatable benefit.

The depth-store result motivates one additional bounded final-pass bandwidth
control: `ENG_NATIVE_DISCARD_COLOR=1` invalidates default framebuffer color
immediately before the opaque fullscreen compositor overwrites every pixel.
The shader has no discard, blending is disabled, and all channels are written.
No previous output is read. Test an explicit original/color pair with the same
new executable; require >=0.2 ms paired final-pass benefit before selection.
This does not change LUT resources or the shader's appearance. Preserve the
first binary and both generated source snapshots; final tests use one binary.

Final selection before the fixed repeatability series: FRONT1/FOLD0/LOG ram,
PEN_RG0/DISCARD_DEPTH1/DISCARD_COLOR0. Final-color discard gives only0.073872 ms
paired final-pass benefit; rejected. Baseline is FRONT1/FOLD0/LOG ram with all
new flags0. Both use `probe-color` (SHA256
cee2c1fd92a0c035b1280deee8a994ddd606d3b04bb38a63044b363171257a0f).
The pen output repeats in259 of3839 transitions; no caching implementation is
selected without a measured benefit. No further experiment is planned after
the fixed six-process sequence and its gate assessment.

Device evidence base: `/userdata/system/rave-racer-native-m3-tail-hud-20261015`.
`probe` is the first RG8/depth diagnostic and ordering visual binary;
`probe-color` includes the later optional output-color invalidate control and is
used identically by both final benchmark paths. Generated private sources remain
under `native/generated/milestone3/tail-hud-probe` and `tail-hud-color-probe`.
Static resources are read from the unchanged earlier M3 directories.

Build with `python3 raverace/native/tools/build_tail_hud_probe.py OUT --ticks LIST`,
then compile `OUT/probe.c` with `-O2 -Wall -Wextra -DRR_GPU_HUD`, repository
`raverace/include` and `engine` include directories, SDL2/GLES libraries, libm,
zlib and pthreads. ARM compiler is the existing Knulli H700 SDK's
`aarch64-buildroot-linux-gnu-gcc`; link `-lSDL2 -lmali -lm -lz -pthread`.
Host tests use `SDL_VIDEODRIVER=offscreen` and the certified opaque shader directory.

Final series command on H700:
`sh BASE/run_tail_repeatability_h700.sh BASE 0`.
The runner refuses existing output directories. The recorded series is the
predeclared experiment, not an invitation to rerun favourable samples after a
failed gate. `analyse_tail_repeatability.py DEVICE_OUTPUT EVIDENCE/repeatability.json`
checks all flags, source/visible totals and per-tick draw/geometry identity, and
reports per-process completion/interval gates and paired reductions.

Final outcome: all three candidate p95 values (16.139230,15.506663,15.488718 ms)
fail14 ms. One-fence paired median gains0.091671/0.114132/0.143260 ms fail the
0.2 ms selection threshold; all three candidate deadline-miss counts increase.
Reject depth discard along with RG8 and output-color discard. The selected
experimental path stays FRONT1/FOLD0/LOG ram with PEN_RG0/DISCARD_DEPTH0/
DISCARD_COLOR0. All new options default0 and remain diagnostic only. STOPPED.
