#!/bin/sh
# Stock clocks only: never writes governor or frequency sysfs.
set -eu
base=/userdata/system/rave-racer-native-p1b-20261016
assets=/userdata/system/rave-racer-native-live-4a-20261015
name=$1;depth=$2;valve=$3;mode=$4
run=$base/$name;mkdir "$run"
child=;monitor=;stopped=0
clocks(){ cat /sys/class/devfreq/gpu/governor /sys/class/devfreq/gpu/min_freq /sys/class/devfreq/gpu/max_freq;for c in 0 1 2 3;do cat /sys/devices/system/cpu/cpu$c/cpufreq/scaling_governor;done; }
clocks > "$run/clocks.before"
[ "$(sed -n 1p "$run/clocks.before")" = simple_ondemand ]
[ "$(sed -n 2p "$run/clocks.before")" = 0 ]
[ "$(sed -n 3p "$run/clocks.before")" = 0 ]
for c in 4 5 6 7;do [ "$(sed -n "${c}p" "$run/clocks.before")" = schedutil ];done
sha256sum /userdata/roms/ports/rave-gles2/rr_gles2 > "$run/production.before"
sha256sum "$base/rr_p1b" "$base/warm-tick1800.live" > "$run/binary.sha256"
cleanup(){
 set +e
 if [ -n "$child" ] && kill -0 "$child" 2>/dev/null;then kill -TERM "$child";wait "$child";fi
 if [ -n "$monitor" ];then kill -TERM "$monitor" 2>/dev/null;wait "$monitor" 2>/dev/null;fi
 clocks > "$run/clocks.after";sha256sum /userdata/roms/ports/rave-gles2/rr_gles2 > "$run/production.after"
 if [ "$stopped" = 1 ];then env -i PATH=/usr/bin:/bin:/usr/sbin:/sbin HOME=/root /etc/init.d/S31emulationstation start > "$run/es-start.txt" 2>&1;fi
 tries=0;until pidof emulationstation > "$run/es-restored.txt";do tries=$((tries+1));[ "$tries" -lt 30 ] || break;sleep 1;done
 cmp "$run/clocks.before" "$run/clocks.after";cmp "$run/production.before" "$run/production.after"
 cat /sys/class/thermal/thermal_zone0/temp > "$run/temperature.end"
}
trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM
while :;do read temp < /sys/class/thermal/thermal_zone0/temp;printf '%s\n' "$temp" >> "$run/cooldown.txt";[ "$temp" -lt 50000 ] && break;sleep 5;done
printf '%s\n' "$temp" > "$run/temperature.start"
{ cat /proc/meminfo;df -k /tmp /userdata;du -sk /tmp;mount; } > "$run/preflight.txt"
[ "$(awk '/MemAvailable:/{print $2}' /proc/meminfo)" -gt 600000 ] || exit 3
sync
if pidof emulationstation >/dev/null;then stopped=1;/etc/init.d/S31emulationstation stop > "$run/es-stop.txt" 2>&1;fi
export ENG_P1B_VALVE_PERIODS=2
export SDL_VIDEODRIVER=mali XDG_RUNTIME_DIR=/var/run ENG_GPU_HUD=1 ENG_GPU_HUD_GEOMETRY=1 ENG_P1_QUEUE="$depth" ENG_P1B_VALVE="$valve"
export ENG_LIVE_PACK="$assets/pack" ENG_LIVE_SHADERS="$assets/shaders" ENG_LIVE_LOOKUP="$assets/live.lookup" ENG_P1B_WARM_PACKET="$base/warm-tick1800.live" ENG_P1B_RECEIPTS="$run/command-sha256.csv"
unset ENG_P1B_TEST_PRESENT_TICK ENG_P1B_TEST_PRESENT_MS LD_PRELOAD RR_AFFINITY ENG_P1_FENCE ENG_P1_VALIDATION ENG_P1_TEST_FALLBACK ENG_LIVE_CAPTURE ENG_LIVE_COMMAND_DUMP ENG_NATIVE_CAPTURE ENG_GPU_HUD_VERIFY RR_AUDIODUMP RR_DUMP_DIR ENG_P1B_IMAGE_HASH
set -- "$base/rr_p1b" /userdata/roms/ports/rave-gles2/extracted --presentation-thread on --timing independent --audio on --thread-profile "$run/thread.csv" --profile-simulation --simulation-profile-from 1800 --replay-input /userdata/system/rave-racer-replays/race-20261013-122242-25136.rec --input-checkpoints "$run/states.jsonl" --replay-timing "$run/ticks.csv"
case "$mode" in validation) export ENG_LIVE_CAPTURE=hash-only ENG_P1B_IMAGE_HASH="$run/image-sha256.csv";;trace) set -- strace -ff -ttt -T -yy -e trace=write,writev,pwrite64,fsync,fdatasync,openat,close,futex -o "$run/syscalls" "$@";;headline|observer) :;;*) exit 2;;esac
observer=off;case "$mode" in observer|trace) observer=on;;esac
printf 'queue=%s valve=%s mode=%s clocks=stock observer=%s max_start_mC=50000 tmp_output=NONE\n' "$depth" "$valve" "$mode" "$observer" > "$run/method.txt"
cd "$run";"$@" > "$run/run.txt" 2>&1 & child=$!;game=$child
if [ "$mode" = trace ];then until game=$(pgrep -P "$child" -x rr_p1b);do sleep 1;done;renice 19 -p "$child" > "$run/tracer-priority.txt";fi
printf '%s\n' "$game" > "$run/pid.txt"
case "$mode" in observer|trace) "$base/memory_sampler" "$game" > "$run/memory.txt" 2>&1 & monitor=$!;;esac
set +e;wait "$child";result=$?;set -e;child=
printf '%s\n' "$result" > "$run/exit.txt"
exit "$result"
