#!/bin/sh
# A real recorded race, never a synthetic steering sequence. Separate binary.
set -eu
[ "$#" -ge 3 ] || { echo 'Usage: RaveRacer-Replay-Benchmark.sh RECORDING.rec RACE_FIRST RACE_LAST [RUNS]' >&2;exit 2; }
recording=$1;first=$2;last=$3;runs=${4:-3}
case "$recording" in /*);;*)echo 'Use an absolute replay path' >&2;exit 2;;esac
for n in "$first" "$last" "$runs"; do case "$n" in ''|*[!0-9]*)exit 2;;esac;done
[ "$first" -gt 1 ] && [ "$last" -ge "$first" ] && [ "$runs" -gt 0 ] || exit 2
app=${RR_APP_DIR:-/userdata/roms/ports/rave-gles2}
experimental=${RR_REPLAY_DIR:-/userdata/system/rave-racer-replay}
out="$experimental/results-$(date +%Y%m%d-%H%M%S)-$$"
mkdir "$out"
for suffix in '' .meta .eeprom .end .states.jsonl; do [ -f "$recording$suffix" ] || exit 2;done
[ "$last" -le "$(cat "$recording.end")" ] || { echo 'Requested range beyond recording';exit 2; }
# Avoid interrupting a user playing either installation.
if pidof rr_gles2 rr_replay >/dev/null; then echo 'Exit the running game before benchmarking' >&2;exit 2;fi
restore_es=0;child=
restore(){
 status=$?
 if [ -n "$child" ];then kill "$child" 2>/dev/null || :;wait "$child" 2>/dev/null || :;fi
 if [ "$restore_es" = 1 ];then /etc/init.d/S31emulationstation start > "$out/es-restore.log" 2>&1 || status=1;fi
 exit "$status"
}
trap restore EXIT
trap 'exit 130' INT TERM HUP
export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-mali} XDG_RUNTIME_DIR=${XDG_RUNTIME_DIR:-/var/run}
sha256sum "$app/rr_gles2" "$experimental/rr_replay" "$recording" > "$out/manifest.sha256"
# Verify the entire original recording without presentation or audio output.
"$experimental/rr_headless" "$app/extracted" --replay-input "$recording" \
 --input-checkpoints "$out/verification.states.jsonl" > "$out/verification.json" 2> "$out/verification.log" &
child=$!;wait "$child";child=
cmp "$recording.states.jsonl" "$out/verification.states.jsonl"
"$experimental/rr_headless" "$app/extracted" --replay-input "$recording" --frames "$last" --replay-checks off  > "$out/benchmark-reference.json" 2> "$out/benchmark-reference.log" &
child=$!;wait "$child";child=
if pidof emulationstation >/dev/null; then restore_es=1;/etc/init.d/S31emulationstation stop;fi
if pidof emulationstation >/dev/null;then echo 'Frontend did not stop';exit 1;fi
trial=1
while [ "$trial" -le "$runs" ]; do
 case "$trial" in 2|4|6)modes='traced untraced';;*)modes='untraced traced';;esac
 for mode in $modes;do
  echo "Replay benchmark $trial/$runs $mode: $out"
  if [ "$mode" = traced ];then
   name="run-$trial"
   set -- --thread-profile "$out/$name.presentations.csv" --replay-timing "$out/$name.ticks.csv"
  else name="untraced-$trial";set --;fi
  "$experimental/rr_replay" "$app/extracted" --replay-input "$recording" --replay-checks off \
   --frames "$last" --timing independent --skip-geometry-on-dropped-frames on --texture-budget-mib 64 \
   --vbo-upload frame --audio on --overlay off --profile-from "$first" "$@" \
   > "$out/$name.json" 2> "$out/$name.log" &
  child=$!;wait "$child";child=
  cmp "$out/benchmark-reference.json" "$out/$name.json"
 done
 trial=$((trial+1))
done
sha256sum -c "$out/manifest.sha256"
echo "Verified recording; benchmark results: $out"
