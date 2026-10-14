# Rave Racer — Native H700 Renderer Roadmap

**Status:** Milestone 1 COMPLETE (2026-10-14); Milestone 2 not started or authorised. See [progress and acceptance evidence](NATIVE_RENDERER_PROGRESS.md).  
**Target:** Anbernic RG40XX H / Allwinner H700 / Cortex-A53 ARM64 / Mali-G31 / Knulli.  
**Approach:** A **separate native renderer and offline asset pipeline**, initially driven by the original Rave Racer simulation.  
**Primary goal:** Original game handling and behaviour with visually faithful graphics at a consistently paced arcade-rate **59.9040599 Hz** (or an explicitly validated display-compatible presentation policy).  
**Implementation policy:** Work on **one milestone at a time**. **Stop at its completion gate. Never start the next milestone without explicit user authorisation.**

## 1. Project contract

### Non-negotiable requirements

1. **Preserve the game, not the old graphics implementation.** The translated 68020 game and DSP logic remain authoritative for vehicle physics, steering response, collisions, traffic, AI, race state, camera/game timing and audio events. Rendering changes must not influence simulation state or timing. Do not rewrite physics as part of this roadmap.
2. **Visual fidelity, not pixel identity.** Preserve the recognisable original models, track layouts, proportions, colours, scenery, animations, HUD and overall arcade aesthetic. Intentional differences in rasterisation, depth, sampling and antialiasing are permitted where visual review finds them acceptable. Any altered gameplay or missing scene content is not.
3. **Consistent pacing matters more than peak FPS.** The eventual acceptance target is one new game picture for every authoritative simulation tick, without routine discarded pictures, with refresh-synchronised display and no audio underruns/resyncs. A 60 FPS average or a fast frozen frame is **not** proof of success.
4. **Retain and protect the existing playable port.** The current GLES2 production executable, launcher, controller settings, saves/replays and recovery backups remain untouched unless the user separately authorises a deployment. Keep experimental code and binaries isolated.
5. **A genuinely native graphics path.** Aim to convert ROM assets into reusable native meshes, materials, indices and GPU resources *offline or at load time*, rather than repeatedly reconstructing each original System 22 polygon and texture allocation during gameplay. A hybrid path for dynamic geometry is acceptable.
6. **Modern features are optional and measured.** Design for selectable antialiasing, mipmapping, anisotropic filtering and related enhancements, but do not enable anything that compromises the locked-performance profile. Never assume the Mali driver supports a feature without checking.
7. **Preserve provenance and legal boundaries.** Read graphics/audio data from the user's legally obtained ROM sets. Do not commit or distribute copyrighted ROM data, converted meshes, textures, recordings containing extracted assets or other generated game assets. Keep asset outputs out of Git by default. Check repository licence obligations for any shared code or dependencies.

### Source material to inspect before changes

- `raverace/HANDOVER.md` — latest build identity, known-good executable, rollback and current state.
- `raverace/INPUT_RECORDING_REPLAY.md` — authoritative deterministic replay and validation.
- `raverace/RENDERING_ARCHITECTURE_REVIEW.md` — recovered scene/display-list semantics.
- `raverace/GEOMETRY_INDEX_OPTIMISATION.md` — list walk, `geo_quad` and geometry/sort contracts.
- `raverace/GPU_HUD_PRODUCTION.md` — working native GPU HUD and production protections.
- `raverace/GPU_WORLD_ATLAS_INVESTIGATION.md`, `raverace/MALI_TEXTURE_ARRAY_DRIVER_INVESTIGATION.md` and `raverace/GPU_NATIVE_TEXTURE_INVESTIGATION.md` — failed designs and measured Mali limitations; **do not repeat them under new names**.
- Relevant implementation: `raverace/src/rr_scene.c`, `raverace/src/rr_dsp.c`, `raverace/src/rr_presentation.c`, `engine/slave_list.c`, `engine/geo_hw.c`, `engine/tex_bake.c`, and the active GLES renderer.

**Reference replay:** `/userdata/system/rave-racer-replays/race-20261013-122242-25136.rec`; representative measured window **ticks 1800–5639**. Preserve its original sidecars. The latest accepted production reference is approximately **52.9 FPS**, **25 FPS 1% low**, **32.7 ms p99 interval**, and **59.904 Hz simulation**. Reconfirm exact baseline and device state before any new comparisons; reports made under earlier HUD configurations are not interchangeable.

### Separate project layout (proposed, not mandatory)

```text
raverace/
  native/                 # Experimental native scene API, importer, renderer
    assets/               # ROM -> native asset compiler (source only)
    scene/                # Immutable model/material and per-frame instance structures
    render/               # Mali GLES backend, shaders, frame resources
    tools/                # Asset inspection, capture and replay utilities
    tests/                # Fixtures, format validation, determinism tests
  NATIVE_RENDERER_ROADMAP.md
  NATIVE_RENDERER_PROGRESS.md  # Created during Milestone 1
```

Keep generated asset packs, screenshots that embed extracted game data, large captures and temporary binaries in ignored local directories. Prefer small, independently testable interfaces to introducing a second giant copy of the emulator.

## 2. How Codex must execute this roadmap

1. **At the start of each task**, read this file, `HANDOVER.md`, the current Git status and the last milestone report. Identify the **one** milestone explicitly authorised by the user. If none is authorised, report the next available milestone; do not begin implementation.
2. Before coding, state the intended scope, assumptions, technical risks and acceptance tests **for that milestone only**. The milestone's final goal does not authorise work from later milestones.
3. Implement and validate the smallest useful end-to-end increment. Use representative actual ROM assets and recorded scene data rather than invented stand-ins for performance conclusions. Keep a functional fallback whenever touching shared interfaces.
4. **Stop at the milestone boundary.** A milestone is complete only when its required deliverables and exit tests are met. Mark `COMPLETE`, `BLOCKED` or `FAILED/RETHINK` with evidence. If blocked, stop and describe the blocker; do not bypass it or silently proceed.
5. Create/update `raverace/NATIVE_RENDERER_PROGRESS.md` with the completed work, changed files, reproducible commands, measurements, test results, unresolved issues and the **next milestone that requires approval**. Link raw logs and reports without committing copyrighted game assets.
6. **Do not automatically begin the next milestone, commit, push, deploy, alter the live launcher, delete user data or modify clock/driver/OS settings.** Ask the user for authorisation. When access to the handheld or original ROMs is unavailable, finish every possible host-side step and label the unverified hardware gate honestly.
7. For all device tests: use separate diagnostic executable names/paths, verify the known-good production hash, avoid concurrent transfers during benchmarks, preserve the sole `Ports/Rave Racer.sh` launcher, and restore EmulationStation. Do not change the production build to make tests easier.
8. If a milestone reveals that the architecture cannot meet its gate, propose a specific revised approach and **stop**. Do not turn a bounded milestone into an unlimited optimisation exercise.

**Required final message from Codex for each milestone:**

```text
MILESTONE <N> — <name>
Status: COMPLETE | BLOCKED | FAILED/RETHINK
Delivered: <files and functionality>
Verified: <tests, platforms, hashes, measurements>
Remaining risks: <specific unresolved issues>
Production status: <confirmed unchanged / authorised exception>
STOPPED. Next milestone <N+1> requires explicit user approval.
```

## 3. Milestones

### Milestone 1 — Recover and package reusable game assets

**Outcome:** A validated offline asset pipeline and an accurate inventory of what can be reused without reconstructing System 22 rendering on every frame.

**Scope**

- Map the Rave Racer ROM asset hierarchy: point-ROM models, object IDs, mesh/polygon packets, geometry/material flags, UV references, tile/pen maps, palettes, textures, and dynamic point-RAM/direct-polygon content.
- Trace `eng_walk_list()` → `geo_hw_object()` and identify a stable mapping from a display-list object's identity to its immutable geometry, separately from per-instance transformations.
- Classify assets as **static reusable**, **static with dynamic palette/material**, **dynamic transform**, or **runtime-generated geometry**. Document cases that cannot be preconverted safely.
- Implement an **offline converter** using the user's local ROMs. Export a documented, versioned native asset format supporting vertex/index buffers, normals or original lighting attributes, original material IDs, source UVs and asset provenance. Keep the converter deterministic; produce a manifest, content hashes, sizes and clear diagnostics for invalid data.
- Prepare a small representative set of **actual** models and associated materials, including road/terrain and at least one car or scenery object. A simple desktop viewer or CLI validation of extracted topology is enough at this stage; a handheld graphics renderer belongs to Milestone 2.
- Propose a material pipeline that **bakes actual RGBA once before gameplay**, with well-defined palette variants/updates. Consider mipmap generation, gutters and UV seams from the outset. Do **not** reproduce the rejected GPU-native map → pen → palette fragment shader.
- Estimate actual asset pack sizes, unique texture count, memory budget and feasibility of preloading a track. Measure instead of assuming all textures fit.

**Deliverables**

- Native asset converter and format specification, with versioning and checksum validation.
- Small native asset loader/inspector and representative extracted-asset validation results.
- `raverace/NATIVE_ASSET_INVESTIGATION.md` with model/texture counts, dynamic exceptions, expected memory and major risks.
- Initial `raverace/NATIVE_RENDERER_PROGRESS.md`.

**Exit gate — COMPLETE (2026-10-14)**

Evidence: [asset investigation](NATIVE_ASSET_INVESTIGATION.md) and
[progress report](NATIVE_RENDERER_PROGRESS.md). Representative assets are terrain
object 1280 and racing car body 1945; final visible pack has 144 validated material
recipes. All five gates below passed.

- [x] The converter rebuilds deterministic output from the same local ROM inputs, without storing ROM-derived assets in Git.
- [x] Representative meshes have validated topology, indices, UV/material associations and original dimensions/coordinate conventions.
- [x] The static/dynamic asset boundary is documented with at least one worked example of each important category.
- [x] A plausible bounded load/residency budget and required unknowns are documented.
- [x] The current emulator builds/tests as before and production is unchanged.

**STOP. Do not begin Milestone 2.**

### Milestone 2 — Build a standalone native frozen-scene renderer

**Outcome:** Render a representative actual Rave Racer scene using persistent native assets, with *no running game simulation* in the benchmark loop.

**Scope**

- Implement a separate GLES backend optimised for the H700/Mali-G31. Request/test actual supported GLES features; use a simple, performant RGBA shader first.
- Load the converted meshes/materials once before timing. Use persistent GPU vertex/index buffers, grouped material draws and stable GPU texture resources. No per-frame ROM decode or GL texture creation/deletion in the steady-state benchmark.
- Import real camera, instance transforms, per-object lighting, visibility/scissor information and material state from an existing frozen captured scene (start with tick 2500).
- Choose and document a native coordinate and transform convention. Conventional GPU vertex transforms and a depth buffer are acceptable, but original synthetic priority, painter-order dependencies, transparency, sky/road overlays, near clipping, direct polygons and HUD interactions must be either represented correctly or explicitly identified as visual gaps. Do not assume depth testing alone reproduces them.
- Add side-by-side captures against the current renderer at 640×480, a scene-coverage checklist, and automated geometry/material/instance checks. **Visual fidelity is the criterion; byte-identical pixels are not required.**
- Benchmark representative static scenes with resources resident and warmed. Report thread CPU, submission/completion spans, world rendering cost, draw counts, GPU memory, frame completeness and driver caveats. Separate loading/preprocessing from steady-state timing. Do not interpret `glFinish` timings as exclusive shader cost or as live-game FPS.

**Deliverables**

- Standalone renderer executable using original ROM-derived converted assets; preserve independent production binary.
- Frozen-scene comparison captures and `raverace/NATIVE_FROZEN_SCENE_REPORT.md`.
- Reproducible native and H700 build/test commands plus asset-validation tests.

**Exit gate**

- [ ] The captured scene is recognisably faithful: correct major geometry, road, car/scenery placement, textures, camera and layering; significant exceptions are documented.
- [ ] No per-frame rebuilding/uploading of static models or textures in steady-state mode.
- [ ] Repeatable **real H700** measurements establish whether the new path has convincing performance headroom over the incumbent frozen-scene path; do not claim a full-game FPS result.
- [ ] No gameplay/physics/simulation source changes, no production deployment.
- [ ] If the native scene is too slow or visually incomplete for the stated model, document a focused redesign and stop.

**STOP. Do not begin Milestone 3.**

### Milestone 3 — Replay a complete race with native scene commands

**Outcome:** Feed recorded, **time-varying** scene information to the standalone renderer over the complete representative race, without replacing the live emulator's graphics path.

**Scope**

- Build a versioned **native scene-command protocol** containing each picture's tick ID, camera/view/clip settings, ordered object instance IDs and transforms, material/palette revisions, animation state, depth/priority metadata and all necessary dynamic-geometry/direct-polygon events. Document which fields come from immutable ROM asset packs and which are per-picture.
- Capture those commands from the existing simulation/display-list processing **without influencing the authoritative simulation**. Keep capture diagnostics out of performance runs.
- Validate scene captures against the same recorded user-input replay; they must represent actual produced scenes, not a hand-authored camera path or track approximation.
- Render the complete tick window **1800–5639**, including texture-loading bursts around 1965 and later heavy sections. Verify content completeness for every tick; include at least one additional course/scene if reliably available.
- Implement bounded scene buffering, per-tick palette versions, dynamic object and direct-polygon handling. Static geometry and textures should stay resident; per-frame uploads should be limited to genuinely dynamic data.
- Compare side-by-side sequences with the old renderer, checking scenery continuity, car animation, race HUD/world layering, camera movement and persistent texture/material identity.
- Benchmark uncapped replay capacity, p50/p95/p99/worst frame intervals, 1% lows, CPU/GPU stages, cache occupancy and any loading spikes. Distinguish **offline scene playback capacity** from true simulation+rendering throughput.

**Deliverables**

- Versioned scene-command capture/playback utility and stable scene API.
- Complete replay clips or comparison captures stored outside Git where they contain game assets.
- `raverace/NATIVE_RACE_REPLAY_REPORT.md` with coverage, exceptions and real-device performance data.

**Exit gate**

- [ ] Every required tick's scene command packet is accounted for; no omitted polygons/objects are hidden to improve performance.
- [ ] Scene output retains the original track, car movement, scenery, HUD/world relationships and atmosphere to an agreed visual standard.
- [ ] Static resources are reused through the lap and resource-loading hitches are measured explicitly.
- [ ] The H700 race-playback benchmark demonstrates a credible path to the 16.693 ms total picture deadline **with headroom for simulation and audio**; otherwise mark blocked and stop.
- [ ] Production emulator, physics and audio paths are unchanged.

**STOP. Do not begin Milestone 4.**

### Milestone 4 — Integrate live original gameplay with the native renderer

**Outcome:** A separate playable native-renderer build driven by the *unchanged original Rave Racer game logic*, with no dependence on the old GLES world renderer.

**Scope**

- Connect the native scene bridge to the actual emulation-owned display-list and video state. The translated 68020/master DSP/game systems remain the **authoritative** simulation. Do not duplicate or replace vehicle physics, AI, controls, collisions or race rules.
- Preserve hardware-visible writes, PDP/list eligibility, vblank, direct-polygon consumption and palette updates at the original tick boundaries. Rendering threads must not read mutable game state without an immutable captured snapshot.
- Handle skipped or delayed scene packets explicitly without changing simulation state. Do not extrapolate physics or interpolate authoritative gameplay states as a replacement for simulating them.
- Use the existing GPU HUD/audio pipelines where compatible, adapting interfaces only as needed. Original controllers, sound, menus and race progress must work.
- Test the replay twice (original production and new live-renderer build) with the **same input, ROM, EEPROM and reset state**. Compare tick-by-tick simulation/board hashes, recorded car/camera/race-state probes where available, events and PCM/voice digests. Any difference in actual game state is a **blocking regression**; visual differences alone are not.
- Run native/ARM64 race tests and available race/thread-safety gates. Run interactive handling tests on the handheld as an additional subjective acceptance check.
- Keep the native backend opt-in in an isolated build/runtime selector; preserve the original GLES path.

**Deliverables**

- Separate playable native-renderer experimental executable (not a second permanent Ports launcher).
- Original-vs-native deterministic simulation/audio comparison results.
- `raverace/NATIVE_LIVE_INTEGRATION_REPORT.md` with any residual visual/functionality gaps and real-world H700 pacing.

**Exit gate**

- [ ] The user can drive a full race with the original controls, physics, traffic, collisions, scoring, HUD and audio.
- [ ] Deterministic **gameplay state and PCM/voice outputs** match the reference for identical replay input; unexpected differences are resolved or block completion.
- [ ] The native renderer does not mutate or reschedule game logic to improve picture rate.
- [ ] Visual output is acceptable and major missing features are resolved or explicitly approved by the user.
- [ ] Production executable/launcher remain untouched and fully recoverable.

**STOP. Do not begin Milestone 5.**

### Milestone 5 — Achieve locked presentation, then add optional enhancements

**Outcome:** A smooth native Rave Racer on the H700, with a measured locked-cadence profile and optional graphics improvements that do not break it.

**Part A — Prove sustained frame pacing first**

- Measure the **actual** handheld display refresh and EGL/SDL swap behaviour. Distinguish true arcade 59.9040599 Hz from a nominal 60.000 Hz panel; do not conceal any cadence mismatch through reported average FPS. Decide on a documented panel-compatible presentation policy, preserving authoritative simulation speed unless the user explicitly chooses an alternate game-speed policy.
- Optimise the native scene backend, source loading, material batching, draw submission, geometry residency, dynamic upload and threading based on full-game measurements. Preload track assets before the race where memory permits; allow longer loading times if they eliminate in-race stalls.
- Use a deadline-oriented scheduler and explicit tracking of every simulated, rendered, submitted and displayed picture. Measure missed display deadlines, repeated pictures, queue latency and image age; GPU completion and display scanout are different events.
- Test cold and warm loads, multiple courses where available, dense city/traffic scenes, texture-rich sections and extended play. Repeated runs must include audio and input, not just an isolated world benchmark.
- **Acceptance target:** simulation remains at original arcade rate; one new complete game picture is available per intended refresh over accepted representative gameplay, no routine skipped simulation pictures, no audio underruns/resyncs, and stable refresh-paced presentation. Where the physical panel prevents exact arcade cadence, quantify repetitions/mismatches and seek the smoothest non-speed-altering policy. Report **all** exceptional misses; a median of 59.9 FPS is insufficient.

**Part B — Add enhancements only after Part A passes**

- Provide selectable modes: `Authentic` (original look/nearest filtering), `Performance` (locked refresh is mandatory), and `Enhanced` (optional effects under a measured budget).
- Enhanced draw distance to eliminate graphics pop-in.
- Evaluate **FXAA** and, if supported, **MSAA** with the actual render-target/resolve cost. Keep the original HUD legible, avoiding indiscriminate post-AA across text.
- Generate correct mipmaps and edge padding for converted textures; then assess bilinear/trilinear and **anisotropic filtering** only if the driver reports the relevant feature. Compare road shimmer, texture seams, distant ground and overall arcade fidelity.
- Consider optional fog/distance shading or mild colour treatment only where consistent with the original visual identity. Avoid reinventing the art style, adding gameplay-affecting visual effects or changing geometry just for aesthetics.
- Test each enhancement alone and in sensible combinations. Make all graphics upgrades reversible without modifying simulation, asset provenance or original game data.

**Deliverables**

- Pacing scheduler/telemetry with independent display-cadence evidence.
- Repeatable H700 performance suite and `raverace/NATIVE_60HZ_ACCEPTANCE.md`.
- Graphics mode settings, supported-feature detection and user-facing comparison images.
- A final deployment proposal **for user approval only**; not an automatic production replacement.

**Exit gate**

- [ ] The chosen native profile meets and documents the refresh-cadence, zero-routine-drop and audio/state targets over the agreed representative workloads.
- [ ] No silent interpolation, game-state skipping or simulation-speed modification was used to manufacture 60 FPS.
- [ ] Each enhancement has a measured image-quality benefit, explicit performance cost and reversible setting.
- [ ] The production port and recovery path remain available.
- [ ] If the hardware cannot meet the locked target, state the measured limit and required concessions/hardware change honestly; do not mark complete merely because average FPS reaches 60.

**STOP. Do not deploy, declare release-ready or begin a new feature phase without explicit user authorisation.**

## 4. Global measurement and review rules

- **Simulation correctness is binary.** Any novel discrepancy in state, handling, collision outcomes, score progression or audio versus the original authoritative simulation must be investigated, not waved away as rendering tolerance.
- **Visual correctness is reviewed, not byte-equal.** Use same-tick side-by-side scenes, show material/geometry coverage and document accepted artistic/raster differences. Do not omit difficult geometry or effects silently.
- **Performance tests must be real-device tests.** Host llvmpipe and desktop GPUs are useful for functional tests but cannot substantiate H700 FPS claims. Record firmware/driver, governor/clock state, resolution, temperature, resource budget, audio mode, swap interval, preparation, image age and pacing.
- **Use comparable workloads.** First-lap replay ticks 1800–5639 for continuity with existing studies; add dense and longer workloads before claiming locked performance. Use at least five alternating paired device runs for meaningful final A/B comparisons, and do not combine timing buckets from unrelated modes.
- **One frame per simulation tick is distinct from display refresh.** Track source tick, scene creation, CPU/GPU completion and scanout separately. Account for the 59.904 vs 60.000 Hz discrepancy if present.
- **Memory counts.** GPU allocations share physical RAM with the CPU on the H700; do not add GPU accounting and RSS as if they are separate physical allocations. Measure peak RSS and estimates of actual GPU resource footprint.
- **Avoid benchmark shortcuts.** No prerecorded pre-baked gameplay poses, fake scene state, hidden presentation skips, unmeasured image approximations, input replay-specific branches or disabled audio. Offline asset preparation may be ROM/track-specific, but the live renderer must handle unseen valid gameplay.

## 5. Instructions to paste into Codex

### Start Milestone 1

> Read `raverace/NATIVE_RENDERER_ROADMAP.md` and `raverace/HANDOVER.md`, inspect the current repository and production state, and execute **Milestone 1 only**. Implement the offline asset inventory/converter and the required validations without changing the production playable. Write the milestone investigation and progress files, report the acceptance evidence and **STOP**. Do not begin Milestone 2 or create commits/deploy unless I separately authorise it.

### Continue or begin a subsequent authorised milestone

> Read `raverace/NATIVE_RENDERER_ROADMAP.md`, `raverace/NATIVE_RENDERER_PROGRESS.md`, and `raverace/HANDOVER.md`. I authorise **Milestone N only**. Confirm prior milestone's exit gate, execute the authorised milestone with its stated constraints, test and document the result, and **STOP** at its exit gate. Do not begin Milestone N+1, deploy, push or commit without my separate authorisation.

---

**Design intent:** This is a **native rendering port first**, not a complete gameplay remake. Recover and reuse original assets, preserve the original game's physics and rules, build for the H700, prove full-speed *complete* rendering, and make modern graphics optional. Every milestone must produce independently reviewable evidence before the next is authorised.
