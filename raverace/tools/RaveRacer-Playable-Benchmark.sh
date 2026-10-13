#!/bin/sh
# Four conditions, five alternating trials. No installed binary replacement.
set -u
app=${RR_APP_DIR:-/userdata/roms/ports/rave-gles2}
eval_dir=${RR_EVALUATION_DIR:-/userdata/system/rave-racer-evaluation-20261013-playable}
cd "$eval_dir" || exit 1
out="benchmark-$(date +%Y%m%d-%H%M%S)-$$"
mkdir "$out" || exit 1
export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-mali} XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/var/run}
child=;monitor=
restore(){
 status=$?
 if [ -n "$child" ]; then kill "$child" 2>/dev/null; wait "$child"; fi
 if [ -n "$monitor" ]; then kill "$monitor" 2>/dev/null; wait "$monitor"; fi
 if ! pidof emulationstation >/dev/null; then /etc/init.d/S31emulationstation start > "$out/es-restore.log" 2>&1 || status=1; fi
 exit "$status"
}
trap restore EXIT
trap 'exit 130' INT TERM HUP
sha256sum "$app/rr_gles2" "$app/rr-thread-final-arm64" ./rr-playable-release-arm64 > "$out/manifest.sha256" || exit 1
{ date;uname -a;cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor;cat /sys/class/devfreq/gpu/governor; } > "$out/manifest.txt"
/etc/init.d/S31emulationstation stop || :
sleep 1
if pidof emulationstation >/dev/null; then echo 'Frontend did not stop'; exit 1; fi
failed=0
run(){
 name=$1;binary=$2;frames=$3;shift 3
 echo "Running $name"
 "$binary" "$app/extracted" --frames "$frames" --coin 180 --gas 200 --steer 300:2304 --steer 700:2048 \
  --timing independent --skip-geometry-on-dropped-frames on --texture-budget-mib 64 --vbo-upload frame \
  --profile-from 1400 --profile-memory --thread-profile "$out/$name.csv" "$@" > "$out/$name.json" 2> "$out/$name.log" &
 child=$!
 (
  while kill -0 "$child" 2>/dev/null; do
   echo "SAMPLE $(cut -d' ' -f1 /proc/uptime)"
   cat /proc/stat
   for task in /proc/"$child"/task/*; do cat "$task/stat" 2>/dev/null; done
   grep -E 'VmRSS|VmHWM|Threads' /proc/"$child"/status 2>/dev/null
   grep -E 'MemAvailable|MemTotal' /proc/meminfo
   for f in /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq /sys/class/thermal/thermal_zone*/temp /sys/class/thermal/cooling_device*/cur_state /sys/class/devfreq/gpu/cur_freq; do
    [ ! -f "$f" ] || echo "$f $(cat "$f")"
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
 if [ "$frames" = 1700 ] && [ -f "$out/dense-1-previous.json" ]; then cmp "$out/dense-1-previous.json" "$out/$name.json" || failed=1; fi
}
for trial in 1 2 3 4 5; do
 case "$trial" in 2|4)modes='expanded compact playable previous';;*)modes='previous playable compact expanded';;esac
 for mode in $modes; do
  name="dense-$trial-$mode"
  if [ "$mode" = previous ]; then run "$name" "$app/rr-thread-final-arm64" 1700
  else
   case "$mode" in playable)overlay=off;;*)overlay=$mode;;esac
   run "$name" ./rr-playable-release-arm64 1700 --audio on --overlay "$overlay"
  fi
 done
done
# Separate instrumentation and stability passes, excluded from headline medians.
run dense-video-profile ./rr-playable-release-arm64 1700 --audio on --overlay off --profile-video
run extended-playable ./rr-playable-release-arm64 3600 --audio on --overlay off
run stability-playable ./rr-playable-release-arm64 18000 --audio on --overlay compact
sha256sum -c "$out/manifest.sha256" || failed=1
echo "Results: $eval_dir/$out"
exit "$failed"
