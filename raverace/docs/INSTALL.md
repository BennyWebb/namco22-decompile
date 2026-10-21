# Install Rave Racer on a Knulli RG40XX H

You need an Anbernic RG40XX H running Knulli (tested with scarab 2026/05/11,
Python 3.12), a computer to copy files, and at least 1 GiB free on the games card.
This build supports this device, not other handhelds. No public binary package
is provided; build the installation ZIP from source using your own dumps.
The executables contain Game ROM-derived code, the complete C71 BIOS, C71
BIOS-derived code, and C74 sound BIOS-derived code and instruction bytes.
The source tree also includes Game ROM-derived code and C71 BIOS content.
The ZIP contains no Rave Racer graphics assets, sound samples, EEPROM images,
saves, recordings or game screenshots.

Use your own legally obtained board dumps: Rave Racer World revision B
(MAME `raverace`) and the Namco C74 sound BIOS (`namcoc74`). This project does
not provide ROM files. Extract the files on your computer; do not rename
a different revision to match. The exact filenames and identities are below.

## Build and copy

1. Build `rave-racer-knulli-h700-<version>.zip` on your computer using
   **Build an installation ZIP on a Linux PC** below. Record the SHA256 printed
   by the packaging command so you can verify the copied ZIP.
2. Extract the ZIP **into the card's `roms/ports/` directory**. Do not create
   another enclosing directory. You should have these two items:

   ```text
   /userdata/roms/ports/Rave Racer.sh
   /userdata/roms/ports/rave-racer/
       README.md
       bin/
       launcher/
       roms/
       cache/
       saves/
       logs/
   ```

3. Copy the 36 extracted ROM files into `rave-racer/roms/`. Place the files
   directly there, not ZIPs or another subdirectory.
4. Refresh Knulli's game list or restart EmulationStation. Open **Ports →
   Rave Racer**. You do not need SSH, pip or an Internet connection on the device.

Everything the game creates stays inside `rave-racer/`. It does not use
`/userdata/system` or `/userdata/saves`. No setup script needs to run at boot.

## First launch

The launcher checks all ROM sizes, CRC32s and SHA256s. A missing or incorrect
file is named on screen; press a button to return to Ports. Multiple error
pages are advanced with a button. It then displays a preparation screen while
compiling only Rave Racer textures, meshes and material tables from your ROMs
on the handheld. Sound samples are read from your wave ROMs at run time.
The streaming compiler uses system Python and the included C library; it does
not need NumPy or Internet access. Progress shows the stage and percentage.
Do not switch it off during preparation. START+SELECT cancels the build;
the next launch discards incomplete output and starts again. The measured asset build takes 3–4 minutes at 1,512 MHz, including
verification. Automatic calibration takes another 90 seconds; allow about
5–6 minutes before first play.

Next, the game's own TEST/SERVICE adjustment runs automatically for 1,600 ticks
with centred steering and released pedals. This creates a calibrated EEPROM
in `saves/rr_eeprom.nv`; no stored EEPROM image is supplied. The initial
SHA256 must be `81e19e9ce2fca04dbbd487b12e85cc473e30e61f2797c703743c9b86373721c7`.
Interactive play starts only after that check passes. Do not press controls
while the automatic adjustment screens are visible.

Later launches verify the package, ROMs and existing asset cache, and reuse your
saved EEPROM. They do not repeat compilation or reset your calibration, scores
or settings. A damaged cache is reported, never silently replaced by a different
renderer. To rebuild it deliberately, exit and delete the contents of `cache/`.

## Controls and logging

The RG40XX H built-in mapping is included. SELECT inserts a coin, START starts,
B or R1 accelerates, A or L1 brakes, and the left stick or D-pad left/right
steers. D-pad up/down shifts gears; Y changes view and X toggles the overlay. **START+SELECT together exits to EmulationStation.**
The CPU runs at 1,512 MHz during the session; original CPU and GPU governors
are restored on exit, including recovery by an independent watchdog.

To customize mapping, copy `launcher/controller-input.cfg` to
`saves/controller-input.cfg` and edit the EmulationStation-style input IDs.
Keep the device GUID and name. Your saved copy overrides the included mapping.
External controllers need an appropriate mapping for their own device.

Normal play records a small bounded `logs/play.log` (64 KiB plus one rotation)
and a launch summary; it does not collect per-picture profiles or sample
resources. Measured supervisor overhead is about 0.43% of one CPU core and
12 KiB of logs per minute. For explicit diagnostics, change the last line of `Rave Racer.sh`
to append `--telemetry full` after `"$@"`, then revert that change afterwards.
Full profiles are under `logs/sessions/`; delete them when no longer needed.
Saves are persisted locally; back up `saves/` before updating or removing the game.

## Update or uninstall

Exit first. Back up the entire old `rave-racer/` folder to your computer for
rollback. Extract the new package over the old files, keeping `roms/` and
`saves/`. Existing cache generations can be deleted while idle to reclaim space.
To roll back, restore your entire backup and its matching `Rave Racer.sh`.
Do not copy a generated test EEPROM over a personal save.

To uninstall, delete **`Rave Racer.sh` and `rave-racer/`** from `roms/ports/`,
then refresh the game list. There are no separate system or save directories.

## Troubleshooting

| Error or symptom | What to do |
|---|---|
| ROM check failed: Missing / Wrong | Copy the named file from your own correct dump. Compare the identities below. ZIPs must be extracted. |
| Package file damaged | Extract a fresh copy of the same local build, preserving your ROMs and saves. |
| Asset compiler failed (stage named) | Read `logs/setup.log`; check free space, correct Knulli/Python version and intact package files. |
| Asset cache damaged | Exit, delete cache contents, and relaunch to compile again. Keep ROMs and saves. |
| EEPROM calibration failed | Read `logs/calibration.log`. Check ROMs and package integrity; do not substitute an uncalibrated EEPROM. |
| EEPROM damaged | Restore your personal save. For a deliberate factory reset, move the damaged EEPROM aside and relaunch. |
| Rave Racer is already running | Exit the other session. If no game is running, restart the handheld. |
| Clock watchdog unavailable / Clock restoration failed | Restart the handheld; check the package and writable storage before retrying. |
| Setup cancelled or display unavailable | Relaunch from Ports to restart setup. Check the package if its setup screen cannot open. |
| Save write failed | Check free space and card health; preserve your last good save backup. |
| Game stopped (exit 86) | Unsupported content was detected. Retain `logs/latest-summary.txt` and report it. There is no substitute rendering path. |
| Game stopped (other exit) | Read `logs/play.log`, or the full session logs if enabled. Report any sound translation trap. |
| Other operating-system error | See `logs/last-error.txt`; check writable storage and the supported Knulli version. |

If setup cannot open its display, the launcher exits instead of starting the game;
inspect `logs/last-error.txt` and try from EmulationStation, not a second graphical
session. A long list of ROM errors may span more than one screen.

## Required ROM files

SHA256s and CRC32s identify the dump; these hashes contain no ROM payload.

| File | Bytes | CRC32 | SHA256 |
|---|---:|---|---|
| `c74.bin` | 16384 | `a3dce360` | `4144cc885f6468e92a116d0d629f8ea83828a5194ac054415470dbfa9221b5bb` |
| `rr1gam.2d` | 256 | `b2161bce` | `96fee23bf6505fc8352cb695b7c8bd9a74cb2a42457e27fcfb45b8c628141e64` |
| `rr1gam.3d` | 256 | `b2161bce` | `96fee23bf6505fc8352cb695b7c8bd9a74cb2a42457e27fcfb45b8c628141e64` |
| `rr1gam.4d` | 256 | `b2161bce` | `96fee23bf6505fc8352cb695b7c8bd9a74cb2a42457e27fcfb45b8c628141e64` |
| `rv1ccrh.5c` | 524288 | `a741b262` | `c89ab12278e823b057d99091828576445f5617acf3347ea779de71f5b7a6eb4a` |
| `rv1ccrl.5a` | 2097152 | `bc634f72` | `c9f36979585457186304ccce2b7ee72b3f84cbc1c74560bfc5da08907297a99a` |
| `rv1cg0.1a` | 2097152 | `c518f06b` | `e8c39e37259d0a69332ac2a0909d460c1fedffa5896cafb2508e3fd05a5293cd` |
| `rv1cg1.1c` | 2097152 | `6628f792` | `8bb17af866e1a97421519405ebfedcc9c3a5ff6c32be51ca2247670a977db8a4` |
| `rv1cg2.1d` | 2097152 | `0b707cc5` | `ed89673d15b1e39f93c67a973bd5d70a7ac6b61f49b6e573fb686727e922e7ec` |
| `rv1cg3.1e` | 2097152 | `39b62921` | `2b669aa4102a879d7d26071c354038d00803055b43d9d53229d968e2602a865f` |
| `rv1cg4.1f` | 2097152 | `a9791ea2` | `daff8e4c5183edcb04fcc7b5cf3d2566d12681a7ca7dd6216dc415e194ac0443` |
| `rv1cg5.1j` | 2097152 | `b2c79ec1` | `80214a88732d30500f0c3d1a6e54c414c2f49f40d8a5e9c2807bbdc0c0582658` |
| `rv1cg6.1k` | 2097152 | `8cddedc2` | `176c2b1ed8f9c5c403b0f73f2f0246097c879937fcc347ab1cb3ab6e8d013c38` |
| `rv1cg7.1n` | 2097152 | `b39147ca` | `829d9d715711243346e8feca03299ad925c90e1f38f8ff54c19baabb72224186` |
| `rv1data.6r` | 524288 | `d358ec20` | `3d19693d12ff8feb183a838fbd282a87183e9f8ccb71596e1e0f16f128fee578` |
| `rv1eeprm.9e` | 8192 | `e00dd412` | `a34d3b84b16df89e899d9d927a54d8546748aa0e5181f644b1d12f76b84cd866` |
| `rv1potl0.5b` | 524288 | `de2ce519` | `f7e7a364c37043cb50f778a836f4fd04ecb1fa79c49be7be37ea0a6fc3a8b714` |
| `rv1potl1.4b` | 524288 | `2215cb5a` | `a2ed893ff7789763649d434a62a627266112e4213734788c01669180ae06b8ca` |
| `rv1potl2.3b` | 524288 | `ddb15bf7` | `90933fbf8d33958faf84c4279acf4d165e87439bd5ad03ab16d062751a4423d9` |
| `rv1potl3.2b` | 524288 | `fa9361ca` | `bd579a2e713392a3b23695bc69a50a02b1d3cc9b97821aa6ec2691c125e87d12` |
| `rv1potm0.5c` | 524288 | `3c024f3a` | `6c79bded78635f3849be76f07e84720ada5dc7700e427f4185ef95f03b3262f5` |
| `rv1potm1.4c` | 524288 | `b1a32a68` | `6baf343f7d759d322d7d0f4352974f40606949f83e9371d5ba5267eb266bdd4d` |
| `rv1potm2.3c` | 524288 | `a414fe15` | `1d903510644627dbb7c48bd80203010e3f2c59843b6ba1807947382658c71e32` |
| `rv1potm3.2c` | 524288 | `2953bbb4` | `73ae3e6ea51849287feceffcbf90273c0df11b0cc7c4699fa1592b5023e0eca4` |
| `rv1potu0.5d` | 524288 | `b9eaf3cc` | `192bbc3d37caeca608dc4c4d366118c535be4c0043d1647525fb1da92ff56f30` |
| `rv1potu1.4d` | 524288 | `a5c55258` | `5dc7d20ce1917eabd044fbe74c47dd9716a4ae21c7921059363df483e3e8d794` |
| `rv1potu2.3d` | 524288 | `c18fcb74` | `74c0d441881f218fc1113f1b261ce14a218848aa9c0301a83592030a811ed9ab` |
| `rv1potu3.2d` | 524288 | `79735aaa` | `1d5f00fbb638c9bb411f9f1f78f6aa103cca46e6b9d6dfe00be406a6133a3d96` |
| `rv1wav0.10r` | 1048576 | `5aef8143` | `614299c3f03bdf636305e1e3643ea2d406be8f7a85f1f8b46668f59c9918f09e` |
| `rv1wav1.10p` | 1048576 | `9ed9e6b3` | `4b65868688f877441a06b322983b5bc3db487120531a1253aa6b6cf5841cdb70` |
| `rv1wav2.10n` | 1048576 | `5af9dc83` | `8c680b35cbd7e6f662e7ca4ac772f0a24f7c3f209c22a6549f8192340e618af7` |
| `rv1wav3.10l` | 1048576 | `ffb9ad75` | `9c796a6480712c18f4d15c8957308ddff2c66d85d738d4e0f845a8918f65fc0e` |
| `rv2_prgllb.4d` | 524288 | `3017cd1e` | `cc8a1e04546ae7e59583628ce5fef10e2207ba334d3f518f068de00f5268a77a` |
| `rv2_prglmb.2d` | 524288 | `894be0c3` | `fc6cc2886b442714f5dc4341bbf3633d1a80c562ac511d3ae6adea16039c250e` |
| `rv2_prgumb.8d` | 524288 | `6414a800` | `e3a0d90407d1bb9724ac310ca24775937ec389d830a626d4b48da66dddbba0e2` |
| `rv2_prguub.6d` | 524288 | `a9f18714` | `63f25d1d4af12885552ac46674a95aea82438af8bdbb80af897150b28d961c89` |

## Build an installation ZIP on a Linux PC

Install the repository's documented host build dependencies and the Knulli
H700 cross SDK. Run these commands from `raverace/`, with your own ROMs available
to the build tools. The resulting executables contain the content described
above and are copied into the installation ZIP.

```sh
cmake --preset knulli-h700 -DRR_ROM_DIR=/absolute/path/to/your/extracted-ROMs
cmake --build --preset knulli-h700 --target rr_gles3 rr_gles3_setup rr_message rr_asset_codec gles3_session_io
python3 gles3/tools/release.py --build build/knulli-h700 --output build/packages --version 0.1.0
python3 gles3/tests/test_release.py build/packages/rave-racer-knulli-h700-0.1.0.zip
```

The packaging command is offline. It copies compiled executables/libraries,
asset compiler source, text shaders, device configuration, hashes and this guide.
It does not read a ROM directory directly; it packages the executables with the
content described above. There is no NumPy dependency in the package. Build
outputs remain ignored.
