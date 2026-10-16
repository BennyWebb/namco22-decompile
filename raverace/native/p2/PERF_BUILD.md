# Isolated recorder build and attribution method

Matching Knulli linux-headers source is 4.9.170 at
`/home/bwebb/dev/knulli-scarab/output/h700/build/linux-headers-4.9.170`.
Copy tools, include and arch/arm64/include into `/tmp/rr-p2-perf`; no Buildroot
source/configuration is modified. Cross compiler is the existing h700 host/bin
`aarch64-buildroot-linux-gnu-gcc`. The recorder lives only in the P2 diagnostic
directory, never /usr/bin or another system directory.

Host `sudo -n apt-get install` could not authenticate. Instead `apt-get download`
fetched flex, libfl2, libelf-dev, bison and m4 into /tmp/rr-p2-deps, and `dpkg-deb -x`
extracted a private prefix. PATH/M4/BISON_PKGDATADIR point at that prefix.
libelf-dev is host architecture and cannot serve as an ARM64 target library;
NO_LIBELF=1 is deliberate. Neither target nor host perf here has libelf support.

Build invocation (local diagnostic tree):

```
make -C /tmp/rr-p2-perf/tools/perf -j4 ARCH=arm64 \
 CROSS_COMPILE=/home/bwebb/dev/knulli-scarab/output/h700/host/bin/aarch64-buildroot-linux-gnu- \
 NO_LIBELF=1 NO_LIBUNWIND=1 NO_LIBNUMA=1 NO_LIBPYTHON=1 NO_LIBPERL=1 \
 NO_LIBAUDIT=1 NO_SLANG=1 NO_LIBCRYPTO=1 WERROR=0 \
 EXTRA_CFLAGS='-U_FORTIFY_SOURCE -fcommon'
```

Old kernel tool headers need -fcommon under this compiler. The generated
libtraceevent-dynamic-list contained @GLIBC symbol-version suffixes rejected by
ld; removing those suffixes in that generated /tmp file permits linking. Failed
build attempts and successful build6 logs are retained under evidence/build.
Recorder SHA256: b2fe2902908e09660e5d64fdd2fad96bcdf9490d69ddacc522284ddb332a80b3.

Separate profile binaries add -g -fno-omit-frame-pointer -fno-inline. They are
attribution experiments, never timed performance populations. Recorder targets
rr-prepare TID with `record -B -F 999 -g`; SIGINT termination/status130 is expected.
Both full native and dense windows are selected from the matching preparation
start/end timestamps in thread.csv. No DWARF unwinding.

profile.py decodes raw IP/callchains/maps with host perf and resolves them with
cross-nm only inside exact ELF symbol spans. Each executable SHA must match its
recorded run. Toolchain libc/libm symbol files are permitted only after comparing
every executable section byte/address to the copied device libraries; proof is
retained. Unknown samples remain labelled and counted. Tail calls, omitted driver
frames and no-inline perturbation limit absolute attribution. Shares are samples,
not clock-normalised timing savings. Flame graphs include all retained stacks.
Raw perf.data and perf-script stay in ignored generated storage; public evidence
contains their hashes, top-function tables, sample counts and SVGs.
