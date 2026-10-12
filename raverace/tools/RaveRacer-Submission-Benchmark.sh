#!/bin/sh
# Separate experimental binaries; leave rr_gles2 and user launcher untouched.
set -u
cd "${RR_SUBMISSION_APP_DIR:-/userdata/roms/ports/rave-gles2}" || exit 1
for flag in OFF ON; do
 for kind in '' '-audit'; do [ -x "./rr-submit${kind}-$flag-arm64" ] || exit 1; done
done
out=${RESULTS_DIR:-"$PWD/submission-benchmark-$(date +%Y%m%d-%H%M%S)-$$"}
[ ! -e "$out" ] || exit 1
mkdir -p "$out" || exit 1
export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-mali}
sha256sum ./rr_gles2 ./rr-submit*-arm64 > "$out/manifest.sha256" || exit 1
{ date; uname -a; cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor; cat /sys/class/devfreq/gpu/governor; } > "$out/manifest.txt"
restore_es() {
 status=$?
 # Knulli returns 1 for an already running frontend; preserve test exit status.
 if ! pidof emulationstation >/dev/null; then
  /etc/init.d/S31emulationstation start >/tmp/rr-submit-es.log 2>&1 || status=1
 fi
 exit "$status"
}
trap restore_es EXIT
child=
trap 'if [ -n "$child" ]; then kill "$child" 2>/dev/null; wait "$child"; fi; exit 130' INT TERM HUP
/etc/init.d/S31emulationstation stop || :
# A previous runner may have just launched the frontend asynchronously.
sleep 1
if pidof emulationstation >/dev/null; then /etc/init.d/S31emulationstation stop || :; fi
if pidof emulationstation >/dev/null; then echo 'Frontend did not stop' >&2; exit 1; fi
failed=0
run_case() {
 name=$1;exe=$2;shift 2
 echo "Running $name"
 "$exe" extracted --frames 1700 --coin 180 --gas 200 --steer 300:2304 --steer 700:2048 \
  --timing independent --skip-geometry-on-dropped-frames on --texture-budget-mib 64 \
  --vbo-upload frame --render-mode textured --profile-from 1400 --simulation-profile-from 1400 \
  --profile-memory "$@" > "$out/$name.json" 2> "$out/$name.log" &
 child=$!;wait "$child";status=$?;child=;echo "$status" > "$out/$name.exit"
 [ "$status" = 0 ] || failed=1
 grep -q '"traps":0,"unmapped":0,"romwrites":0' "$out/$name.json" || failed=1
 grep -q '"dsp_fault":false,"sound_fault":false' "$out/$name.json" || failed=1
 cmp "$out/dense-1-OFF.json" "$out/$name.json" || failed=1
}
for trial in 1 2 3 4 5; do
 case "$trial" in 2|4) modes='ON OFF';;*) modes='OFF ON';;esac
 for flag in $modes; do run_case "dense-$trial-$flag" "./rr-submit-$flag-arm64"; done
done
for trial in 1 2; do
 case "$trial" in 1) modes='OFF ON';;2) modes='ON OFF';;esac
 for flag in $modes; do
  name="fixed-$trial-$flag"
  run_case "$name" "./rr-submit-audit-$flag-arm64" --test-present-every 3 --profile-stages --audit-csv "$out/$name.csv"
 done
done
sha256sum -c "$out/manifest.sha256" || failed=1
echo "Results: $out"
exit "$failed"
