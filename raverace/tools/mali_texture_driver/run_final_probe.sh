#!/bin/sh
set -eu
work=/userdata/system/rave-racer-world-atlas-20261014
out=$work/driver-final-probe-$(date +%Y%m%d-%H%M%S)
mkdir -p "$out"
if pidof rr_gles2 rr_world_array rr_mali_trace rr_mali_trace_sync rr_mali_trace_low rr_mali_retain rr_mali_retain_idlefirst mali_texture_probe_final >/dev/null; then echo 'Game or diagnostic running'; exit 2; fi
restore=0
if pidof emulationstation >/dev/null || pgrep -f '^/bin/bash /usr/bin/emulationstation-standalone' >/dev/null; then restore=1; fi
trap 'if [ "$restore" = 1 ]; then env -u ENG_WORLD_ARRAY_RETAIN -u ENG_DRIVER_LAST_TICK -u ENG_MALI_CALL_TRACE /etc/init.d/S31emulationstation start > "$out/es-restore.log" 2>&1; fi' EXIT
/etc/init.d/S31emulationstation stop
n=0
while pidof emulationstation >/dev/null || pgrep -f '^/bin/bash /usr/bin/emulationstation-standalone' >/dev/null; do n=$((n+1)); [ "$n" -le 20 ]; sleep 1; /etc/init.d/S31emulationstation stop; done
export SDL_VIDEODRIVER=mali XDG_RUNTIME_DIR=/var/run
for repeat in 1 2 3; do
 for kind in synthetic actual pressure64 pressure112 burst; do
  mkdir -p "$out/$kind"
  unset ENG_PROBE_PAYLOAD_DIR ENG_PROBE_PRESSURE_MIB ENG_BURST_PAYLOAD_DIR
  case "$kind" in
   actual) export ENG_PROBE_PAYLOAD_DIR="$work/driver-representative-payloads";;
   pressure64) export ENG_PROBE_PAYLOAD_DIR="$work/driver-representative-payloads" ENG_PROBE_PRESSURE_MIB=64;;
   pressure112) export ENG_PROBE_PAYLOAD_DIR="$work/driver-representative-payloads" ENG_PROBE_PRESSURE_MIB=112;;
   burst) export ENG_BURST_PAYLOAD_DIR="$work/driver-actual-burst";;
  esac
  binary=$work/mali_texture_probe_final
  [ "$kind" != burst ] || binary=$work/mali_burst_probe_final
  timeout -k 5 180 "$binary" "$work/driver-shapes-final.txt" > "$out/$kind/samples-$repeat.csv" 2> "$out/$kind/samples-$repeat.log"
  cat /proc/meminfo > "$out/$kind/memory-$repeat.txt"
 done
done
sha256sum /userdata/roms/ports/rave-gles2/rr_gles2 > "$out/production.sha256"
echo "$out"
