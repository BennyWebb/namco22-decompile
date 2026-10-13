#!/bin/sh
set -eu
work=${RR_MULTITEXTURE_TEST_DIR:-/userdata/system/rave-racer-multitexture-resume-20261013}
out=${RESULTS_DIR:-"$work/performance-$(date +%Y%m%d-%H%M%S)-$$"}
[ ! -e "$out" ] || { echo "Results already exist: $out" >&2;exit 2; }
mkdir -p "$out"
restore_es=0
child=
monitor=
restore(){ rc=$?;trap - EXIT;[ -z "$child" ] || { kill "$child" 2>/dev/null || :;wait "$child" 2>/dev/null || :; };[ -z "$monitor" ] || { kill "$monitor" 2>/dev/null || :;wait "$monitor" 2>/dev/null || :; };if [ "$restore_es" = 1 ];then /etc/init.d/S31emulationstation start > "$out/es-restore.log" 2>&1 || rc=1;fi;echo "$rc" > "$out/exit";exit "$rc"; }
trap restore EXIT
trap 'exit 130' INT TERM HUP
if pidof rr_gles2 rr_baseline rr_batched rr_checks rr_gpu >/dev/null;then exit 4;fi
export SDL_VIDEODRIVER=mali XDG_RUNTIME_DIR=/var/run
if pidof emulationstation >/dev/null;then restore_es=1;/etc/init.d/S31emulationstation stop;fi
sha256sum "$work/bin/rr_baseline" "$work/bin/rr_batched" /userdata/roms/ports/rave-gles2/rr_gles2 /userdata/system/rave-racer-backups/20261013-pre-parallel-prep/rr_gles2 > "$out/manifest.sha256"
run_one(){
 name=$1
 variant=$2
 if [ "$variant" = baseline ];then binary="$work/bin/rr_baseline";mode=off;else binary="$work/bin/rr_batched";mode=batch;fi
 "$binary" /userdata/roms/ports/rave-gles2/extracted --replay-input /userdata/system/rave-racer-replays/race-20261013-122242-25136.rec --replay-checks off --frames 5639 --timing independent --skip-geometry-on-dropped-frames on --texture-budget-mib 64 --vbo-upload frame --audio on --overlay off --gles-multitexture "$mode" --profile-from 1800 --thread-profile "$out/$name.presentations.csv" --replay-timing "$out/$name.ticks.csv" > "$out/$name.json" 2> "$out/$name.log" &
 child=$!
 (
  while kill -0 "$child" 2>/dev/null;do
   echo "SAMPLE $(cut -d' ' -f1 /proc/uptime)"
   cat /proc/stat
   for task in /proc/"$child"/task/*;do
    [ -r "$task/stat" ] || continue
    cat "$task/stat" 2>/dev/null || :
    printf '\nTHREAD %s ' "${task##*/}";cat "$task/comm" 2>/dev/null || :
    printf 'WCHAN %s ' "${task##*/}";cat "$task/wchan" 2>/dev/null || :;printf '\n'
   done
   for file in /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq /sys/class/thermal/thermal_zone*/temp /sys/class/devfreq/gpu/cur_freq;do
    [ ! -r "$file" ] || { printf '%s ' "$file";cat "$file"; }
   done
   [ ! -r /proc/"$child"/status ] || awk '/VmRSS:|VmHWM:/{print}' /proc/"$child"/status
   sleep 1
  done
 ) > "$out/$name.monitor" &
 monitor=$!
 wait "$child";child=
 kill "$monitor" 2>/dev/null || :;wait "$monitor" 2>/dev/null || :;monitor=
 cmp /userdata/system/rave-racer-multicore/reference-5639.json "$out/$name.json"
}
for pair in 1 2 3 4 5;do
 if [ $((pair % 2)) -eq 1 ];then variants='baseline candidate';else variants='candidate baseline';fi
 for variant in $variants;do
  echo "$(date) pair=$pair variant=$variant"
  run_one "$variant-$pair" "$variant"
 done
done
sha256sum -c "$out/manifest.sha256"
echo "Completed: $out"
