#!/bin/sh
# Run after releasing EmulationStation's framebuffer ownership. No captures.
set -u
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit 1
app_dir=${RR_SKIP_APP_DIR:-"$script_dir/rave-gles2"}
cd "$app_dir" || exit 1
binary=${RR_SKIP_BINARY:-./rr_gles2-skip-geometry}
[ -x "$binary" ] || { echo "Missing executable: $binary" >&2; exit 1; }
result_dir=${RESULTS_DIR:-"$app_dir/skipped-geometry-$(date +%Y%m%d-%H%M%S)-$$"}
[ ! -e "$result_dir" ] || { echo 'Results directory already exists' >&2; exit 1; }
mkdir -p "$result_dir" || exit 1
export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-mali}
{ date; uname -a; sha256sum "$binary"; echo "SDL_VIDEODRIVER=$SDL_VIDEODRIVER"; } > "$result_dir/manifest.txt"
failed=0
child=
trap 'if [ -n "$child" ]; then kill "$child" 2>/dev/null; fi; exit 130' INT TERM HUP
for workload in city race; do
    case "$workload" in city) frames=1700;; race) frames=3600;; esac
    for trial in 1 2 3; do
        case "$trial" in 2) order='on off';; *) order='off on';; esac
        for mode in $order; do
            name="$workload-$trial-$mode"
            echo "Running $name: frames 1400..$frames"
            "$binary" extracted --frames "$frames" --coin 180 --gas 200 \
                --steer 300:2304 --steer 700:2048 --timing independent \
                --skip-geometry-on-dropped-frames "$mode" --render-mode textured \
                --vbo-upload frame --texture-budget-mib 64 --profile-from 1400 \
                --profile-stages --profile-memory > "$result_dir/$name.json" 2> "$result_dir/$name.log" &
            child=$!
            wait "$child"; status=$?; child=
            echo "$status" > "$result_dir/$name.exit"
            if [ "$status" -ne 0 ] ||
               ! grep -q "\"frames\":$frames," "$result_dir/$name.json" ||
               ! grep -q '"traps":0,"unmapped":0,"romwrites":0' "$result_dir/$name.json" ||
               ! grep -q '"dsp_fault":false,"sound_fault":false' "$result_dir/$name.json"; then
                failed=1
            fi
            echo "$name exit=$status"
        done
    done
    for trial in 1 2 3; do
        for mode in off on; do
            cmp "$result_dir/$workload-1-off.json" "$result_dir/$workload-$trial-$mode.json" || failed=1
        done
    done
 done
sha256sum "$result_dir"/*.json > "$result_dir/state-sha256.txt"
echo "Logs, stage cohorts, state and memory reports: $result_dir"
exit "$failed"
