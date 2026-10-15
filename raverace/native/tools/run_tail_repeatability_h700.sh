#!/bin/sh
# Fixed six-process sequence; all runs retained. Evaluate gate after this series.
set -eu
base=$1
color=$2
case "$color" in 0|1) ;; *) exit 2;; esac
for pair in 1 2 3; do
 for variant in baseline candidate; do
  test ! -e "$base/repeat-$pair-$variant"
 done
done
unset ENG_NATIVE_HUD_CONTROL ENG_NATIVE_HUD_UBO
for pair in 1 2 3; do
 printf 'RUN repeat-%s-baseline\n' "$pair"
 ENG_NATIVE_PEN_RG=0 ENG_NATIVE_DISCARD_DEPTH=0 ENG_NATIVE_DISCARD_COLOR=0 sh "$base/run_depth_lut_h700.sh" "$base" "$base/repeat-$pair-baseline" 1 0 ram none off "$base/probe-color"
 printf 'RUN repeat-%s-candidate\n' "$pair"
 ENG_NATIVE_PEN_RG=0 ENG_NATIVE_DISCARD_DEPTH=1 ENG_NATIVE_DISCARD_COLOR="$color" sh "$base/run_depth_lut_h700.sh" "$base" "$base/repeat-$pair-candidate" 1 0 ram none off "$base/probe-color"
done
