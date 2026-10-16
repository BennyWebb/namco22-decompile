#!/bin/sh
# P2 isolated fixed-CPU measurements, with unconditional clock restoration.
set -eu
base=/userdata/system/rave-racer-native-g1c-20261016
assets=/userdata/system/rave-racer-native-live-4a-20261015
name=$1;binary=$2;mode=$3;depth=2;valve=0;frequency=1512000;export ENG_P3_PRIORITY=off;texture=$4;gpu=stock;duration=${5:-600}
run=$base/$name;mkdir "$run"
child=;monitor=;profiler=;stopped=0
clocks(){ for c in 0 1 2 3;do p=/sys/devices/system/cpu/cpu$c/cpufreq;cat "$p/scaling_governor" "$p/scaling_min_freq" "$p/scaling_max_freq";done;cat /sys/class/devfreq/gpu/governor /sys/class/devfreq/gpu/min_freq /sys/class/devfreq/gpu/max_freq; }
clocks > "$run/clocks.before"
for key in governor min_freq max_freq;do cat /sys/class/devfreq/gpu/$key > "$run/gpu.$key";done
sha256sum -c "$base/protected.sha256" > "$run/protected.before"
cat /proc/sys/kernel/sched_rt_runtime_us /proc/sys/kernel/sched_rt_period_us > "$run/rt.before"
for c in 0 1 2 3;do p=/sys/devices/system/cpu/cpu$c/cpufreq;cat "$p/scaling_governor" > "$run/cpu$c.governor";cat "$p/scaling_min_freq" > "$run/cpu$c.min";cat "$p/scaling_max_freq" > "$run/cpu$c.max";done
[ "$(cat /sys/class/devfreq/gpu/governor)" = simple_ondemand ]
sha256sum /userdata/roms/ports/rave-gles2/rr_gles2 > "$run/production.before"
sha256sum "$base/$binary" "$base/pack/manifest.json" > "$run/binary.sha256"
cleanup(){
 set +e
 if [ -n "$child" ] && kill -0 "$child" 2>/dev/null;then kill -TERM "$child";wait "$child";fi
 if [ -n "$profiler" ];then kill -INT "$profiler" 2>/dev/null;wait "$profiler" 2>/dev/null;echo $? > "$run/perf-exit.txt";fi
 if [ -n "$monitor" ];then kill -TERM "$monitor" 2>/dev/null;wait "$monitor" 2>/dev/null;fi
 for c in 0 1 2 3;do p=/sys/devices/system/cpu/cpu$c/cpufreq;cat "$run/cpu$c.max" > "$p/scaling_max_freq";cat "$run/cpu$c.min" > "$p/scaling_min_freq";cat "$run/cpu$c.governor" > "$p/scaling_governor";done
 for key in min_freq max_freq governor;do cat "$run/gpu.$key" > /sys/class/devfreq/gpu/$key;done
 sha256sum -c "$base/protected.sha256" > "$run/protected.after"
 clocks > "$run/clocks.after";cat /proc/sys/kernel/sched_rt_runtime_us /proc/sys/kernel/sched_rt_period_us > "$run/rt.after";sha256sum /userdata/roms/ports/rave-gles2/rr_gles2 > "$run/production.after"
 if [ "$stopped" = 1 ];then env -i PATH=/usr/bin:/bin:/usr/sbin:/sbin HOME=/root /etc/init.d/S31emulationstation start > "$run/es-start.txt" 2>&1;fi
 tries=0;until pidof emulationstation > "$run/es-restored.txt";do tries=$((tries+1));[ "$tries" -lt 30 ] || break;sleep 1;done
 restoration_failed=0
 cmp "$run/clocks.before" "$run/clocks.after" || restoration_failed=1
 cmp "$run/rt.before" "$run/rt.after" || restoration_failed=1
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
read cool rest < /proc/uptime;cool=${cool%.*}
while :;do read temp < /sys/class/thermal/thermal_zone0/temp;printf '%s\n' "$temp" >> "$run/cooldown.txt";read elapsed rest < /proc/uptime;elapsed=$((${elapsed%.*}-cool));[ "$temp" -le 55000 ] && break;[ "$elapsed" -ge 180 ] && break;sleep 5;done
printf '%s\n' "$temp" > "$run/temperature.start"
{ cat /proc/meminfo;df -k /tmp /userdata;du -sk /tmp;mount; } > "$run/preflight.txt"
[ "$(awk '/MemAvailable:/{print $2}' /proc/meminfo)" -gt 600000 ] || exit 3
sync
for c in 0 1 2 3;do p=/sys/devices/system/cpu/cpu$c/cpufreq;if grep -qw userspace "$p/scaling_available_governors";then echo userspace > "$p/scaling_governor";echo "$frequency" > "$p/scaling_setspeed";else echo performance > "$p/scaling_governor";echo "$frequency" > "$p/scaling_min_freq";echo "$frequency" > "$p/scaling_max_freq";fi;done

clocks > "$run/clocks.fixed"
export ENG_P1B_VALVE_PERIODS=2
export SDL_VIDEODRIVER=mali XDG_RUNTIME_DIR=/var/run ENG_GPU_HUD=1 ENG_GPU_HUD_GEOMETRY=1 ENG_P1_QUEUE="$depth" ENG_P1B_VALVE="$valve"
unset ENG_G1_STATE_AUDIT ENG_P2_METADATA
# export ENG_P2_METADATA="/userdata/system/rave-racer-native-p2-20261016/mesh-metadata.bin"
export ENG_LIVE_PACK="$base/pack" ENG_LIVE_SHADERS="$assets/shaders" ENG_LIVE_LOOKUP="$base/pack/live.lookup" ENG_P1B_RECEIPTS="$run/command-sha256.csv"
export ENG_G1_REFERENCE_IDENTITY="$base/reference-identity.bin"
unset ENG_P1B_TEST_PRESENT_TICK ENG_P1B_TEST_PRESENT_MS LD_PRELOAD RR_AFFINITY ENG_P1_FENCE ENG_P1_VALIDATION ENG_P1_TEST_FALLBACK ENG_LIVE_CAPTURE ENG_LIVE_COMMAND_DUMP ENG_NATIVE_CAPTURE ENG_GPU_HUD_VERIFY RR_AUDIODUMP RR_DUMP_DIR ENG_P1B_IMAGE_HASH
set -- "$base/$binary" /userdata/roms/ports/rave-gles2/extracted --presentation-thread on --timing independent --audio on --thread-profile "$run/thread.csv" --profile-simulation --simulation-profile-from 1800 --replay-input /userdata/system/rave-racer-replays/race-20261013-122242-25136.rec --input-checkpoints "$run/states.jsonl" --replay-timing "$run/ticks.csv"
export ENG_C1_TEXTURES="$texture" ENG_C1_ATLAS="$base/pack" ENG_C1_SAMPLER=texture
unset ENG_C1_DEBUG ENG_C1_PROVENANCE_DIR ENG_C1_IMAGES ENG_C1_SEMANTIC ENG_C1_PALETTE_PRECISION
case "$binary" in play*) unset ENG_G1_REFERENCE_IDENTITY;;esac
case "$mode" in attract) set -- "$base/$binary" /userdata/roms/ports/rave-gles2/extracted --presentation-thread on --timing independent --audio on --thread-profile "$run/thread.csv" --profile-simulation --simulation-profile-from 1 --frames 1000000;;allocation) if [ "$texture" = index ];then pack="$base/index";else pack="$assets/pack";fi;set -- "$base/$binary" "$pack" "$texture";;smoke-fence) export ENG_P1_FENCE=1;set -- "$@" --frames 1900;;smoke) set -- "$@" --frames 1900;;validation) export ENG_LIVE_CAPTURE=hash-only ENG_P1B_IMAGE_HASH="$run/image-sha256.csv";;provenance) export ENG_LIVE_CAPTURE=hash-only ENG_P1B_IMAGE_HASH="$run/image-sha256.csv" ENG_C1_DEBUG=provenance ENG_C1_PROVENANCE_DIR="$run/images";;fence) export ENG_P1_FENCE=1;;trace) set -- strace -ff -ttt -T -yy -e trace=write,writev,pwrite64,fsync,fdatasync,openat,close,futex -o "$run/syscalls" "$@";;headline|observer|profile) :;;*) exit 2;;esac
case "$mode" in validation|provenance) mkdir "$run/images";export ENG_C1_IMAGES="$run/images" ENG_C1_SEMANTIC="$run/semantic-sha256.csv";;esac
observer=low-impact-memory;case "$mode" in observer|trace) observer=on;;esac
printf 'queue=%s valve=%s mode=%s clocks=fixed-CPU GPU=%s observer=%s cooldown=le55000-or180s tmp_output=NONE\n' "$depth" "$valve" "$mode" "$gpu" "$observer" > "$run/method.txt"
if [ "$mode" = attract ];then set -- timeout -s TERM -k 30 "$duration" "$@";fi
cd "$run"
if [ "${ENG_G1B_STREAM_TRACE:-0}" = 1 ];then "$@" > "$run/stdout.txt" 2> "$run/run.txt" & child=$!;else "$@" > "$run/run.txt" 2>&1 & child=$!;fi
game=$child
if [ "$mode" = trace ] || [ "$mode" = attract ];then until game=$(pgrep -P "$child" | head -n 1) && [ -n "$game" ];do sleep 1;done;renice 19 -p "$child" > "$run/tracer-priority.txt";fi
printf '%s\n' "$game" > "$run/pid.txt"
 echo "$ENG_P3_PRIORITY" > "$run/priority-mode.txt"
if [ "$mode" = smoke ];then
 sleep 3
 for attempt in 1 2 3 4 5 6 7 8 9 10;do
  for t in /proc/$game/task/*;do cat "$t/stat" 2>/dev/null || :;done >> "$run/census.txt"
  grep -q '(rr-prepare)' "$run/census.txt" && grep -q '(rr-present)' "$run/census.txt" && break
  sleep 1
 done
fi
case "$mode" in observer|trace) "$base/memory_sampler" "$game" > "$run/memory.txt" 2>&1 & monitor=$!;;*) "$base/memory_sampler" "$game" > "$run/memory.txt" 2>&1 & monitor=$!;;esac
if [ "$mode" = profile ];then
 prep=;while [ -z "$prep" ]&&kill -0 "$game" 2>/dev/null;do for file in /proc/$game/task/*/comm;do if [ "$(cat "$file" 2>/dev/null)" = rr-prepare ];then prep=${file%/comm};prep=${prep##*/};break;fi;done;[ -n "$prep" ]||sleep 1;done
 [ -n "$prep" ]||exit 4;echo "$prep" > "$run/perf-tid.txt"
 "$base/perf" --buildid-dir "$run/perf-cache" record -B -F 999 -g -t "$prep" -o "$run/perf.data" > "$run/perf.txt" 2>&1 & profiler=$!
fi
set +e;wait "$child";result=$?;set -e;child=
printf '%s\n' "$result" > "$run/exit.txt"
exit "$result"
