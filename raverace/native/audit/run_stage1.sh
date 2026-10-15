#!/bin/sh
# Investigation only. All diagnostics isolated; original scheduling unchanged.
set -eu
base=/userdata/system/rave-racer-native-stage1-20261015
assets=/userdata/system/rave-racer-native-live-4a-20261015
name=$1
mode=$2
run="$base/$name"
work=/tmp/rr-stage1-$name
test ! -e "$run"
test ! -e "$work"
! pidof rr_stage1 >/dev/null
mkdir "$run" "$work"
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
cd "$base"
export SDL_VIDEODRIVER=mali XDG_RUNTIME_DIR=/var/run ENG_GPU_HUD=1 ENG_GPU_HUD_GEOMETRY=1
unset ENG_GPU_HUD_VERIFY ENG_LIVE_CAPTURE ENG_LIVE_COMMAND_DUMP ENG_NATIVE_STAGE_SYNC ENG_NATIVE_HUD_FENCES
export ENG_LIVE_PACK="$assets/pack" ENG_NATIVE_ASSET_DIR="$assets/pack" ENG_LIVE_SHADERS="$assets/shaders" ENG_LIVE_LOOKUP="$assets/live.lookup"
export ENG_LIVE_TELEMETRY="$work/presentation.csv" ENG_LIVE_SIM_TELEMETRY="$work/simulation.csv" ENG_STAGE1_CSV="$work/audit.csv"
set -- /userdata/roms/ports/rave-gles2/extracted --replay-input "$assets/race-20261013-122242-25136.rec" --input-checkpoints "$work/states.jsonl" --replay-timing "$work/ticks.csv" --audio on --profile-from 1 --es-input "$assets/controller-input.cfg"
case "$mode" in
 detailed) exe="$base/rr_stage1";export ENG_STAGE1_DETAIL=1;set -- "$@" --presentation-thread off --profile-stages --profile-simulation --simulation-profile-from 1;;
 control) exe="$base/rr_stage1";export ENG_STAGE1_DETAIL=0;set -- "$@" --presentation-thread off --profile-stages --profile-simulation --simulation-profile-from 1;;
 production) exe="$assets/rr_production_baseline";unset ENG_LIVE_TELEMETRY ENG_LIVE_SIM_TELEMETRY;set -- "$@" --presentation-thread on --timing independent --skip-geometry-on-dropped-frames on --thread-profile "$work/thread.csv" --thread-sync-profile --profile-simulation --simulation-profile-from 1;;
 *) exit 2;;
esac
"$exe" "$@" > "$work/run.txt" 2>&1 &
child=$!
"$base/thread_sampler" "$child" > "$work/tasks.txt" 2>&1 &
monitor=$!
printf '%s\n' "$child" > "$work/pid.txt"
set +e
wait "$child"
result=$?
set -e
child=
printf '%s\n' "$result" > "$work/exit.txt"
exit "$result"
