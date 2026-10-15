#!/bin/sh
# Diagnostic 4A only. No production launcher/settings or gameplay mutations.
set -eu
base=/userdata/system/rave-racer-native-live-4a-20261015
name=$1
exe=$2
audio=$3
capture=$4
pace=$5
run="$base/$name"
test ! -e "$run"
! pidof rr_gles2 >/dev/null
mkdir "$run"
work=/tmp/rr-live-4a-$name
mkdir "$work"
cd "$base"
stopped=0
child=
monitor=
restore(){
 if [ -n "$child" ] && kill -0 "$child" 2>/dev/null; then kill -TERM "$child" 2>/dev/null || :; wait "$child" 2>/dev/null || :; fi
 if [ -n "$monitor" ]; then kill "$monitor" 2>/dev/null || :; wait "$monitor" 2>/dev/null || :; fi
 cp "$work"/* "$run/" 2>/dev/null || :
 if [ "$stopped" = 1 ]; then /etc/init.d/S31emulationstation start > "$run/es-start.txt" 2>&1; fi
}
trap restore EXIT HUP INT TERM
if pidof emulationstation >/dev/null; then stopped=1; /etc/init.d/S31emulationstation stop > "$run/es-stop.txt" 2>&1; fi
cat /proc/meminfo > "$work/memory-before.txt"
dmesg > "$work/dmesg-before.txt"
unset ENG_GPU_HUD_VERIFY ENG_NATIVE_STAGE_SYNC ENG_NATIVE_HUD_FENCES ENG_NATIVE_SEQUENCE ENG_LIVE_COMMAND_DUMP ENG_LIVE_CAPTURE
export SDL_VIDEODRIVER=mali XDG_RUNTIME_DIR=/var/run ENG_GPU_HUD=1 ENG_GPU_HUD_GEOMETRY=1
export ENG_LIVE_PACK="$base/pack" ENG_NATIVE_ASSET_DIR="$base/pack" ENG_LIVE_SHADERS="$base/shaders" ENG_LIVE_LOOKUP="$base/live.lookup"
if [ "$capture" = on ]; then mkdir "$run/captures"; export ENG_LIVE_CAPTURE="$run/captures"; fi
export ENG_LIVE_TELEMETRY="$work/telemetry.csv" ENG_LIVE_SIM_TELEMETRY="$work/simulation.csv"
set -- "$base/race-20261013-122242-25136.rec" --input-checkpoints "$work/states.jsonl" --replay-timing "$work/ticks.csv" --presentation-thread off --audio "$audio" --profile-stages --profile-simulation --simulation-profile-from 1 --profile-video --profile-from 1 --es-input "$base/controller-input.cfg"
if [ "$pace" = unpaced ]; then set -- "$@" --unpaced; fi
"$base/$exe" /userdata/roms/ports/rave-gles2/extracted --replay-input "$@" > "$work/run.txt" 2>&1 &
child=$!
printf '%s\n' "$child" > "$work/pid.txt"
"$base/memory_sampler" "$child" > "$work/memory-samples.txt" 2>&1 &
monitor=$!
set +e
wait "$child"
result=$?
set -e
child=
printf '%s\n' "$result" > "$work/exit.txt"
cat /proc/meminfo > "$work/memory-after.txt"
dmesg > "$work/dmesg-after.txt"
exit "$result"
