#!/bin/sh
# Isolated native playback only. Never alter installed game or launcher.
set -eu
base=$1
run=$2
first=${3:-1800}
last=${4:-5639}
[ -x "$base/rr-m3-race-final" ]
[ ! -e "$run" ]
if pidof rr_gles2 >/dev/null; then echo 'A production game is running; stop test.' >&2; exit 4; fi
printf '%s  %s\n' 6e1cac0099a7fd9b5e7d705a2d3634dd387f7705b7ce36a8aa011742c07b5a1e /userdata/roms/ports/rave-gles2/rr_gles2 | sha256sum -c -
mkdir -p "$run"
stopped=0
restore(){ if [ "$stopped" = 1 ]; then /etc/init.d/S31emulationstation start > "$run/es-start.txt" 2>&1; fi; }
trap restore EXIT HUP INT TERM
if pidof emulationstation >/dev/null; then /etc/init.d/S31emulationstation stop > "$run/es-stop.txt" 2>&1; stopped=1; fi
cat /proc/meminfo > "$run/memory-before.txt"
SDL_VIDEODRIVER=mali XDG_RUNTIME_DIR=/var/run "$base/rr-m3-race-final" "$base/race" "$base/shaders" "$first" "$last" 1 - > "$run/run.txt" 2>&1
cat /proc/meminfo > "$run/memory-after.txt"
