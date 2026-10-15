#!/bin/sh
# Bounded interactive diagnostic: native activation1800 is never reached.
set -eu
base=/userdata/system/rave-racer-native-live-4a-20261015
run="$base/controls"
test ! -e "$run"
! pidof rr_native_live >/dev/null
mkdir "$run"
stopped=0
restore(){ if [ "$stopped" = 1 ]; then /etc/init.d/S31emulationstation start > "$run/es-start.txt" 2>&1; fi; }
trap restore EXIT HUP INT TERM
if pidof emulationstation >/dev/null; then stopped=1; /etc/init.d/S31emulationstation stop > "$run/es-stop.txt" 2>&1; fi
cd "$base"
export SDL_VIDEODRIVER=mali XDG_RUNTIME_DIR=/var/run ENG_GPU_HUD=1 ENG_GPU_HUD_GEOMETRY=1
export ENG_LIVE_PACK="$base/pack" ENG_NATIVE_ASSET_DIR="$base/pack" ENG_LIVE_SHADERS="$base/shaders" ENG_LIVE_LOOKUP="$base/live.lookup"
unset ENG_LIVE_CAPTURE ENG_LIVE_COMMAND_DUMP ENG_LIVE_TELEMETRY ENG_LIVE_SIM_TELEMETRY ENG_GPU_HUD_VERIFY
set +e
"$base/rr_native_live" /userdata/roms/ports/rave-gles2/extracted --interactive --frames 1500 --presentation-thread off --timing independent --audio on --es-input "$base/controller-input.cfg" --input-log > "$run/run.txt" 2>&1
result=$?
set -e
printf '%s\n' "$result" > "$run/exit.txt"
exit "$result"
