#!/bin/sh
# Build identical game objects with three rr_mem implementations.
set -eu
[ "$#" -eq 2 ] || { echo "Usage: $0 NINJA_BUILD_DIR OUTPUT_DIR" >&2;exit 2; }
build_dir=$1;out=$2
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_dir=$(CDPATH= cd -- "$script_dir/../.." && pwd)
grep -q '^CMAKE_GENERATOR:INTERNAL=Ninja$' "$build_dir/CMakeCache.txt" || { echo 'Use a configured Ninja tree' >&2;exit 2; }
mkdir -p "$out"
for name in original existing split;do
 fast=ON;split=OFF
 [ "$name" != original ] || fast=OFF
 [ "$name" != split ] || split=ON
 cmake -S "$repo_dir/raverace" -B "$build_dir" -DRR_FAST_MEMORY_READS="$fast" -DRR_SPLIT_MEMORY_READER="$split" -DRR_MEMORY_READ_COUNTERS=OFF
 cmake --build "$build_dir" --target rr_gles2 -j4
 # Replace atomically so a concurrently running validation keeps its old inode.
 cp "$build_dir/rr_gles2" "$out/.rr_gles2-reader-$name-$$"
 mv "$out/.rr_gles2-reader-$name-$$" "$out/rr_gles2-reader-$name"
done
# Leave evaluation options disabled, and rebuild that default artifact.
cmake -S "$repo_dir/raverace" -B "$build_dir" -DRR_FAST_MEMORY_READS=OFF -DRR_SPLIT_MEMORY_READER=OFF -DRR_MEMORY_READ_COUNTERS=OFF
cmake --build "$build_dir" --target rr_gles2 -j4
