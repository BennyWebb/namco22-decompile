#!/bin/sh
# Experimental binaries only. Never replace rr_gles2 or the normal launcher.
set -u
cd "${RR_THREADED_APP_DIR:-/userdata/roms/ports/rave-gles2}" || exit 1
out=${RESULTS_DIR:-"$PWD/threaded-benchmark-$(date +%Y%m%d-%H%M%S)-$$"}
[ ! -e "$out" ] || exit 1
mkdir -p "$out" || exit 1
baseline=${RR_THREAD_REFERENCE:-./rr-thread-reference-arm64}
experimental=${RR_THREAD_BINARY:-./rr-thread-final-arm64}
export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-mali}
sha256sum ./rr_gles2 "$baseline" "$experimental" > "$out/manifest.sha256" || exit 1
{ date; uname -a; cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor; cat /sys/class/devfreq/gpu/governor; } > "$out/manifest.txt"
child=; monitor=
restore() {
 status=$?
 if [ -n "$child" ]; then kill "$child" 2>/dev/null; wait "$child"; fi
 if [ -n "$monitor" ]; then kill "$monitor" 2>/dev/null; wait "$monitor"; fi
 if ! pidof emulationstation >/dev/null; then /etc/init.d/S31emulationstation start >/tmp/rr-thread-es.log 2>&1 || status=1; fi
 exit "$status"
}
trap restore EXIT
trap 'exit 130' INT TERM HUP
/etc/init.d/S31emulationstation stop || :
sleep 1
if pidof emulationstation >/dev/null; then /etc/init.d/S31emulationstation stop || :; fi
if pidof emulationstation >/dev/null; then echo 'Frontend did not stop' >&2; exit 1; fi
failed=0
run_case() {
 name=$1;exe=$2;frames=$3;shift 3
 echo "Running $name"
 "$exe" extracted --frames "$frames" --coin 180 --gas 200 --steer 300:2304 --steer 700:2048 \
  --timing independent --skip-geometry-on-dropped-frames on --texture-budget-mib 64 \
  --vbo-upload frame --render-mode textured --profile-from 1400 --simulation-profile-from 1400 \
  --profile-memory "$@" > "$out/$name.json" 2> "$out/$name.log" &
 child=$!
 (
  while kill -0 "$child" 2>/dev/null; do
   echo "SAMPLE $(cut -d' ' -f1 /proc/uptime)"
   cat /proc/stat
   for task in /proc/"$child"/task/*; do cat "$task/stat" 2>/dev/null; done
   for file in /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq /sys/class/thermal/thermal_zone*/temp /sys/class/devfreq/gpu/cur_freq; do
    [ -f "$file" ] && echo "$file $(cat "$file")"
   done
   sleep 1
  done
 ) > "$out/$name.monitor" &
 monitor=$!
 wait "$child";status=$?;child=
 kill "$monitor" 2>/dev/null;wait "$monitor" 2>/dev/null;monitor=
 echo "$status" > "$out/$name.exit"
 [ "$status" = 0 ] || failed=1
 grep -q '"traps":0,"unmapped":0,"romwrites":0' "$out/$name.json" || failed=1
 grep -q '"dsp_fault":false,"sound_fault":false' "$out/$name.json" || failed=1
 case "$name" in dense*) ref="$out/dense-1-baseline.json";;*) ref="$out/extended-1-baseline.json";;esac
 [ ! -f "$ref" ] || cmp "$ref" "$out/$name.json" || failed=1
}
for workload in ${RR_THREAD_WORKLOADS:-dense extended}; do
 case "$workload" in dense)frames=1700;;extended)frames=3600;;*)echo "Unknown workload: $workload" >&2;exit 1;;esac
 for trial in 1 2 3 4 5; do
  case "$trial" in 2|4)modes='threaded baseline';;*)modes='baseline threaded';;esac
  for mode in $modes; do
   name="$workload-$trial-$mode"
   if [ "$mode" = threaded ]; then run_case "$name" "$experimental" "$frames" --thread-profile "$out/$name.csv"
   else run_case "$name" "$baseline" "$frames"; fi
  done
 done
done
sha256sum -c "$out/manifest.sha256" || failed=1
echo "Results: $out"
exit "$failed"
