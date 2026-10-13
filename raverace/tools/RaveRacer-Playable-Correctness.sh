#!/bin/sh
# Isolated checks. Existing executables, ROMs and saved state are never replaced.
set -eu
app=${RR_APP_DIR:-/userdata/roms/ports/rave-gles2}
eval_dir=${RR_EVALUATION_DIR:-/userdata/system/rave-racer-evaluation-20261013-playable}
cd "$eval_dir"
out="correctness-$(date +%Y%m%d-%H%M%S)-$$"
mkdir "$out"
export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-mali}
export XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/var/run}
export GLES_TEXTURE_TRACE=1
child=
restore(){
 status=$?
 if [ -n "$child" ]; then kill "$child" 2>/dev/null || :; wait "$child" || :; fi
 if ! pidof emulationstation >/dev/null; then /etc/init.d/S31emulationstation start > "$out/es-restore.log" 2>&1 || status=1; fi
 exit "$status"
}
trap restore EXIT
trap 'exit 130' INT TERM HUP
sha256sum "$app/rr_gles2" "$app/rr-thread-final-arm64" > "$out/preserved.sha256"
/etc/init.d/S31emulationstation stop || :
sleep 1
if pidof emulationstation >/dev/null; then echo 'Frontend did not stop'; exit 1; fi
./rr-test-video-layers-arm64 > "$out/video-layers.log" 2>&1
for frame in 600 1700 3600; do
 ./rr_headless "$app/extracted" --frames "$frame" --coin 180 --gas 200 --steer 300:2304 --steer 700:2048 --audio-checksum-from 1 > "$out/$frame-headless.json" 2> "$out/$frame-headless.log"
 for mode in serial snapshot thread drop audio; do
  case "$mode" in
   serial)set -- --presentation-thread off --test-present-every 3;;
   snapshot)set -- --snapshot-reference --test-present-every 3;;
   thread)set -- --test-present-every 3;;
   drop)set --;;
   audio)set -- --test-present-every 3 --audio on;;
  esac
  ./rr-playable-validation-arm64 "$app/extracted" --frames "$frame" --coin 180 --gas 200 --steer 300:2304 --steer 700:2048 \
   --timing independent --skip-geometry-on-dropped-frames on --texture-budget-mib 64 --vbo-upload frame \
   --audio-checksum-from 1 --capture-frame "$frame" --capture "$out/$frame-$mode.ppm" "$@" > "$out/$frame-$mode.json" 2> "$out/$frame-$mode.log" &
  child=$!;wait "$child";child=
  cmp "$out/$frame-headless.json" "$out/$frame-$mode.json"
  grep -E '^\[(AUDIO-CHECK|BOARD-STATE|PRESENTATION-GEOMETRY)\]' "$out/$frame-$mode.log" > "$out/$frame-$mode.digests"
  sed -n 's/^\[GEOMETRY\].* ticks=/ticks=/p' "$out/$frame-$mode.log" >> "$out/$frame-$mode.digests"
  grep '^\[TEXTURE-EVENTS\]' "$out/$frame-$mode.log" > "$out/$frame-$mode.textures"
  if [ "$mode" != serial ]; then
   cmp "$out/$frame-serial.ppm" "$out/$frame-$mode.ppm"
   cmp "$out/$frame-serial.digests" "$out/$frame-$mode.digests"
   if [ "$mode" != drop ]; then cmp "$out/$frame-serial.textures" "$out/$frame-$mode.textures"; fi
  fi
 done
 echo "$frame: pixels, simulation/board/video, ordered geometry/textures, PCM/voices and audio-on equivalence passed"
done
sha256sum -c "$out/preserved.sha256"
echo "Results: $eval_dir/$out"
