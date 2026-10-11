# Knulli SDL2 / OpenGL ES triangle

Standalone C++17 graphics probe for the RG40XX H, independent of all game
targets. Requests a fullscreen 640×480 GLES 2.0 context, draws a rotating
RGB triangle, logs graphics/input diagnostics, and caps rendering at 60 FPS.
Rendering uses only core GLES 2.0 entry points. SDL owns EGL/context creation;
the application links SDL2 and GLESv2, not desktop GL, X11, Wayland or DRM.
Native desktop SDL may itself depend on desktop libraries. The target SDK must
provide Knulli's SDL with its custom `mali` backend and compatible vendor GLES
libraries; linking generic desktop SDL would not meet the target requirement.

## Native development

Prerequisites: CMake 3.18+, a C++17 compiler, pkg-config, SDL2 development
headers and `sdl2.pc`, GLES2 headers and the GLESv2 shared library. On Debian/
Ubuntu these development packages are typically `libsdl2-dev` and
`libgles2-mesa-dev`. No dependencies or SDKs are downloaded by this project.

From the repository root:

```sh
cmake -S tools/knulli-gles-test -B tools/knulli-gles-test/build-native -DCMAKE_BUILD_TYPE=Release
cmake --build tools/knulli-gles-test/build-native -j2
tools/knulli-gles-test/build-native/knulli-gles-test
```

The program preserves an existing `SDL_VIDEODRIVER`. When unset, it selects
`mali` if SDL lists that backend; otherwise it lets desktop SDL choose. Escape
exits on desktop. For an unattended software-rendered smoke test:

```sh
MESA_GLES_VERSION_OVERRIDE=2.0 SDL_VIDEODRIVER=offscreen \
  tools/knulli-gles-test/build-native/knulli-gles-test --seconds 6
```

`--seconds N` is optional and exits successfully after a positive duration.
Fullscreen mode can change the desktop display mode; the offscreen driver
avoids that during unattended tests.

## ARM64 cross build

Obtain a trusted, matching Knulli Scarab / H700 Buildroot 2024.11 SDK from
the firmware build or its maintainers. A Buildroot SDK normally requires its
supplied relocation script to be run after extraction. Follow that SDK's
instructions. **No compatible SDK or ARM64 compiler was available locally.**
The paths below are placeholders for the actual SDK; do not use the host's
Ubuntu sysroot or assume its glibc ABI matches the handheld.

Required SDK contents:

- AArch64 Linux glibc GCC/G++ supporting C++17, including target libstdc++.
- Target sysroot with glibc headers/startup objects and dynamic loader,
  SDL2 headers, `usr/lib/pkgconfig/sdl2.pc`, GLES2 headers and linkable shared
  GLESv2/SDL2 libraries (with their symlink targets).
- Knulli-compatible Mali SDL/EGL/GLES runtime ABI. The device's installed
  runtime `.so` files alone are not a complete development sysroot.

Example exact build sequence, after setting the two SDK paths:

```sh
export KNULLI_TOOLCHAIN_PREFIX=/absolute/path/to/sdk/bin/aarch64-buildroot-linux-gnu-
export KNULLI_SYSROOT=/absolute/path/to/sdk/aarch64-buildroot-linux-gnu/sysroot
cmake -S tools/knulli-gles-test -B tools/knulli-gles-test/build-arm64 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/aarch64-linux.cmake \
  -DCMAKE_BUILD_TYPE=Release
cmake --build tools/knulli-gles-test/build-arm64 -j2
file tools/knulli-gles-test/build-arm64/knulli-gles-test
readelf -h tools/knulli-gles-test/build-arm64/knulli-gles-test
readelf -l tools/knulli-gles-test/build-arm64/knulli-gles-test
readelf -d tools/knulli-gles-test/build-arm64/knulli-gles-test
readelf --version-info tools/knulli-gles-test/build-arm64/knulli-gles-test
```

Compiler prefix naming varies by SDK. The toolchain restricts dependency
discovery and pkg-config to the supplied sysroot. If changing SDKs, use a
fresh build directory. Expect ELF64, machine AArch64 and interpreter
`/lib/ld-linux-aarch64.so.1`. Inspect `NEEDED` entries: they should use target
SDL2/GLESv2 SONAMEs plus ordinary C/C++ runtime libraries, with no direct
X11/Wayland/DRM/desktop OpenGL dependencies. Check transitive dependencies
on the device too. Do not run host `ldd` on the ARM64 executable.

Match required GLIBC, GLIBCXX and CXXABI symbol versions to the device's
libraries, and ensure the SDK's libc supports the device's Linux 4.9.170
kernel. A newer generic ARM64 toolchain can produce a correctly identified
ARM64 ELF that still cannot run here. This test does not use the main repo's
glibc compatibility shims, static linking or copied desktop graphics libraries.

## Deploy and launch

Use SSH credentials configured on your Knulli installation. Set the actual
device IP/address in `KNULLI_HOST`; these commands copy only the new test:

```sh
export KNULLI_HOST=root@192.168.1.100
ssh "$KNULLI_HOST" 'mkdir -p /userdata/roms/ports/knulli-gles-test'
scp tools/knulli-gles-test/build-arm64/knulli-gles-test \
  tools/knulli-gles-test/knulli-gles-test.sh \
  "$KNULLI_HOST":/userdata/roms/ports/knulli-gles-test/
ssh "$KNULLI_HOST" 'chmod +x /userdata/roms/ports/knulli-gles-test/knulli-gles-test /userdata/roms/ports/knulli-gles-test/knulli-gles-test.sh'
```

In EmulationStation, update game lists, open **Ports**, open the
`knulli-gles-test` folder if shown, then select **knulli-gles-test** (the `.sh`
entry). Ports menu presentation depends on the installed configuration;
this follows the usual executable `.sh` Ports convention and remains
unverified on this device. No system configuration changes are needed.

Exit by holding **START + SELECT** (SDL's START + BACK buttons) on the same
mapped controller. D-pad directions appear as button events. Stick/trigger
axis logs are throttled to five updates per second per axis and changes of
at least 2048 units, to suppress noise. Controllers are opened on startup
and hotplug. Unmapped joysticks are listed but not opened as controllers;
an SDL mapping may be required for the handheld's input device.

The launcher defaults to `SDL_VIDEODRIVER=mali`, preserves any preexisting
value, starts in its own directory, appends both output streams to
`/userdata/roms/ports/knulli-gles-test/knulli-gles-test.log`, and returns the
application's exit code. Logs persist across launches; periodically remove
or archive this file if needed.

Retrieve diagnostics:

```sh
ssh "$KNULLI_HOST" 'tail -n 100 /userdata/roms/ports/knulli-gles-test/knulli-gles-test.log'
scp "$KNULLI_HOST":/userdata/roms/ports/knulli-gles-test/knulli-gles-test.log ./knulli-gles-test.log
```

## Troubleshooting

- **`mali not available`:** inspect the logged SDL driver list. This custom
  Knulli SDL backend is essential; generic SDL cannot supply it. Check that
  the intended target SDL library was loaded. Do not substitute `kmsdrm`:
  this device has `/dev/fb0` and `/dev/mali0`, but no `/dev/dri`.
- **SDL/EGL window or context error:** the exact SDL error is logged. Verify
  `/dev/fb0`, `/dev/mali0`, permissions and vendor EGL/GLES/Mali libraries.
  Launch through Ports first; graphics ownership and environment can differ
  when started from an SSH session while EmulationStation is displaying.
- **No such file although the binary exists:** verify AArch64 architecture
  and `/lib/ld-linux-aarch64.so.1` with `file`/`readelf`; this can mean a
  missing ELF interpreter. Check executable permissions and LF launcher
  line endings if the script itself will not start.
- **Missing shared library:** on the device run
  `ldd /userdata/roms/ports/knulli-gles-test/knulli-gles-test`.
  Look for `not found`; compare the binary's `NEEDED` names to the installed
  libraries. Unversioned `/usr/lib/libSDL2.so` or `libGLESv2.so` being present
  does not alone guarantee the required versioned SONAME is installed.
  Rebuild against the correct SDK rather than replacing system libraries.
- **`GLIBC_x.y`, `GLIBCXX_x.y` or `CXXABI_x.y not found`:** compare
  `readelf --version-info` for the executable and target libc/libstdc++
  (copy target libraries to the host if readelf is unavailable on Knulli).
  Rebuild with a matching SDK; the C++ runtime must also be compatible.
  For loader diagnostics, launch over SSH with
  `LD_DEBUG=libs,versions sh /userdata/roms/ports/knulli-gles-test/knulli-gles-test.sh --seconds 6`.
- **No controller mapping:** read the listed device name and mapping status.
  Supply a known correct `SDL_GAMECONTROLLERCONFIG` mapping in the launch
  environment. Do not guess button indices. Escape, SDL quit, and
  `--seconds N` remain available for development.
- **Low FPS or software rendering:** inspect `GL_VENDOR` and `GL_RENDERER`.
  `llvmpipe`/`softpipe` indicate software rendering. A triangle appearing
  alone does not prove hardware acceleration. The logged average includes
  swap and frame pacing; it is not a GPU benchmark.

## Validation record (2026-10-11)

- Native Release build: GCC 15.2, CMake 4.2, SDL2 2.32.10; no compiler warnings.
- Six-second SDL offscreen run: Mesa llvmpipe GLES 3.2; shader compile/link
  success, about 60.19 FPS, clean exit.
- Repeated with `MESA_GLES_VERSION_OVERRIDE=2.0`: GL reported GLES 2.0 and
  GLSL ES 1.0.16; both `#version 100` shaders compiled/linked, rendering had
  no GL errors, about 60.18 FPS, clean exit. This validates the shaders on
  a software ES 2.0 implementation, not the handheld's driver.
- Invalid SDL driver produced a detailed SDL_Init error and exit code 1.
- Launcher shell syntax and temporary stub runs verified default `mali`,
  environment override, own-directory execution, persistent logging and
  preservation of a nonzero application exit status (7).
- Native ELF inspected with `file` and `readelf`: x86-64; direct dependencies
  SDL2, GLESv2 and C/C++ runtime libraries, no direct desktop window libraries.
- Cross configuration without SDK paths fails with a prerequisite diagnostic.
  No ARM64 compiler/SDK found in PATH, /opt or the available home tree;
  ARM64 compilation/ELF inspection remains unverified.
- No handheld access or physical controllers available: Mali acceleration,
  framebuffer fullscreen output, target loader/library ABI, controller
  mapping/exit combination and Ports integration remain unverified.

References: [SDL GL attributes](https://wiki.libsdl.org/SDL2/SDL_GL_SetAttribute),
[Khronos GLSL ES 1.00 specification](https://registry.khronos.org/OpenGL/specs/es/2.0/GLSL_ES_Specification_1.00.pdf).
