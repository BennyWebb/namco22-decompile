#!/bin/sh
# Dense opening only. Four independent compile-time variants; no recompilation.
set -u
cd "${RR_GEOMETRY_APP_DIR:-/userdata/roms/ports/rave-gles2}" || exit 1
for ordered in OFF ON; do
 for indexed in OFF ON; do
  [ -x "./rr_gles2-geometry-ordered-$ordered-index-$indexed" ] || exit 1
 done
done
for indexed in OFF ON; do [ -x "./rr_gles2-geometry-profile-$indexed" ] || exit 1; done
out=${RESULTS_DIR:-"$PWD/geometry-benchmark-$(date +%Y%m%d-%H%M%S)-$$"}
[ ! -e "$out" ] || exit 1
mkdir -p "$out" || exit 1
export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-mali}
sha256sum ./rr_gles2 ./rr_gles2-geometry-* > "$out/manifest.sha256" || exit 1
{ date;uname -a;cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor;cat /sys/class/devfreq/gpu/governor; } > "$out/manifest.txt"
trap '/etc/init.d/S31emulationstation start >/tmp/rr-geometry-benchmark-es.log 2>&1' EXIT
child=
trap 'if [ -n "$child" ]; then kill "$child" 2>/dev/null; wait "$child"; fi; exit 130' INT TERM HUP
/etc/init.d/S31emulationstation stop
failed=0
run_case() {
 name=$1;exe=$2;shift 2
 echo "Running $name"
 "$exe" extracted --frames 1700 --coin 180 --gas 200 --steer 300:2304 --steer 700:2048 \
  --timing independent --skip-geometry-on-dropped-frames on --texture-budget-mib 64 \
  --vbo-upload frame --render-mode textured --profile-from 1400 --simulation-profile-from 1400 \
  --profile-memory "$@" > "$out/$name.json" 2> "$out/$name.log" &
 child=$!;wait "$child";status=$?;child=;echo "$status" > "$out/$name.exit"
 [ "$status" = 0 ] || failed=1
 grep -q '"traps":0,"unmapped":0,"romwrites":0' "$out/$name.json" || failed=1
 grep -q '"dsp_fault":false,"sound_fault":false' "$out/$name.json" || failed=1
 cmp "$out/dense-1-ordered-OFF-index-OFF.json" "$out/$name.json" || failed=1
 echo "$name exit=$status"
}
for trial in 1 2 3 4 5; do
 case "$trial" in 2|4) ordered_modes='ON OFF';index_modes='ON OFF';;*) ordered_modes='OFF ON';index_modes='OFF ON';;esac
 for ordered in $ordered_modes; do
  for indexed in $index_modes; do
   run_case "dense-$trial-ordered-$ordered-index-$indexed" "./rr_gles2-geometry-ordered-$ordered-index-$indexed"
  done
 done
done
# Same rendered ticks for CPU attribution only; forced FPS is not throughput.
for trial in 1 2; do
 case "$trial" in 1) index_modes='OFF ON';;2) index_modes='ON OFF';;esac
 for indexed in $index_modes; do
  name="fixed-$trial-index-$indexed"
  run_case "$name" "./rr_gles2-geometry-profile-$indexed" --test-present-every 3 \
   --profile-stages --audit-csv "$out/$name.csv"
 done
done
sha256sum -c "$out/manifest.sha256" || failed=1
echo "Results: $out"
exit "$failed"
