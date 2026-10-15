#!/bin/sh
# Diagnostic controls suppress output; never use them as visual/performance acceptance.
set -eu
base=$1
run=$2
mode=${3:-0}
sync=${4:-0}
shaders=${5:-/userdata/system/rave-racer-native-m3-revised-20261015/shaders}
exe=${6:-$base/race-profile}
[ ! -e "$run" ]
! pidof rr_gles2 >/dev/null
printf '%s  %s\n' 6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e /userdata/roms/ports/rave-gles2/rr_gles2 | sha256sum -c -
mkdir -p "$run"
stopped=0
restore(){ if [ "$stopped" = 1 ]; then /etc/init.d/S31emulationstation start > "$run/es-start.txt" 2>&1; fi; }
trap restore EXIT HUP INT TERM
if pidof emulationstation >/dev/null; then /etc/init.d/S31emulationstation stop > "$run/es-stop.txt" 2>&1; stopped=1; fi
cat /proc/meminfo > "$run/memory-before.txt"
if [ "$sync" = 1 ]; then export ENG_NATIVE_STAGE_SYNC=1; else unset ENG_NATIVE_STAGE_SYNC; fi
ENG_NATIVE_PROFILE_MODE="$mode" ENG_NATIVE_ASSET_DIR=/userdata/system/rave-racer-native-m3-20261015/race SDL_VIDEODRIVER=mali XDG_RUNTIME_DIR=/var/run "$exe" /userdata/system/rave-racer-native-m3-revised-20261015/pack "$shaders" 1800 5639 1 - > "$run/run.txt" 2>&1
cat /proc/meminfo > "$run/memory-after.txt"
