#!/bin/sh
# Isolated M3 only. Component fences/captures never count as acceptance timing.
set -eu
base=$1
run=$2
front=$3
fold=$4
logging=$5
split=$6
capture=${7:-off}
exe=${8:-$base/probe}
[ ! -e "$run" ]
! pidof rr_gles2 >/dev/null
printf '%s  %s\n' 6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e /userdata/roms/ports/rave-gles2/rr_gles2 | sha256sum -c -
mkdir -p "$run"
stopped=0
restore(){ if [ "$stopped" = 1 ]; then /etc/init.d/S31emulationstation start > "$run/es-start.txt" 2>&1; fi; }
trap restore EXIT HUP INT TERM
if pidof emulationstation >/dev/null; then /etc/init.d/S31emulationstation stop > "$run/es-stop.txt" 2>&1; stopped=1; fi
unset ENG_NATIVE_STAGE_SYNC ENG_NATIVE_HUD_FENCES ENG_NATIVE_SEQUENCE ENG_NATIVE_HUD_FONT_FIXTURE ENG_GPU_HUD_VERIFY
case "$split" in none) ;; world) export ENG_NATIVE_STAGE_SYNC=1;; hud) export ENG_NATIVE_STAGE_SYNC=1 ENG_NATIVE_HUD_FENCES=1;; *) exit 2;; esac
case "$logging" in sync|ram) ;; *) exit 2;; esac
cat /proc/meminfo > "$run/memory-before.txt"
out=-
if [ "$capture" = on ]; then out="$run"; fi
ENG_GPU_HUD=1 ENG_GPU_HUD_GEOMETRY=1 ENG_NATIVE_FRONT="$front" ENG_NATIVE_FOLD="$fold" ENG_NATIVE_LOG="$logging" ENG_NATIVE_ASSET_DIR=/userdata/system/rave-racer-native-m3-20261015/race SDL_VIDEODRIVER=mali XDG_RUNTIME_DIR=/var/run "$exe" /userdata/system/rave-racer-native-m3-revised-20261015/pack /userdata/system/rave-racer-native-m3-fragment-hud-20261015/opaque 1800 5639 1 "$out" > "$run/run.txt" 2>&1
cat /proc/meminfo > "$run/memory-after.txt"
