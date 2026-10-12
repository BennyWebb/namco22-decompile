#!/bin/sh
# Three prebuilt readers; no ROM extraction/recompile on handheld.
set -u
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit 1
cd "${RR_READER_APP_DIR:-$script_dir/rave-gles2}" || exit 1
out=${RESULTS_DIR:-"$PWD/reader-benchmark-$(date +%Y%m%d-%H%M%S)-$$"}
[ ! -e "$out" ] || exit 1
for variant in original existing split; do [ -x "./rr_gles2-reader-$variant" ] || exit 1;done
mkdir -p "$out" || exit 1
export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-mali}
{ date;uname -a;sha256sum ./rr_gles2-reader-*;echo "SDL_VIDEODRIVER=$SDL_VIDEODRIVER"; } > "$out/manifest.txt"
failed=0;child=
trap 'if [ -n "$child" ];then kill "$child" 2>/dev/null;fi;exit 130' INT TERM HUP
run_case() {
 phase=$1;trial=$2;mode=$3
 name="$workload-$phase-$trial-$mode"
 variant=$mode
 case "$phase" in profile) set -- --sample-simulation "$out/$name-samples.tsv" --profile-stages;; *) set --;;esac
 echo "Running $name: measured 1400..$frames"
 "./rr_gles2-reader-$variant" extracted --frames "$frames" --coin 180 --gas 200 \
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
 # Original reader is a correctness reference, not the headline baseline.
 run_case reference 1 original
 for trial in 1 2 3;do
  case "$trial" in 2) order='split existing';;*) order='existing split';;esac
  for mode in $order;do run_case performance "$trial" "$mode";done
 done
 run_case profile 1 existing
 run_case profile 1 split
 for json in "$out/$workload-"*.json;do cmp "$out/$workload-performance-1-existing.json" "$json" || failed=1;done
done
sha256sum "$out"/*.json > "$out/state-sha256.txt"
echo "Reader A/B logs, samples and state: $out"
exit "$failed"
