#!/bin/sh
set -eu
work=/userdata/system/rave-racer-world-atlas-20261014
out=$work/driver-probe-$(date +%Y%m%d-%H%M%S)
mkdir -p "$out"
if pidof rr_gles2 rr_mali_trace mali_texture_probe >/dev/null; then echo 'Diagnostic/game already running'; exit 2; fi
restore=0
if pidof emulationstation >/dev/null || pgrep -f '^/bin/bash /usr/bin/emulationstation-standalone' >/dev/null; then restore=1; fi
trap 'if [ "$restore" = 1 ]; then env -u ENG_WORLD_ARRAY_RETAIN -u ENG_DRIVER_LAST_TICK -u ENG_MALI_CALL_TRACE /etc/init.d/S31emulationstation start > "$out/es-restore.log" 2>&1; fi' EXIT
/etc/init.d/S31emulationstation stop
n=0
while pidof emulationstation >/dev/null || pgrep -f '^/bin/bash /usr/bin/emulationstation-standalone' >/dev/null; do n=$((n+1)); [ "$n" -le 20 ]; sleep 1; /etc/init.d/S31emulationstation stop; done
export SDL_VIDEODRIVER=mali XDG_RUNTIME_DIR=/var/run
for repeat in 1 2 3; do
 timeout 180 "$work/mali_texture_probe" "$work/driver-shapes.txt" > "$out/samples-$repeat.csv" 2> "$out/samples-$repeat.log"
 cat /proc/meminfo > "$out/memory-$repeat.txt"
done
echo "$out"
