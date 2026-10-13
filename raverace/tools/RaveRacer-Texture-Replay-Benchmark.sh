#!/bin/sh
set -eu
work=${RR_TEXTURE_TEST_DIR:-/userdata/system/rave-racer-texture-hitch}
recording=${1:-/userdata/system/rave-racer-replays/race-20261013-122242-25136.rec}
first=${2:-1800};last=${3:-5639};pairs=${4:-5}
for n in "$first" "$last" "$pairs";do case "$n" in ""|*[!0-9]*)exit 2;;esac;done
[ "$first" -gt 1 ] && [ "$last" -ge "$first" ] && [ "$pairs" -ge 5 ] || exit 2
[ -f "$recording" ] || exit 2
baseline=${RR_TEXTURE_BASELINE:-$work/rr_texture_baseline}
candidate=${RR_TEXTURE_CANDIDATE:-$work/rr_texture_candidate}
[ -x "$baseline" ] && [ -x "$candidate" ] || exit 2
out="$work/ab-$(date +%Y%m%d-%H%M%S)-$$";mkdir "$out"
if pidof rr_gles2 rr_replay >/dev/null;then echo 'Game active';exit 1;fi
sha256sum "$baseline" "$candidate" "$recording" /userdata/roms/ports/rave-gles2/rr_gles2 > "$out/manifest.sha256"
restore_es=0;child=
restore(){ status=$?;if [ -n "$child" ];then kill "$child" 2>/dev/null || :;wait "$child" 2>/dev/null || :;fi;if [ "$restore_es" = 1 ];then /etc/init.d/S31emulationstation start > "$out/es-restore.log" 2>&1 || status=1;fi;exit "$status"; }
trap restore EXIT
trap 'exit 130' INT TERM HUP
export SDL_VIDEODRIVER=mali XDG_RUNTIME_DIR=/var/run
if pidof emulationstation >/dev/null;then restore_es=1;/etc/init.d/S31emulationstation stop;fi
n=1
while [ "$n" -le "$pairs" ];do
 case $((n%2)) in 0) variants='candidate baseline';;*) variants='baseline candidate';;esac
 for v in $variants;do
  echo "$(date) pair=$n variant=$v results=$out"
  case "$v" in baseline)binary=$baseline;;candidate)binary=$candidate;;esac
  "$binary" /userdata/roms/ports/rave-gles2/extracted \
   --replay-input "$recording" \
   --replay-checks off --frames "$last" --timing independent --skip-geometry-on-dropped-frames on \
   --texture-budget-mib 64 --vbo-upload frame --audio on --overlay off --profile-from "$first" \
   --thread-profile "$out/$v-$n.presentations.csv" --replay-timing "$out/$v-$n.ticks.csv" \
   > "$out/$v-$n.json" 2> "$out/$v-$n.log" &
  child=$!
  while kill -0 "$child" 2>/dev/null;do
   { echo "sample $(date +%s)";awk '/VmRSS|VmHWM/{print}' /proc/$child/status;awk '/MemAvailable|MemFree/{print}' /proc/meminfo; } >> "$out/$v-$n.memory"
   for f in /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq /sys/class/thermal/thermal_zone*/temp /sys/class/devfreq/gpu/cur_freq;do [ ! -r "$f" ] || { printf "%s " "$f";cat "$f"; };done >> "$out/$v-$n.memory"
   sleep 5
  done
  wait "$child";child=
  cmp "$work/reference-$last.json" "$out/$v-$n.json"
 done
 n=$((n+1))
done
sha256sum -c "$out/manifest.sha256"
echo "Completed: $out"
