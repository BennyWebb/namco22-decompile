# ARM64 scheduler compatibility test

`RR_HEADLESS_ONLY=ON` builds only `rr_headless`. The default CMake configuration
continues to build the desktop game as before. This test is a separate milestone:
it does not introduce a GLES renderer.

The test calls the actual Rave Racer reset entry and `rr_main.c` scheduler,
with the existing byte-addressed memory map, board devices, C139 link model,
translated 68K/C71 DSP/C74 sound programs and C352 mixer. It runs unpaced and
stops after a fixed number of scheduler frames. There is no SDL, OpenGL, window,
controller, socket transport, ZIP extraction or audio device in the executable.
The sound CPU and C352 still advance: their clocks and status registers are part
of the board model. Only generated audio output is discarded. Scene bookkeeping
retains the DSP's display handshake; direct polygon output is discarded each
frame instead of rasterised. No replacement game simulation is used.

For a stable translated baseline the test forces `RR_RD=0` (no readable-C
substitutions) and removes inherited `RR_*` and `SND_*` variables before subsystem
initialisation. It does not load desktop controls, replay files or persistent
EEPROM saves. Every run begins with the ROM directory's EEPROM image. Use the
same ROM files, executable revision and command-line inputs on both machines.

## Build

Run from the repository root. Native prerequisites are GCC, CMake, make and
host Python 3; no SDL or graphics development packages are needed for this target.
Keep GNU C and `-fno-strict-aliasing -fwrapv`. Generated lifted C retains the
existing optimisation settings; the large DSP translation can take a while.

```sh
cmake -S raverace -B /tmp/rr-headless-native \
  -DRR_HEADLESS_ONLY=ON -DCMAKE_C_COMPILER=gcc -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/rr-headless-native --target rr_headless -j2

cmake -S raverace -B /tmp/rr-headless-arm64 \
  -DRR_HEADLESS_ONLY=ON \
  -DCMAKE_TOOLCHAIN_FILE=cmake/knulli-aarch64.cmake \
  -DKNULLI_HOST="$HOME/dev/knulli-scarab/output/h700/host" \
  -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/rr-headless-arm64 --target rr_headless -j2

file /tmp/rr-headless-arm64/rr_headless
readelf -h -l -d --version-info /tmp/rr-headless-arm64/rr_headless
```

The cross file selects the H700 Buildroot GCC, not Ubuntu's AArch64 GCC. Python
is a host build tool. Configure in separate build directories for each compiler;
do not run simultaneous builds in the same directory. `/tmp` build directories
may disappear after reboot.

ROMs are not distributed with the test. Use your existing extracted
`raverace.zip` and `namcoc74.zip` files under `raverace/extracted/`. The existing
Python sound/DSP generation commands still require these files at build time.
The point ROMs, sound program/BIOS, wave ROMs and EEPROM image are needed at
runtime; missing required startup files fail with status 2. This target does
not perform automatic ROM extraction.

## Run and output

```sh
/tmp/rr-headless-native/rr_headless raverace/extracted --frames 120 \
  > /tmp/rr-headless-120.json 2> /tmp/rr-headless-120.log

/tmp/rr-headless-native/rr_headless raverace/extracted \
  --frames 600 --coin 180 --gas 200 --steer 300:2304 \
  > /tmp/rr-headless-600.json 2> /tmp/rr-headless-600.log

python3 raverace/tools/test_headless.py \
  /tmp/rr-headless-native/rr_headless raverace/extracted
```

Supported options:

- `--frames N`: positive frame limit (default 600); frame scheduling remains
  the existing 83,000 instruction budget split into 16 slices.
- `--coin F`: coin-1 pulse starting at frame F, released six frames later;
  repeat up to eight times. These are the existing scheduler input operations.
- `--gas F`: hold the existing game-specific maximum throttle from frame F.
- `--steer F:V`: hold wheel value V (0..4095) from frame F; repeat up to eight
  times. Later entries take precedence when their frame is reached.
- `--dump DIR`: existing binary RAM dumps at 60-frame intervals and the final
  frame. Create the directory first. Dumps are optional and independent of JSON.
- `--help`: print usage. Input frame numbers are positive and one-based.

Stdout contains one JSON object at the final frame boundary, after that frame's
interrupt delivery. Logs go to stderr. The JSON contains D0–D7/A0–A7, SR, the last
executed translated instruction PC, FNV-1a 32-bit checksums, frames/slices,
executed 68K instruction count, delivered IRQ counts and device execution/fault
counters. `last_instruction_pc` is instrumentation of the translated program,
not a claim that the Ghidra PC register slot holds the current execution PC.
`instructions_68k` includes instructions in IRQ handlers. DSP steps/sound cycles
are the existing device counters (which can reset with their device).

Checksums cover explicit byte arrays; polygon words are serialised big-endian,
avoiding host struct padding and pointer representation. FNV checksums are
regression checks, not cryptographic hashes or proof of complete machine-state
equivalence. The summary intentionally excludes elapsed time and host identity.

Exit status: 0 at the requested limit without CPU traps or DSP/sound faults;
2 for invalid arguments/startup failures; 3 for detected execution faults at the
limit. A fatal lifted-runtime error uses the existing runtime's nonzero exit
code and emits a partial summary via its fatal hook. Unexpected reset-entry
return exits 1. Always check both process status and `frames` when comparing.

For the local 120-frame idle run, selected observed fields were:

```json
{
  "frames": 120,
  "slices": 1920,
  "instructions_68k": 10351846,
  "last_instruction_pc": "00026c5c",
  "sr": "2000",
  "fnv1a32": {"wram": "f627692b", "shared": "de429a88"},
  "traps": 0,
  "dsp_fault": false,
  "sound_fault": false
}
```

This is an excerpt of observed output, not a hardcoded expected result. ROMs or
future source changes can legitimately change these values. Compare newly
captured desktop and device summaries from the same revision.

## Run the identical test on Knulli

No Ports integration or display shutdown is needed for this graphics-free test.
Set the actual SSH address, then copy the executable and your same ROM directory:

```sh
export KNULLI_HOST=root@192.168.1.100
ssh "$KNULLI_HOST" 'mkdir -p /userdata/roms/ports/rave-headless'
scp /tmp/rr-headless-arm64/rr_headless \
  "$KNULLI_HOST":/userdata/roms/ports/rave-headless/
scp -r raverace/extracted "$KNULLI_HOST":/userdata/roms/ports/rave-headless/
ssh "$KNULLI_HOST" 'chmod +x /userdata/roms/ports/rave-headless/rr_headless'

ssh "$KNULLI_HOST" 'cd /userdata/roms/ports/rave-headless && ./rr_headless extracted --frames 600 --coin 180 --gas 200 --steer 300:2304 > state-arm64.json 2> headless.log'
scp "$KNULLI_HOST":/userdata/roms/ports/rave-headless/state-arm64.json \
  /tmp/rr-headless-arm64.json

python3 - <<'PY'
import json
with open('/tmp/rr-headless-600.json') as f:
    desktop = json.load(f)
with open('/tmp/rr-headless-arm64.json') as f:
    device = json.load(f)
assert desktop == device, 'Desktop/device final states differ'
print('Desktop and device final summaries match')
PY
```

If it fails before producing JSON, inspect `headless.log` and run target `ldd`
on the executable. Expected loader: `/lib/ld-linux-aarch64.so.1`. No SDL, GL,
EGL, GLES, zlib or libstdc++ dependency should be required.

## Validation record

Native GCC 15.2 Release and H700 Buildroot GCC 13.3 Release builds succeeded.
Native 120-frame idle and 600-frame scripted runs completed without traps,
unmapped accesses, ROM writes or DSP/sound faults. The Python regression checks
verify repeated identical state, inherited-environment isolation, effective
scripted input, real execution counters and rejected invalid CLI arguments.
The normal desktop `rr` target also rebuilt and ran with SDL offscreen and
`RR_RD=0`; all six final-frame RAM dumps at frame 120 matched the headless
test byte for byte. Existing compiler warnings remain (including a nested-comment
warning in `rr_hw.c`); no warning cleanup is included in this milestone.
The ARM64 ELF identifies Linux 4.9.0, AArch64 and the expected loader; its NEEDED
entries are libc, libm and the loader, with GLIBC requirements up to 2.34.
No graphics, SDL, audio-device or controller libraries are linked.

ARM64 execution remains unverified here: no QEMU user emulator or connected
handheld was available. Cross-machine comparison must be performed using the
commands above. This milestone says nothing about rendering or playable frame
rate on the handheld.
