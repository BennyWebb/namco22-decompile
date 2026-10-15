#!/bin/sh
# M3 isolated bandwidth tests. All options explicit; no LUT alternatives.
set -eu
base=$1
name=$2
front=$3
rg=$4
dead=$5
split=$6
capture=${7:-off}
export ENG_NATIVE_PEN_RG="$rg" ENG_NATIVE_DISCARD_DEPTH="$dead"
unset ENG_NATIVE_HUD_CONTROL ENG_NATIVE_HUD_UBO
exec sh "$base/run_depth_lut_h700.sh" "$base" "$base/$name" "$front" 0 ram "$split" "$capture" "$base/probe"
