#!/bin/sh
# Run on Knulli over SSH. Experimental binaries stay outside the active Ports tree.
set -eu
work=${RR_MULTICORE_TEST_DIR:-/userdata/system/rave-racer-multicore}
recording=${1:-/userdata/system/rave-racer-replays/race-20261013-122242-25136.rec}
first=${2:-1800}
last=${3:-5639}
pairs=${4:-5}
for number in "$first" "$last" "$pairs"; do
  case "$number" in ''|*[!0-9]*) echo 'Expected integer frame bounds and pair count' >&2; exit 2;; esac
done
[ "$first" -gt 1 ] && [ "$last" -ge "$first" ] && [ "$pairs" -ge 5 ] || exit 2
baseline=${RR_MULTICORE_BASELINE:-$work/rr_baseline}
candidate=${RR_MULTICORE_CANDIDATE:-$work/rr_parallel_prep}
[ -x "$baseline" ] && [ -x "$candidate" ] && [ -f "$recording" ] || exit 2
[ -f "$work/reference-$last.json" ] || { echo 'Missing deterministic endpoint reference' >&2; exit 2; }
if pidof rr_gles2 >/dev/null; then echo 'Rave Racer is running' >&2; exit 1; fi
out="$work/ab-$(date +%Y%m%d-%H%M%S)-$$"
mkdir -p "$out"
sha256sum "$baseline" "$candidate" "$recording" \
  /userdata/roms/ports/rave-gles2/rr_gles2 > "$out/manifest.sha256"
child=
monitor=
restore_es=0
restore() {
  status=$?
  if [ -n "$child" ]; then kill "$child" 2>/dev/null || :; wait "$child" 2>/dev/null || :; fi
  if [ -n "$monitor" ]; then kill "$monitor" 2>/dev/null || :; wait "$monitor" 2>/dev/null || :; fi
  if [ "$restore_es" = 1 ]; then
    /etc/init.d/S31emulationstation start > "$out/es-restore.log" 2>&1 || status=1
  fi
  echo "$status" > "$out/exit"
  exit "$status"
}
trap restore EXIT
trap 'exit 130' INT TERM HUP
export SDL_VIDEODRIVER=mali XDG_RUNTIME_DIR=/var/run
if pidof emulationstation >/dev/null; then
  restore_es=1
  /etc/init.d/S31emulationstation stop
fi
run_one() {
  name=$1
  binary=$2
  "$binary" /userdata/roms/ports/rave-gles2/extracted \
    --replay-input "$recording" --replay-checks off --frames "$last" \
    --timing independent --skip-geometry-on-dropped-frames on \
    --texture-budget-mib 64 --vbo-upload frame --audio on --overlay off \
    --profile-from "$first" --thread-profile "$out/$name.presentations.csv" \
    --replay-timing "$out/$name.ticks.csv" \
    > "$out/$name.json" 2> "$out/$name.log" &
  child=$!
  (
    while kill -0 "$child" 2>/dev/null; do
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
      [ ! -r /proc/"$child"/status ] || awk '/VmRSS:|VmHWM:/{print}' /proc/"$child"/status
      sleep 1
    done
  ) > "$out/$name.monitor" &
  monitor=$!
  wait "$child"
  child=
  kill "$monitor" 2>/dev/null || :
  wait "$monitor" 2>/dev/null || :
  monitor=
  cmp "$work/reference-$last.json" "$out/$name.json"
}
pair=1
while [ "$pair" -le "$pairs" ]; do
  if [ $((pair % 2)) -eq 1 ]; then variants='baseline candidate'; else variants='candidate baseline'; fi
  for variant in $variants; do
    echo "$(date) pair=$pair variant=$variant output=$out"
    case "$variant" in
      baseline) run_one "$variant-$pair" "$baseline" ;;
      candidate) run_one "$variant-$pair" "$candidate" ;;
    esac
  done
  pair=$((pair + 1))
done
sha256sum -c "$out/manifest.sha256"
echo "Completed: $out"
