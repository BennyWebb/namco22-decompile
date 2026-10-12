#!/bin/sh
set -eu
cd "${RR_PROJECTION_APP_DIR:-/userdata/roms/ports/rave-gles2}"
out=${RESULTS_DIR:-"$PWD/projection-correctness-$(date +%Y%m%d-%H%M%S)-$$"}
[ ! -e "$out" ];mkdir -p "$out"
export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-mali}
sha256sum rr_gles2 > "$out/working.sha256"
restore_es() {
 status=$?
 # Knulli returns 1 for an already running frontend; preserve test exit status.
 if ! pidof emulationstation >/dev/null; then
  /etc/init.d/S31emulationstation start >/tmp/rr-projection-es.log 2>&1 || status=1
 fi
 exit "$status"
}
trap restore_es EXIT
child=
trap 'if [ -n "$child" ]; then kill "$child" 2>/dev/null; wait "$child"; fi; exit 130' INT TERM HUP
/etc/init.d/S31emulationstation stop || :
# A previous runner may have just launched the frontend asynchronously.
sleep 1
if pidof emulationstation >/dev/null; then /etc/init.d/S31emulationstation stop || :; fi
if pidof emulationstation >/dev/null; then echo 'Frontend did not stop' >&2; exit 1; fi
for frame in 600 1700 3600; do
 ./rr_headless-c352-fast extracted --frames "$frame" --coin 180 --gas 200 --steer 300:2304 --steer 700:2048 > "$out/$frame-headless.json" 2> "$out/$frame-headless.log" &
 child=$!;wait "$child";child=
 for flag in OFF ON; do
  ./rr-projection-$flag-arm64 extracted --frames "$frame" --coin 180 --gas 200 --steer 300:2304 --steer 700:2048 \
   --timing independent --test-present-every 3 --skip-geometry-on-dropped-frames on --texture-budget-mib 64 --vbo-upload frame \
   --audio-checksum-from 1 --capture-frame "$frame" --capture "$out/$frame-$flag.ppm" > "$out/$frame-$flag.json" 2> "$out/$frame-$flag.log" &
  child=$!;wait "$child";child=
  cmp "$out/$frame-headless.json" "$out/$frame-$flag.json"
  grep -E '^\[(AUDIO-CHECK|BOARD-STATE|GEOMETRY)\]' "$out/$frame-$flag.log" > "$out/$frame-$flag.digests"
 done
 cmp "$out/$frame-OFF.ppm" "$out/$frame-ON.ppm"
 cmp "$out/$frame-OFF.digests" "$out/$frame-ON.digests"
 echo "$frame pixels, PCM/voice/board/video digests and headless state identical"
done
sha256sum -c "$out/working.sha256"
echo "Results: $out"
