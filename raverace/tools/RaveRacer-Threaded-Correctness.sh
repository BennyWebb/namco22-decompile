#!/bin/sh
# On-device pixel/ordered-record/audio/state checks, on one Mali context at a time.
set -eu
cd "${RR_THREADED_APP_DIR:-/userdata/roms/ports/rave-gles2}"
out=${RESULTS_DIR:-"$PWD/threaded-correctness-$(date +%Y%m%d-%H%M%S)-$$"}
[ ! -e "$out" ]; mkdir -p "$out"
export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-mali} GLES_TEXTURE_TRACE=1
sha256sum rr_gles2 rr-thread-validation-arm64 > "$out/manifest.sha256"
child=
restore() {
 status=$?
 if [ -n "$child" ]; then kill "$child" 2>/dev/null; wait "$child" || :; fi
 if ! pidof emulationstation >/dev/null; then /etc/init.d/S31emulationstation start >/tmp/rr-thread-es.log 2>&1 || status=1; fi
 exit "$status"
}
trap restore EXIT
trap 'exit 130' INT TERM HUP
/etc/init.d/S31emulationstation stop || :
sleep 1
if pidof emulationstation >/dev/null; then /etc/init.d/S31emulationstation stop || :; fi
if pidof emulationstation >/dev/null; then echo 'Frontend did not stop' >&2; exit 1; fi
for frame in 600 1700 3600; do
 ./rr_headless-c352-fast extracted --frames "$frame" --coin 180 --gas 200 --steer 300:2304 --steer 700:2048 > "$out/$frame-headless.json" 2> "$out/$frame-headless.log" &
 child=$!;wait "$child";child=
 for mode in baseline snapshot threaded dropping; do
  case "$mode" in
   baseline)set -- --presentation-thread off --test-present-every 3;;
   snapshot)set -- --snapshot-reference --test-present-every 3;;
   threaded)set -- --test-present-every 3;;
   dropping)set --;;
  esac
  ./rr-thread-validation-arm64 extracted --frames "$frame" --coin 180 --gas 200 --steer 300:2304 --steer 700:2048 \
   --timing independent --skip-geometry-on-dropped-frames on --texture-budget-mib 64 --vbo-upload frame \
   --audio-checksum-from 1 --capture-frame "$frame" --capture "$out/$frame-$mode.ppm" "$@" > "$out/$frame-$mode.json" 2> "$out/$frame-$mode.log" &
  child=$!;wait "$child";child=
  cmp "$out/$frame-headless.json" "$out/$frame-$mode.json"
  grep -E '^\[(AUDIO-CHECK|BOARD-STATE|PRESENTATION-GEOMETRY)\]' "$out/$frame-$mode.log" > "$out/$frame-$mode.digests"
  sed -n 's/^\[GEOMETRY\].* ticks=/ticks=/p' "$out/$frame-$mode.log" >> "$out/$frame-$mode.digests"
  grep '^\[TEXTURE-EVENTS\]' "$out/$frame-$mode.log" > "$out/$frame-$mode.textures"
  if [ "$mode" != baseline ]; then
   cmp "$out/$frame-baseline.ppm" "$out/$frame-$mode.ppm"
   cmp "$out/$frame-baseline.digests" "$out/$frame-$mode.digests"
   if [ "$mode" != dropping ]; then cmp "$out/$frame-baseline.textures" "$out/$frame-$mode.textures"; fi
  fi
 done
 echo "$frame baseline/snapshot/thread/drop: pixels, ordered geometry, PCM/voices, board/video and headless state identical"
done
sha256sum -c "$out/manifest.sha256"
echo "Results: $out"
