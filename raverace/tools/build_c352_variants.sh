#!/bin/sh
# Existing WRAM/split optimisations ON; vary only C352 mixer and separate counters.
set -eu
[ "$#" -eq 2 ] || { echo "Usage: $0 NINJA_BUILD_DIR OUTPUT_DIR" >&2;exit 2; }
build_dir=$1;out=$2
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$script_dir/../.." && pwd)
grep -q '^CMAKE_GENERATOR:INTERNAL=Ninja$' "$build_dir/CMakeCache.txt" || { echo 'Use a configured Ninja tree' >&2;exit 2; }
mkdir -p "$out"
for name in baseline fast baseline-counts fast-counts;do
 case "$name" in baseline*) fast=OFF;;*) fast=ON;;esac
 case "$name" in *-counts) counters=ON;;*) counters=OFF;;esac
 cmake -S "$repo_dir/raverace" -B "$build_dir" \
  -DRR_FAST_MEMORY_READS=ON -DRR_SPLIT_MEMORY_READER=ON -DRR_MEMORY_READ_COUNTERS=OFF \
  -DRR_C352_INACTIVE_FASTPATH="$fast" -DRR_C352_VOICE_COUNTERS="$counters"
 cmake --build "$build_dir" --target rr_gles2 -j4
 cp "$build_dir/rr_gles2" "$out/.rr_gles2-c352-$name-$$"
 mv "$out/.rr_gles2-c352-$name-$$" "$out/rr_gles2-c352-$name"
done
cmake -S "$repo_dir/raverace" -B "$build_dir" \
 -DRR_FAST_MEMORY_READS=OFF -DRR_SPLIT_MEMORY_READER=OFF -DRR_MEMORY_READ_COUNTERS=OFF \
 -DRR_C352_INACTIVE_FASTPATH=OFF -DRR_C352_VOICE_COUNTERS=OFF
cmake --build "$build_dir" --target rr_gles2 -j4
