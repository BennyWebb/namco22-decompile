#!/bin/sh
# GPU HUD A/B against the deployed multicore baseline; diagnostics stay outside Ports.
set -eu
work=${RR_MULTICORE_TEST_DIR:-/userdata/system/rave-racer-world-atlas-20261014}
recording=${1:-/userdata/system/rave-racer-replays/race-20261013-122242-25136.rec}
first=${2:-1800}
last=${3:-5639}
pairs=${4:-5}
for number in "$first" "$last" "$pairs"; do
  case "$number" in ''|*[!0-9]*) echo 'Expected integer frame bounds and pair count' >&2; exit 2;; esac
done
[ "$first" -gt 1 ] && [ "$last" -ge "$first" ] && [ "$pairs" -ge 1 ] || exit 2
baseline=${RR_MULTICORE_BASELINE:-/userdata/roms/ports/rave-gles2/rr_gles2}
candidate=${RR_MULTICORE_CANDIDATE:-$work/rr_mali_retain}
original=$work/rr_world_array
export ENG_WORLD_ARRAY_RETAIN=1
[ -x "$baseline" ] && [ -x "$candidate" ] && [ -f "$recording" ] || exit 2
[ -f "$work/reference-$last.json" ] || { echo 'Missing deterministic endpoint reference' >&2; exit 2; }
if pidof rr_gles2 rr_predecode rr_checks rr_cpu_final rr_gpu_preload rr_oriented32 rr_hud rr_hud_geometry rr_hud_geometry_capture rr_hud_final hud_probe hud_probe_geometry >/dev/null; then echo 'Rave Racer is running' >&2; exit 1; fi
out="$work/driver-threeway-$(date +%Y%m%d-%H%M%S)-$$"
mkdir -p "$out"
sha256sum "$baseline" "$candidate" "$original" "$recording" \
  /userdata/roms/ports/rave-gles2/rr_gles2 > "$out/manifest.sha256"
sha256sum /userdata/system/rave-racer-backups/20261013-pre-parallel-prep/rr_gles2 >> "$out/manifest.sha256"
child=
monitor=
restore_es=0
restore() {
  status=$?
  if [ -n "$child" ]; then kill "$child" 2>/dev/null || :; wait "$child" 2>/dev/null || :; fi
  if [ -n "$monitor" ]; then kill "$monitor" 2>/dev/null || :; wait "$monitor" 2>/dev/null || :; fi
  if [ "$restore_es" = 1 ]; then
    env -u ENG_WORLD_ARRAY_RETAIN -u ENG_DRIVER_LAST_TICK -u ENG_MALI_CALL_TRACE /etc/init.d/S31emulationstation start > "$out/es-restore.log" 2>&1 || status=1
  fi
  echo "$status" > "$out/exit"
  exit "$status"
}
trap restore EXIT
trap 'exit 130' INT TERM HUP
export SDL_VIDEODRIVER=mali XDG_RUNTIME_DIR=/var/run
# start is asynchronous: also detect its wrapper, then stop unconditionally.
if pidof emulationstation >/dev/null || pgrep -f '^/bin/bash /usr/bin/emulationstation-standalone' >/dev/null; then restore_es=1; fi
/etc/init.d/S31emulationstation stop
count=0
while pidof emulationstation >/dev/null || pgrep -f '^/bin/bash /usr/bin/emulationstation-standalone' >/dev/null; do
  count=$((count+1)); [ "$count" -le 20 ] || { echo 'Frontend did not stop'; exit 1; }
  sleep 1
  /etc/init.d/S31emulationstation stop
done
run_one() {
  name=$1
  binary=$2
  unset ENG_TEXTURE_PREDECODE_MANIFEST ENG_TEXTURE_PREDECODE_ORIENTED ENG_TEXTURE_PREDECODE_VERIFY ENG_TEXTURE_GPU_PREWARM ENG_TEXTURE_GPU_RECORD
  unset ENG_GPU_HUD_VERIFY ENG_GPU_HUD_CAPTURE ENG_GPU_HUD_CAPTURE_TICKS ENG_HUD_GPU_TRACE
  unset GLES_WORLD_VALIDATE GLES_WORLD_CAPTURE_TICK
  case "$name" in candidate-*|original-*) world_args="--gles-world-atlas group";; *) world_args="";; esac
  cut -d' ' -f1 /proc/uptime > "$out/$name.launch"
  ENG_TEXTURE_MISS_TRACE="$out/$name.misses.csv" "$binary" /userdata/roms/ports/rave-gles2/extracted \
    --replay-input "$recording" --replay-checks off --frames "$last" \
    --timing independent --skip-geometry-on-dropped-frames on \
    --texture-budget-mib 64 --vbo-upload frame --audio on --overlay off $world_args \
    --profile-video --profile-from "$first" --thread-profile "$out/$name.presentations.csv" \
    --replay-timing "$out/$name.ticks.csv" \
    > "$out/$name.json" 2> "$out/$name.log" &
  child=$!
  (
    while kill -0 "$child" 2>/dev/null; do
      if pidof emulationstation >/dev/null || pgrep -f '^/bin/bash /usr/bin/emulationstation-standalone' >/dev/null; then
        echo 'INVALID_COMPETING_FRONTEND'
        kill "$child" 2>/dev/null || :
        exit 1
      fi
      echo "SAMPLE $(cut -d' ' -f1 /proc/uptime)"
      cat /proc/stat
      for task in /proc/"$child"/task/*; do
        [ -r "$task/stat" ] || continue
        cat "$task/stat" 2>/dev/null || :
        printf '\nTHREAD %s ' "${task##*/}"
        cat "$task/comm" 2>/dev/null || :
        printf 'WCHAN %s ' "${task##*/}"
        cat "$task/wchan" 2>/dev/null || :
        printf '\n'
      done
      for file in /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq \
                  /sys/class/thermal/thermal_zone*/temp /sys/class/devfreq/gpu/cur_freq; do
        [ -r "$file" ] && { printf '%s ' "$file"; cat "$file"; }
      done
      [ ! -r /proc/"$child"/status ] || awk '/VmRSS:|VmHWM:/{print}' /proc/"$child"/status 2>/dev/null || :
      awk '/MemAvailable:|MemFree:|Shmem:|CmaFree:/{print}' /proc/meminfo
      sleep 1
    done
  ) > "$out/$name.monitor" &
  monitor=$!
  wait "$child"
  echo 0 > "$out/$name.exit"
  child=
  kill "$monitor" 2>/dev/null || :
  wait "$monitor" 2>/dev/null || :
  monitor=
  cmp "$work/reference-$last.json" "$out/$name.json"
}
pair=${RR_FIRST_PAIR:-1}
while [ "$pair" -le "$pairs" ]; do
  if [ $((pair % 2)) -eq 1 ]; then variants='baseline original candidate'; else variants='candidate original baseline'; fi
  for variant in $variants; do
    echo "$(date) pair=$pair variant=$variant output=$out"
    case "$variant" in
      baseline) run_one "$variant-$pair" "$baseline" ;;
      original) run_one "$variant-$pair" "$original" ;;
      candidate) run_one "$variant-$pair" "$candidate" ;;
    esac
  done
  pair=$((pair + 1))
done
sha256sum -c "$out/manifest.sha256"
echo "Completed: $out"
