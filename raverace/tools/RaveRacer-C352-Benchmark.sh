#!/bin/sh
# Four prebuilt variants; no ROM extraction/recompile on handheld.
set -u
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit 1
cd "${RR_C352_APP_DIR:-$script_dir/rave-gles2}" || exit 1
out=${RESULTS_DIR:-"$PWD/c352-benchmark-$(date +%Y%m%d-%H%M%S)-$$"}
[ ! -e "$out" ] || exit 1
for variant in baseline fast baseline-counts fast-counts; do [ -x "./rr_gles2-c352-$variant" ] || exit 1;done
mkdir -p "$out" || exit 1
export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-mali}
{ date;uname -a;sha256sum ./rr_gles2-c352-*;echo "SDL_VIDEODRIVER=$SDL_VIDEODRIVER"; } > "$out/manifest.txt"
failed=0;child=
trap 'if [ -n "$child" ];then kill "$child" 2>/dev/null;fi;exit 130' INT TERM HUP
run_case() {
 phase=$1;trial=$2;mode=$3
 name="$workload-$phase-$trial-$mode"
 variant=$mode
 if [ "$phase" = activity ];then variant="$mode-counts";fi
 case "$phase" in activity) set -- --c352-activity-from 1400 --audio-checksum-from 1400;; profile) set -- --sample-simulation "$out/$name-samples.tsv" --profile-stages;; *) set --;;esac
 echo "Running $name: measured 1400..$frames"
 "./rr_gles2-c352-$variant" extracted --frames "$frames" --coin 180 --gas 200 \
  --steer 300:2304 --steer 700:2048 --timing independent --skip-geometry-on-dropped-frames on \
  --texture-budget-mib 64 --render-mode textured --vbo-upload frame --profile-from 1400 \
  --simulation-profile-from 1400 --profile-memory "$@" > "$out/$name.json" 2> "$out/$name.log" &
 child=$!;wait "$child";status=$?;child=
 echo "$status" > "$out/$name.exit"
 if [ "$status" -ne 0 ] || ! grep -q "\"frames\":$frames," "$out/$name.json" ||
    ! grep -q '"traps":0,"unmapped":0,"romwrites":0' "$out/$name.json" ||
    ! grep -q '"dsp_fault":false,"sound_fault":false' "$out/$name.json";then failed=1;fi
 echo "$name exit=$status"
}
for workload in city race;do
 case "$workload" in city) frames=1700;;race) frames=3600;;esac
 # Voice activity/audio equivalence are separate from unprofiled performance.
 run_case activity 1 baseline
 run_case activity 1 fast
 # Fail before performance tests if PCM or per-interval voice/clock state differs.
 [ "$failed" -eq 0 ] || exit 1
 cmp "$out/$workload-activity-1-baseline.json" "$out/$workload-activity-1-fast.json" || exit 1
 for mode in baseline fast;do
  grep '^\[AUDIO-CHECK\] ' "$out/$workload-activity-1-$mode.log" > "$out/$workload-$mode-audio-check.txt" || exit 1
 done
 cmp "$out/$workload-baseline-audio-check.txt" "$out/$workload-fast-audio-check.txt" || exit 1
 for trial in 1 2 3;do
  case "$trial" in 2) order='fast baseline';;*) order='baseline fast';;esac
  for mode in $order;do run_case performance "$trial" "$mode";done
 done
 run_case profile 1 baseline
 run_case profile 1 fast
 for json in "$out/$workload-"*.json;do cmp "$out/$workload-performance-1-baseline.json" "$json" || failed=1;done
done
sha256sum "$out"/*.json > "$out/state-sha256.txt"
echo "C352 A/B logs, activity/audio checks, samples and state: $out"
exit "$failed"
