#!/bin/sh
# P2 isolated fixed-CPU measurements, with unconditional clock restoration.
set -eu
base=/userdata/system/rave-racer-native-p2-20261016
assets=/userdata/system/rave-racer-native-live-4a-20261015
name=$1;binary=$2;mode=$3;depth=2;valve=0;frequency=${4:-1512000}
run=$base/$name;mkdir "$run"
child=;monitor=;profiler=;stopped=0
clocks(){ for c in 0 1 2 3;do p=/sys/devices/system/cpu/cpu$c/cpufreq;cat "$p/scaling_governor" "$p/scaling_min_freq" "$p/scaling_max_freq";done;cat /sys/class/devfreq/gpu/governor /sys/class/devfreq/gpu/min_freq /sys/class/devfreq/gpu/max_freq; }
clocks > "$run/clocks.before"
for c in 0 1 2 3;do p=/sys/devices/system/cpu/cpu$c/cpufreq;cat "$p/scaling_governor" > "$run/cpu$c.governor";cat "$p/scaling_min_freq" > "$run/cpu$c.min";cat "$p/scaling_max_freq" > "$run/cpu$c.max";done
[ "$(cat /sys/class/devfreq/gpu/governor)" = simple_ondemand ]
sha256sum /userdata/roms/ports/rave-gles2/rr_gles2 > "$run/production.before"
sha256sum "$base/$binary" "$base/warm-tick1800.live" "$base/mesh-metadata.bin" > "$run/binary.sha256"
cleanup(){
 set +e
 if [ -n "$child" ] && kill -0 "$child" 2>/dev/null;then kill -TERM "$child";wait "$child";fi
 if [ -n "$profiler" ];then kill -INT "$profiler" 2>/dev/null;wait "$profiler" 2>/dev/null;echo $? > "$run/perf-exit.txt";fi
 if [ -n "$monitor" ];then kill -TERM "$monitor" 2>/dev/null;wait "$monitor" 2>/dev/null;fi
 for c in 0 1 2 3;do p=/sys/devices/system/cpu/cpu$c/cpufreq;cat "$run/cpu$c.max" > "$p/scaling_max_freq";cat "$run/cpu$c.min" > "$p/scaling_min_freq";cat "$run/cpu$c.governor" > "$p/scaling_governor";done
 clocks > "$run/clocks.after";sha256sum /userdata/roms/ports/rave-gles2/rr_gles2 > "$run/production.after"
 if [ "$stopped" = 1 ];then env -i PATH=/usr/bin:/bin:/usr/sbin:/sbin HOME=/root /etc/init.d/S31emulationstation start > "$run/es-start.txt" 2>&1;fi
 tries=0;until pidof emulationstation > "$run/es-restored.txt";do tries=$((tries+1));[ "$tries" -lt 30 ] || break;sleep 1;done
 restoration_failed=0
 cmp "$run/clocks.before" "$run/clocks.after" || restoration_failed=1
 cmp "$run/production.before" "$run/production.after" || restoration_failed=1
 [ -s "$run/es-restored.txt" ] || restoration_failed=1
 echo "$restoration_failed" > "$run/restoration-failed.txt"
 [ "$restoration_failed" = 0 ] || exit 92
 cat /sys/class/thermal/thermal_zone0/temp > "$run/temperature.end"
}
trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM
if pidof emulationstation >/dev/null;then stopped=1;/etc/init.d/S31emulationstation stop > "$run/es-stop.txt" 2>&1;fi
while :;do read temp < /sys/class/thermal/thermal_zone0/temp;printf '%s\n' "$temp" >> "$run/cooldown.txt";[ "$temp" -lt 50000 ] && break;sleep 5;done
printf '%s\n' "$temp" > "$run/temperature.start"
{ cat /proc/meminfo;df -k /tmp /userdata;du -sk /tmp;mount; } > "$run/preflight.txt"
[ "$(awk '/MemAvailable:/{print $2}' /proc/meminfo)" -gt 600000 ] || exit 3
sync
for c in 0 1 2 3;do p=/sys/devices/system/cpu/cpu$c/cpufreq;if grep -qw userspace "$p/scaling_available_governors";then echo userspace > "$p/scaling_governor";echo "$frequency" > "$p/scaling_setspeed";else echo performance > "$p/scaling_governor";echo "$frequency" > "$p/scaling_min_freq";echo "$frequency" > "$p/scaling_max_freq";fi;done
clocks > "$run/clocks.fixed"
export ENG_P1B_VALVE_PERIODS=2
export SDL_VIDEODRIVER=mali XDG_RUNTIME_DIR=/var/run ENG_GPU_HUD=1 ENG_GPU_HUD_GEOMETRY=1 ENG_P1_QUEUE="$depth" ENG_P1B_VALVE="$valve"
export ENG_P2_METADATA="$base/mesh-metadata.bin"
export ENG_LIVE_PACK="$assets/pack" ENG_LIVE_SHADERS="$assets/shaders" ENG_LIVE_LOOKUP="$assets/live.lookup" ENG_P1B_WARM_PACKET="$base/warm-tick1800.live" ENG_P1B_RECEIPTS="$run/command-sha256.csv"
unset ENG_P1B_TEST_PRESENT_TICK ENG_P1B_TEST_PRESENT_MS LD_PRELOAD RR_AFFINITY ENG_P1_FENCE ENG_P1_VALIDATION ENG_P1_TEST_FALLBACK ENG_LIVE_CAPTURE ENG_LIVE_COMMAND_DUMP ENG_NATIVE_CAPTURE ENG_GPU_HUD_VERIFY RR_AUDIODUMP RR_DUMP_DIR ENG_P1B_IMAGE_HASH
set -- "$base/$binary" /userdata/roms/ports/rave-gles2/extracted --presentation-thread on --timing independent --audio on --thread-profile "$run/thread.csv" --profile-simulation --simulation-profile-from 1800 --replay-input /userdata/system/rave-racer-replays/race-20261013-122242-25136.rec --input-checkpoints "$run/states.jsonl" --replay-timing "$run/ticks.csv"
case "$mode" in validation) export ENG_LIVE_CAPTURE=hash-only ENG_P1B_IMAGE_HASH="$run/image-sha256.csv";;trace) set -- strace -ff -ttt -T -yy -e trace=write,writev,pwrite64,fsync,fdatasync,openat,close,futex -o "$run/syscalls" "$@";;headline|observer|profile) :;;*) exit 2;;esac
observer=off;case "$mode" in observer|trace) observer=on;;esac
printf 'queue=%s valve=%s mode=%s clocks=fixed-CPU-GPU-stock observer=%s max_start_mC=50000 tmp_output=NONE\n' "$depth" "$valve" "$mode" "$observer" > "$run/method.txt"
cd "$run";"$@" > "$run/run.txt" 2>&1 & child=$!;game=$child
if [ "$mode" = trace ];then until game=$(pgrep -P "$child" | head -n 1) && [ -n "$game" ];do sleep 1;done;renice 19 -p "$child" > "$run/tracer-priority.txt";fi
printf '%s\n' "$game" > "$run/pid.txt"
case "$mode" in observer|trace) "$base/memory_sampler" "$game" > "$run/memory.txt" 2>&1 & monitor=$!;;*) "$base/memory_sampler" "$game" clocks > "$run/memory.txt" 2>&1 & monitor=$!;;esac
if [ "$mode" = profile ];then
 prep=;while [ -z "$prep" ]&&kill -0 "$game" 2>/dev/null;do for file in /proc/$game/task/*/comm;do if [ "$(cat "$file" 2>/dev/null)" = rr-prepare ];then prep=${file%/comm};prep=${prep##*/};break;fi;done;[ -n "$prep" ]||sleep 1;done
 [ -n "$prep" ]||exit 4;echo "$prep" > "$run/perf-tid.txt"
 "$base/perf" --buildid-dir "$run/perf-cache" record -B -F 999 -g -t "$prep" -o "$run/perf.data" > "$run/perf.txt" 2>&1 & profiler=$!
fi
set +e;wait "$child";result=$?;set -e;child=
printf '%s\n' "$result" > "$run/exit.txt"
exit "$result"
