#!/bin/sh
set -eu
base=/userdata/system/rave-racer-native-p1-20261015
cd "$base"
for round in 1;do # User reduced repeats; three-round original retained in hardware metadata.
 case "$round" in 1) configs='stock:2 both:3 stock:3 both:2';modes='timed fence';;2) configs='both:2 stock:3 both:3 stock:2';modes='fence timed';;3) configs='stock:3 both:2 stock:2 both:3';modes='timed fence';;esac
 for config in $configs;do for mode in $modes;do
  profile=${config%:*};depth=${config#*:};name=reference-$round-$profile-q$depth-$mode
  printf 'RUNNING %s\n' "$name" > matrix-status.txt
  sh "$base/run_h700.sh" "$name" "$profile" "$depth" "$mode"
  [ "$(cat "$name/exit.txt")" = 0 ]
  cmp "$name/states.jsonl" validation-q3/states.jsonl
  cmp "$name/clocks.before" "$name/clocks.after"
  cmp "$name/production.before" "$name/production.after"
  [ -s "$name/es-restored.txt" ]
  tar -czf "$name.tgz" "$name"
  sync
  printf 'DONE %s checkpoints=21 exit=0 restored=1\n' "$name" >> matrix-history.txt
  sleep 20
 done;done
done
printf 'COMPLETE\n' > matrix-status.txt
