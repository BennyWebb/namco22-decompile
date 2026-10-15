#!/bin/sh
# All outputs isolated. Omission controls and split fences are diagnostic only.
set -eu
base=$1
run=$2
world=$3
control=$4
split=$5
cull=${6:-0}
ubo=${7:-0}
exe=${8:-$base/probe}
capture=${9:-off}
[ ! -e "$run" ]
! pidof rr_gles2 >/dev/null
printf '%s  %s\n' 6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e /userdata/roms/ports/rave-gles2/rr_gles2 | sha256sum -c -
case "$world" in original|opaque) ;; *) exit 2;; esac
case "$control" in 0|1|2|3) ;; *) exit 2;; esac
mkdir -p "$run"
stopped=0
restore(){ if [ "$stopped" = 1 ]; then /etc/init.d/S31emulationstation start > "$run/es-start.txt" 2>&1; fi; }
trap restore EXIT HUP INT TERM
if pidof emulationstation >/dev/null; then /etc/init.d/S31emulationstation stop > "$run/es-stop.txt" 2>&1; stopped=1; fi
unset ENG_NATIVE_STAGE_SYNC ENG_NATIVE_HUD_FENCES ENG_NATIVE_HUD_FONT_FIXTURE ENG_NATIVE_SEQUENCE
case "$split" in none) ;; world) export ENG_NATIVE_STAGE_SYNC=1;; hud) export ENG_NATIVE_STAGE_SYNC=1 ENG_NATIVE_HUD_FENCES=1;; *) exit 2;; esac
cat /proc/meminfo > "$run/memory-before.txt"
out=-
if [ "$capture" = on ]; then out="$run"; fi
ENG_GPU_HUD=1 ENG_GPU_HUD_GEOMETRY=1 ENG_NATIVE_HUD_CULL="$cull" ENG_NATIVE_HUD_UBO="$ubo" ENG_NATIVE_HUD_CONTROL="$control" ENG_NATIVE_ASSET_DIR=/userdata/system/rave-racer-native-m3-20261015/race SDL_VIDEODRIVER=mali XDG_RUNTIME_DIR=/var/run "$exe" /userdata/system/rave-racer-native-m3-revised-20261015/pack "$base/$world" 1800 5639 1 "$out" > "$run/run.txt" 2>&1
cat /proc/meminfo > "$run/memory-after.txt"
