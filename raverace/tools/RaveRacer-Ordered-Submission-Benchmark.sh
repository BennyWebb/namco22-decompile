#!/bin/sh
# Isolated experimental executables; dense opening is the primary score.
set -u
cd /userdata/roms/ports/rave-gles2 || exit 1
for binary in rr_gles2 rr_gles2-ordered-baseline rr_gles2-ordered-optimized rr_gles2-ordered-profile-baseline rr_gles2-ordered-profile-optimized; do
 [ -x "$binary" ] || exit 1
done
out="$PWD/ordered-benchmark-$(date +%Y%m%d-%H%M%S)-$$"
mkdir -p "$out" || exit 1
export SDL_VIDEODRIVER=mali
sha256sum ./rr_gles2 ./rr_gles2-ordered-* > "$out/manifest.sha256"
trap '/etc/init.d/S31emulationstation start >/tmp/rr-ordered-es-restart.log 2>&1' EXIT
child=
trap 'if [ -n "$child" ]; then kill "$child" 2>/dev/null; wait "$child"; fi; exit 130' INT TERM HUP
/etc/init.d/S31emulationstation stop
failed=0
for trial in 1 2 3 4 5; do
 case "$trial" in 2|4) order='optimized baseline';;*) order='baseline optimized';;esac
 for mode in $order; do
  name="dense-$trial-$mode"
  "./rr_gles2-ordered-$mode" extracted --frames 1700 --coin 180 --gas 200 --steer 300:2304 --steer 700:2048 --timing independent --skip-geometry-on-dropped-frames on --texture-budget-mib 64 --vbo-upload frame --render-mode textured --profile-from 1400 --simulation-profile-from 1400 --profile-memory > "$out/$name.json" 2> "$out/$name.log" &
  child=$!;wait "$child";status=$?;child=;echo "$status" > "$out/$name.exit"
  [ "$status" = 0 ] || failed=1
  grep -q '"traps":0,"unmapped":0,"romwrites":0' "$out/$name.json" || failed=1
  grep -q '"dsp_fault":false,"sound_fault":false' "$out/$name.json" || failed=1
  cmp "$out/dense-1-baseline.json" "$out/$name.json" || failed=1
  echo "$name exit=$status"
 done
done
for mode in baseline optimized; do
 "./rr_gles2-ordered-profile-$mode" extracted --frames 1700 --coin 180 --gas 200 --steer 300:2304 --steer 700:2048 --timing independent --skip-geometry-on-dropped-frames on --texture-budget-mib 64 --vbo-upload frame --render-mode textured --profile-from 1400 --simulation-profile-from 1400 --profile-memory --profile-stages --audit-csv "$out/dense-profile-$mode.csv" > "$out/dense-profile-$mode.json" 2> "$out/dense-profile-$mode.log" &
 child=$!;wait "$child";status=$?;child=;echo "$status" > "$out/dense-profile-$mode.exit";[ "$status" = 0 ] || failed=1
 cmp "$out/dense-1-baseline.json" "$out/dense-profile-$mode.json" || failed=1
done
if [ "${FIXED_ATTRIBUTION:-0}" = 1 ]; then
 # Attribution only: never interpret forced-presentation FPS as throughput.
 for trial in 1 2; do
  case "$trial" in 1) order='baseline optimized';;2) order='optimized baseline';;esac
  for mode in $order; do
   name="fixed-$trial-$mode"
   "./rr_gles2-ordered-profile-$mode" extracted --frames 1700 --coin 180 --gas 200 --steer 300:2304 --steer 700:2048 --timing independent --test-present-every 3 --skip-geometry-on-dropped-frames on --texture-budget-mib 64 --vbo-upload frame --render-mode textured --profile-from 1400 --simulation-profile-from 1400 --profile-memory --profile-stages --audit-csv "$out/$name.csv" > "$out/$name.json" 2> "$out/$name.log" &
   child=$!;wait "$child";status=$?;child=;echo "$status" > "$out/$name.exit"
   [ "$status" = 0 ] || failed=1
   cmp "$out/dense-1-baseline.json" "$out/$name.json" || failed=1
   echo "$name exit=$status"
  done
 done
fi
sha256sum -c "$out/manifest.sha256" || failed=1
echo "Results: $out"
exit "$failed"
