#!/bin/sh
# Isolated Stage1b run. Restore original clocks and ES on EVERY exit.
set -eu
base=/userdata/system/rave-racer-native-stage1b-20261015
assets=/userdata/system/rave-racer-native-live-4a-20261015
name=$1;mode=$2;config=$3
affinity=${4:-none};variant=${5:-current}
run=$base/$name;work=/tmp/rr-stage1b-$name
mkdir "$run" "$work"
sync
gpu=/sys/class/devfreq/gpu
oldgpu=$(cat "$gpu/governor");oldmin=$(cat "$gpu/min_freq");oldmax=$(cat "$gpu/max_freq")
for c in 0 1 2 3; do cat /sys/devices/system/cpu/cpu$c/cpufreq/scaling_governor > "$run/cpu$c.original";done
printf '%s\n' "$oldgpu" "$oldmin" "$oldmax" > "$run/gpu.original"
stopped=0;child=;monitor=
restore(){
 set +e
 if [ -n "$child" ] && kill -0 "$child" 2>/dev/null;then kill -TERM "$child";wait "$child";fi
 if [ -n "$monitor" ];then kill "$monitor" 2>/dev/null;wait "$monitor" 2>/dev/null;fi
 echo "$oldmin" > "$gpu/min_freq";echo "$oldmax" > "$gpu/max_freq";echo "$oldgpu" > "$gpu/governor"
 for c in 0 1 2 3;do cat "$run/cpu$c.original" > /sys/devices/system/cpu/cpu$c/cpufreq/scaling_governor;done
 { cat "$gpu/governor" "$gpu/min_freq" "$gpu/max_freq";for c in 0 1 2 3;do cat /sys/devices/system/cpu/cpu$c/cpufreq/scaling_governor;done; } > "$run/restored.txt"
 cp "$work"/* "$run/" 2>/dev/null
 unset ENG_LIVE_PACK ENG_NATIVE_ASSET_DIR ENG_LIVE_SHADERS ENG_LIVE_LOOKUP ENG_LIVE_TELEMETRY ENG_LIVE_SIM_TELEMETRY ENG_STAGE1_CSV ENG_STAGE1_DETAIL
 if [ "$stopped" = 1 ];then /etc/init.d/S31emulationstation start > "$run/es-start.txt" 2>&1;fi
 sha256sum /userdata/roms/ports/rave-gles2/rr_gles2 > "$run/production.after"
}
trap restore EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM
{ cat /proc/meminfo;df -k /tmp;du -sk /tmp; } > "$run/preflight.txt"
available=$(awk '/MemAvailable:/{print $2}' /proc/meminfo)
[ "$available" -gt 600000 ] || { echo 'insufficient preflight memory' > "$run/FAILED";exit 3; }
sha256sum /userdata/roms/ports/rave-gles2/rr_gles2 > "$run/production.before"
if pidof emulationstation >/dev/null;then stopped=1;/etc/init.d/S31emulationstation stop > "$run/es-stop.txt" 2>&1;fi
case "$config" in stock) :;;gpu|both) max=$(tr ' ' '\n' < "$gpu/available_frequencies" | sort -n | tail -1);echo "$max" > "$gpu/max_freq";echo "$max" > "$gpu/min_freq";;cpu) :;;*) exit 2;;esac
case "$config" in cpu|both) for c in 0 1 2 3;do echo performance > /sys/devices/system/cpu/cpu$c/cpufreq/scaling_governor;done;;esac
{ cat "$gpu/governor" "$gpu/min_freq" "$gpu/max_freq";for c in 0 1 2 3;do cat /sys/devices/system/cpu/cpu$c/cpufreq/scaling_governor;done; } > "$run/applied.txt"
export SDL_VIDEODRIVER=mali XDG_RUNTIME_DIR=/var/run ENG_GPU_HUD=1 ENG_GPU_HUD_GEOMETRY=1
unset ENG_GPU_HUD_VERIFY ENG_LIVE_CAPTURE ENG_LIVE_COMMAND_DUMP ENG_NATIVE_STAGE_SYNC ENG_NATIVE_HUD_FENCES RR_AFFINITY
case "$affinity" in
 none) unset LD_PRELOAD;;
 off) export LD_PRELOAD="$base/affinity.so";;
 pinned) export LD_PRELOAD="$base/affinity.so" RR_AFFINITY=main:0,prep:1,present:2;;
 *) exit 2;;esac
case "$variant" in current|O1|O2|O2-nosp) :;;*) exit 2;;esac
export ENG_LIVE_PACK="$assets/pack" ENG_NATIVE_ASSET_DIR="$assets/pack" ENG_LIVE_SHADERS="$assets/shaders" ENG_LIVE_LOOKUP="$assets/live.lookup"
export ENG_LIVE_TELEMETRY="$work/presentation.csv" ENG_LIVE_SIM_TELEMETRY="$work/simulation.csv" ENG_STAGE1_CSV="$work/audit.csv" ENG_STAGE1_DETAIL=0
set -- /userdata/roms/ports/rave-gles2/extracted --replay-input /userdata/system/rave-racer-replays/race-20261013-122242-25136.rec --input-checkpoints "$work/states.jsonl" --replay-timing "$work/ticks.csv" --audio on --profile-from 1
case "$mode" in
 native) exe=/userdata/system/rave-racer-native-stage1-20261015/rr_stage1;if [ "$variant" != current ];then exe="$base/rr_native_$variant";fi;set -- "$@" --presentation-thread off --profile-stages --profile-simulation --simulation-profile-from 1800;;
 production) exe="$assets/rr_production_baseline";unset ENG_LIVE_TELEMETRY;if [ "$variant" = current ];then unset ENG_LIVE_SIM_TELEMETRY;else exe="$base/rr_production_$variant";fi;set -- "$@" --presentation-thread on --timing independent --skip-geometry-on-dropped-frames on --thread-profile "$work/thread.csv" --thread-sync-profile --profile-simulation --simulation-profile-from 1800;;
 *) exit 2;;esac
game_preload=${LD_PRELOAD:-};game_affinity=${RR_AFFINITY:-}
unset LD_PRELOAD RR_AFFINITY
LD_PRELOAD="$game_preload" RR_AFFINITY="$game_affinity" "$exe" "$@" > "$work/run.txt" 2>&1 & child=$!
"$base/clock_sampler" "$child" > "$work/clocks.txt" 2>&1 & monitor=$!
set +e;wait "$child";result=$?;set -e;child=
printf '%s\n' "$result" > "$work/exit.txt"
exit "$result"
