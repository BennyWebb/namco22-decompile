# G1c private derivative

See ../../NATIVE_G1C_REPORT.md and ../../benchmarks/native-g1c-20261016/README.md.
No shared engine or production sources are modified. Depends on the existing
G1b/G1/C1e/P3 private generators and original ROM-only asset cache.

`prepare.py` reuses the unchanged static pack and prepares validation identity maps.
`build.py host|arm [--validation]` builds the repaired renderer. For attribution,
`build.py host --observe-cache` derives the old G1b cache with counters only into
ignored `generated/g1c-observe/`; it does not change the selected repaired build.
The initial historical attribution binary/source are also preserved in
`generated/g1c/observe-build/` with hashes.

`host.py NAME [--play] [--attract --frames N] [--observe-cache] [--keys]
[--stream-trace] [--debug]` runs the isolated host workload. Attract uses separate
stdout/stderr to prevent interleaving; reference captures retain the prior format.
`--observe-cache` selects the separately built attribution executable, not a
runtime switch on the repaired executable. Runtime diagnostics use ENG_* only.
`ENG_G1C_KEYS=1` reports allocation keys for host working-set analysis; it is off
on device. `ENG_G1B_STREAM_TRACE=1` uses existing asynchronous writer cookies.

`cache.inc`: recipe keys remain all seven exact integers. Hash cells contain
16-bit generation / 16-bit pool index. Logical slot count is next power of two
above 2*(static catalogue size*128 palette groups); actual exact-key pool is
bounded at65535, and each lookup probes at most64. Reaching the pool or probe
limit advances generation in O(1). Generation wrap clears cells before reuse.
No dynamic per-picture material ID is cached globally: only static lookup results
(including negative results) are cached. RGBA lookup keeps the old implementation.

`transform.py`: raises dynamic page to3072² and coalesces physical allocations
by rounding source rectangle interior bounds outward to multiples of256. Logical
recipes/materials stay exact; source bank/cmode are part of allocation identity.
Pens still decode from the original ROM at runtime. UV offsets reference the
allocation's source origin; debug mappings reference the logical footprint's
origin. Two source-aware gutters, pinned current-picture allocations, immutable
slot copies and three fenced GPU resource copies remain intact. This reduces
repeated overlapping rectangles without adding any static/replay catalogue entry.

`working_set.py`, `prove_blocks.py`: derive observed logical working sets and
prove every padded logical footprint is the exact corresponding crop of its ROM
allocation. This is profiling/validation input, not an asset-building catalogue.
`test_cache.c`: exact hits, collision/probe limit, pool turnover and generation wrap.
`test_allocator.py`: original ownership/eviction stress with updated capacity bounds.
`validate.py`: reference checkpoints, packets, per-decode ROM parity,36-image
provenance and capture proofs. `verify.py`: hardware/reference restoration checks.
`analyse.py`: reference distributions; `coverage.py`: every picture and fallback.
`cache_analysis.py`: all selectors, service times, minute buckets, cache counters,
repeats, underruns and gaps. `resources.py`: resource samples per selector.
`attract.py`: arbitrary-length run memory/clock and drain checks. `publish.py`
retains lossless telemetry and hashes, excluding ROM packs/binaries/readback pixels.

`run_h700.sh` is an isolated, authorised-use-only runner with saved governors,
protected hashes and guaranteed EXIT cleanup. Attract timeout is a parameter;
930seconds ensures >=15minutes after startup. No timed transfers or archival.
Queue2/default priorities/affinity off, CPU1512MHz, stock GPU; no new launcher.
