#!/bin/sh
# Run alongside a handheld replay benchmark; stop once EmulationStation returns.
set -eu
out=${1:?output path required}
seen=0
while :; do
  pid=$(pidof rr_pacing_a rr_pacing_b rr_gles2 || :)
  if [ -n "$pid" ]; then
    seen=1
    printf 'SAMPLE %s PID %s\n' "$(cut -d' ' -f1 /proc/uptime)" "$pid" >> "$out"
    for task in /proc/"$pid"/task/*; do
      [ -r "$task/schedstat" ] || continue
      tid=${task##*/}
      printf 'TASK %s ' "$tid" >> "$out"
      cat "$task/comm" "$task/schedstat" >> "$out" 2>/dev/null || :
    done
  elif [ "$seen" = 1 ] && pidof emulationstation >/dev/null; then
    break
  fi
  sleep 1
done
