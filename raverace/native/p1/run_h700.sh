#!/bin/sh
# Isolated P1 runner: persistent output ONLY, clocks/ES restored on all exits.
set -eu
base=/userdata/system/rave-racer-native-p1-20261015
assets=/userdata/system/rave-racer-native-live-4a-20261015
name=$1;profile=$2;depth=$3;mode=$4
run=$base/$name
mkdir "$run"
gpu=/sys/class/devfreq/gpu
child=;monitor=;watchdog=;stopped=0
{ cat "$gpu/governor" "$gpu/min_freq" "$gpu/max_freq";for c in 0 1 2 3;do cat /sys/devices/system/cpu/cpu$c/cpufreq/scaling_governor;done; } > "$run/clocks.before"
sha256sum /userdata/roms/ports/rave-gles2/rr_gles2 > "$run/production.before"
restore(){
 set +e
 if [ -n "$watchdog" ];then kill "$watchdog" 2>/dev/null;wait "$watchdog" 2>/dev/null;fi
 if [ -n "$child" ] && kill -0 "$child" 2>/dev/null;then kill -TERM "$child";wait "$child";fi
 if [ -n "$monitor" ];then kill -TERM "$monitor" 2>/dev/null;wait "$monitor" 2>/dev/null;fi
 sed -n '2p' "$run/clocks.before" > "$gpu/min_freq";sed -n '3p' "$run/clocks.before" > "$gpu/max_freq";sed -n '1p' "$run/clocks.before" > "$gpu/governor"
 for c in 0 1 2 3;do line=$((c+4));sed -n "${line}p" "$run/clocks.before" > /sys/devices/system/cpu/cpu$c/cpufreq/scaling_governor;done
 { cat "$gpu/governor" "$gpu/min_freq" "$gpu/max_freq";for c in 0 1 2 3;do cat /sys/devices/system/cpu/cpu$c/cpufreq/scaling_governor;done; } > "$run/clocks.after"
 sha256sum /userdata/roms/ports/rave-gles2/rr_gles2 > "$run/production.after"
 if [ "$stopped" = 1 ];then env -i PATH=/usr/bin:/bin:/usr/sbin:/sbin HOME=/root /etc/init.d/S31emulationstation start > "$run/es-start.txt" 2>&1;fi
 tries=0;until pidof emulationstation > "$run/es-restored.txt";do tries=$((tries+1));[ "$tries" -lt 30 ] || break;sleep 1;done
 cmp "$run/clocks.before" "$run/clocks.after";cmp "$run/production.before" "$run/production.after"
}
trap restore EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM
{ cat /proc/meminfo;df -k /tmp /userdata;du -sk /tmp;mount; } > "$run/preflight.txt"
[ "$(awk '/MemAvailable:/{print $2}' /proc/meminfo)" -gt 600000 ] || exit 3
sync
if pidof emulationstation >/dev/null;then stopped=1;/etc/init.d/S31emulationstation stop > "$run/es-stop.txt" 2>&1;fi
case "$profile" in stock) :;;both) echo 696000000 > "$gpu/max_freq";echo 696000000 > "$gpu/min_freq";for c in 0 1 2 3;do echo performance > /sys/devices/system/cpu/cpu$c/cpufreq/scaling_governor;done;;*) exit 2;;esac
export SDL_VIDEODRIVER=mali XDG_RUNTIME_DIR=/var/run ENG_GPU_HUD=1 ENG_GPU_HUD_GEOMETRY=1 ENG_P1_QUEUE="$depth"
export ENG_LIVE_PACK="$assets/pack" ENG_LIVE_SHADERS="$assets/shaders" ENG_LIVE_LOOKUP="$assets/live.lookup"
unset LD_PRELOAD RR_AFFINITY ENG_P1_FENCE ENG_P1_VALIDATION ENG_P1_TEST_FALLBACK ENG_LIVE_CAPTURE ENG_LIVE_COMMAND_DUMP ENG_NATIVE_CAPTURE ENG_GPU_HUD_VERIFY RR_AUDIODUMP RR_DUMP_DIR
set -- /userdata/roms/ports/rave-gles2/extracted --presentation-thread on --timing independent --audio on --profile-memory --thread-profile "$run/thread.csv" --profile-simulation --simulation-profile-from 1800
preload=;hash=;dir=
case "$mode" in
 timed|fence|validation)
 set -- "$@" --replay-input /userdata/system/rave-racer-replays/race-20261013-122242-25136.rec --input-checkpoints "$run/states.jsonl" --replay-timing "$run/ticks.csv"
 case "$mode" in fence) export ENG_P1_FENCE=1;;validation) mkdir "$run/commands" "$run/captures";export ENG_LIVE_COMMAND_DUMP="$run/commands" ENG_LIVE_CAPTURE="$run/captures";preload=/userdata/system/rave-racer-native-stage1b-20261015/command_hash.so;hash="$run/command-sha256.csv";dir="$run/commands";;esac;;
 soak|manual) set -- "$@" --interactive --frames 2000000000 --es-input /userdata/roms/ports/rave-gles2/controller-input.cfg;;*) exit 2;;esac
printf 'profile=%s queue=%s mode=%s output=%s tmp_output=NONE\n' "$profile" "$depth" "$mode" "$run" > "$run/method.txt"
cd "$run"
LD_PRELOAD="$preload" RR_COMMAND_HASH="$hash" RR_COMMAND_DIR="$dir" "$base/rr_p1" "$@" > "$run/run.txt" 2>&1 & child=$!
printf '%s\n' "$child" > "$run/pid.txt"
"$base/memory_sampler" "$child" > "$run/memory.txt" 2>&1 & monitor=$!
if [ "$mode" = soak ] || [ "$mode" = manual ];then duration=1800;[ "$mode" != manual ] || duration=180; (trap 'kill "$sleeper" 2>/dev/null;exit' TERM; sleep "$duration" & sleeper=$!;wait "$sleeper";kill -TERM "$child" 2>/dev/null) & watchdog=$!;fi
set +e;wait "$child";result=$?;set -e;child=
if [ "$mode" = validation ];then rmdir "$run/commands" || result=95;fi
printf '%s\n' "$result" > "$run/exit.txt"
exit "$result"
