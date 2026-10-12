#!/bin/sh
# Optional stage/sampling overhead experiment. Release framebuffer ownership first.
set -u
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit 1
cd "${RR_SIM_APP_DIR:-$script_dir/rave-gles2}" || exit 1
binary=${RR_SIM_BINARY:-./rr_gles2-simulation-profile}
[ -x "$binary" ] || exit 1
out=${RESULTS_DIR:-"$PWD/simulation-profile-$(date +%Y%m%d-%H%M%S)-$$"}
[ ! -e "$out" ] || exit 1
mkdir -p "$out" || exit 1
export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-mali}
{ date; uname -a; sha256sum "$binary"; echo "SDL_VIDEODRIVER=$SDL_VIDEODRIVER"; command -v perf || :; } > "$out/manifest.txt"
failed=0;child=
trap 'if [ -n "$child" ]; then kill "$child" 2>/dev/null; fi; exit 130' INT TERM HUP
for workload in city race; do
 case "$workload" in city) frames=1700;; race) frames=3600;; esac
 for trial in 1 2 3; do
  case "$trial" in 1) order='off stages sampled';; 2) order='sampled stages off';; 3) order='stages off sampled';; esac
  for mode in $order; do
   name="$workload-$trial-$mode"
   case "$mode" in off) set --;; stages) set -- --profile-simulation --profile-stages;; sampled) set -- --sample-simulation "$out/$name-samples.tsv" --profile-stages;; esac
   echo "Running $name: measured 1400..$frames"
   "$binary" extracted --frames "$frames" --coin 180 --gas 200 --steer 300:2304 --steer 700:2048 \
    --timing independent --skip-geometry-on-dropped-frames on --texture-budget-mib 64 \
    --vbo-upload frame --render-mode textured --profile-from 1400 --simulation-profile-from 1400 \
    --profile-memory "$@" > "$out/$name.json" 2> "$out/$name.log" &
   child=$!;wait "$child";status=$?;child=
   echo "$status" > "$out/$name.exit"
   if [ "$status" -ne 0 ] || ! grep -q "\"frames\":$frames," "$out/$name.json" ||
      ! grep -q '"traps":0,"unmapped":0,"romwrites":0' "$out/$name.json" ||
      ! grep -q '"dsp_fault":false,"sound_fault":false' "$out/$name.json"; then failed=1;fi
   echo "$name exit=$status"
  done
 done
 for trial in 1 2 3; do
  for mode in off stages sampled; do cmp "$out/$workload-1-off.json" "$out/$workload-$trial-$mode.json" || failed=1;done
 done
done
sha256sum "$out"/*.json > "$out/state-sha256.txt"
echo "Simulation profiles, samples and state: $out"
exit "$failed"
