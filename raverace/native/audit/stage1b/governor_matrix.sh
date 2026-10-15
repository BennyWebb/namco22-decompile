#!/bin/sh
set -eu
base=/userdata/system/rave-racer-native-stage1b-20261015
cd "$base"
# Counterbalance governors and executable order. Three complete repeats each.
for round in 1 2 3;do
 case "$round" in 1) configs='stock gpu cpu both';modes='native production';;2) configs='both cpu gpu stock';modes='production native';;3) configs='gpu stock both cpu';modes='native production';;esac
 for config in $configs;do for mode in $modes;do
  name=B-$round-$config-$mode
  [ ! -e "$base/$name.tgz" ] || continue
  printf 'RUNNING %s\n' "$name" > matrix-status.txt
  if [ ! -e "$base/$name/exit.txt" ];then sh "$base/run.sh" "$name" "$mode" "$config";fi
  run="$base/$name";work=/tmp/rr-stage1b-$name
  cat "$run/gpu.original" "$run/cpu0.original" "$run/cpu1.original" "$run/cpu2.original" "$run/cpu3.original" > "$run/restoration.expected"
  cmp -s "$run/restoration.expected" "$run/restored.txt"
  cmp -s "$run/production.before" "$run/production.after"
  tries=0
  until pidof emulationstation > "$run/es-restored.txt";do tries=$((tries+1));[ "$tries" -lt 30 ];sleep 1;done
  # No archival/copying while a game is active. Verify copied RAM logs first.
  for f in "$work"/*;do cmp -s "$f" "$run/${f##*/}";done
  for f in "$work"/*;do rm "$f";done
  rmdir "$work"
  tar -czf "$base/$name.tgz" -C "$base" "$name"
  printf 'DONE %s\n' "$name" >> matrix-history.txt
  sleep 20
 done;done
done
printf 'COMPLETE\n' > matrix-status.txt
