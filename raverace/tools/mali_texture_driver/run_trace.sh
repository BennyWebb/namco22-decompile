#!/bin/sh
set -eu
work=/userdata/system/rave-racer-world-atlas-20261014
last=${ENG_DRIVER_LAST_TICK:-5639}
out=$work/driver-trace-$(date +%Y%m%d-%H%M%S)
mkdir -p "$out"
if pidof rr_gles2 rr_mali_trace >/dev/null; then echo 'Game running'; exit 2; fi
restore=0
if pidof emulationstation >/dev/null || pgrep -f '^/bin/bash /usr/bin/emulationstation-standalone' >/dev/null; then restore=1; fi
trap 'if [ "$restore" = 1 ]; then env -u ENG_WORLD_ARRAY_RETAIN -u ENG_DRIVER_LAST_TICK -u ENG_MALI_CALL_TRACE /etc/init.d/S31emulationstation start > "$out/es-restore.log" 2>&1; fi' EXIT
/etc/init.d/S31emulationstation stop
n=0
while pidof emulationstation >/dev/null || pgrep -f '^/bin/bash /usr/bin/emulationstation-standalone' >/dev/null; do n=$((n+1)); [ "$n" -le 20 ]; sleep 1; /etc/init.d/S31emulationstation stop; done
export SDL_VIDEODRIVER=mali XDG_RUNTIME_DIR=/var/run
for mode in off group; do
 ENG_MALI_CALL_TRACE="$out/$mode.calls.csv" ENG_TEXTURE_MISS_TRACE="$out/$mode.misses.csv" timeout 150 "$work/rr_mali_trace" /userdata/roms/ports/rave-gles2/extracted --replay-input /userdata/system/rave-racer-replays/race-20261013-122242-25136.rec --replay-checks off --frames "$last" --timing independent --skip-geometry-on-dropped-frames on --texture-budget-mib 64 --vbo-upload frame --audio on --overlay off --gles-world-atlas "$mode" --profile-video --profile-from 1800 --thread-profile "$out/$mode.presentations.csv" --replay-timing "$out/$mode.ticks.csv" > "$out/$mode.json" 2> "$out/$mode.log"
 if [ -f "$work/reference-$last.json" ]; then cmp "$work/reference-$last.json" "$out/$mode.json"; elif [ "$mode" = group ]; then cmp "$out/off.json" "$out/group.json"; fi
done
sha256sum /userdata/roms/ports/rave-gles2/rr_gles2 > "$out/production.sha256"
echo "$out"
