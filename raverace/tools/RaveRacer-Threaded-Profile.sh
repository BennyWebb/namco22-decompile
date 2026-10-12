#!/bin/sh
set -eu
cd /userdata/roms/ports/rave-gles2
out="$PWD/threaded-profile-$(date +%Y%m%d-%H%M%S)-$$";mkdir -p "$out"
export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-mali}
child=
restore() { status=$?; if [ -n "$child" ]; then kill "$child" 2>/dev/null;wait "$child" || :;fi; if ! pidof emulationstation >/dev/null;then /etc/init.d/S31emulationstation start >/tmp/rr-thread-es.log 2>&1 || status=1;fi;exit "$status"; }
trap restore EXIT
trap 'exit 130' INT TERM HUP
/etc/init.d/S31emulationstation stop || :;sleep 1
if pidof emulationstation >/dev/null;then /etc/init.d/S31emulationstation stop || :;fi
if pidof emulationstation >/dev/null;then exit 1;fi
for mode in snapshot baseline-profile threaded-profile; do
 case "$mode" in
  snapshot)exe=./rr-thread-final-arm64;set -- --snapshot-reference;;
  baseline-profile)exe=./rr-thread-reference-arm64;set -- --profile-simulation;;
  threaded-profile)exe=./rr-thread-final-arm64;set -- --profile-simulation --thread-sync-profile --thread-profile "$out/$mode.csv";;
 esac
 "$exe" extracted --frames 1700 --coin 180 --gas 200 --steer 300:2304 --steer 700:2048 \
  --timing independent --skip-geometry-on-dropped-frames on --texture-budget-mib 64 --vbo-upload frame \
  --profile-from 1400 --simulation-profile-from 1400 --profile-memory "$@" > "$out/$mode.json" 2> "$out/$mode.log" &
 child=$!;wait "$child";child=
 if [ "$mode" != snapshot ];then cmp "$out/snapshot.json" "$out/$mode.json";fi
done
echo "Results: $out"
