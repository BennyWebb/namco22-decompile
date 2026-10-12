#!/bin/sh
# Configure the chosen native/Buildroot Ninja tree first (see documentation).
set -eu
[ "$#" -eq 2 ] || { echo "Usage: $0 NINJA_BUILD_DIR OUTPUT_DIR" >&2;exit 2; }
build_dir=$1;out=$2
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$script_dir/../.." && pwd)
grep -q '^CMAKE_GENERATOR:INTERNAL=Ninja$' "$build_dir/CMakeCache.txt" || { echo 'Use a configured Ninja tree to reuse unchanged generated-game objects' >&2;exit 2; }
mkdir -p "$out"
for name in baseline fast baseline-counts fast-counts; do
 case "$name" in baseline*) fast=OFF;; *) fast=ON;; esac
 case "$name" in *-counts) counters=ON;; *) counters=OFF;; esac
 cmake -S "$repo_dir/raverace" -B "$build_dir" -DRR_FAST_MEMORY_READS="$fast" -DRR_MEMORY_READ_COUNTERS="$counters"
 cmake --build "$build_dir" --target rr_gles2 -j4
 cp "$build_dir/rr_gles2" "$out/rr_gles2-$name"
done
# Keep evaluation disabled in the reusable tree as well as in source defaults.
cmake -S "$repo_dir/raverace" -B "$build_dir" -DRR_FAST_MEMORY_READS=OFF -DRR_MEMORY_READ_COUNTERS=OFF
cmake --build "$build_dir" --target rr_gles2 -j4
