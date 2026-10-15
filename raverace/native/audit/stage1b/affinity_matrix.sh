#!/bin/sh
set -eu
base=/userdata/system/rave-racer-native-stage1b-20261015
best=$1
cd "$base"
finish(){
 name=$1;run=$base/$name;work=/tmp/rr-stage1b-$name
 cat "$run/gpu.original" "$run/cpu0.original" "$run/cpu1.original" "$run/cpu2.original" "$run/cpu3.original" > "$run/restoration.expected"
 cmp "$run/restoration.expected" "$run/restored.txt"
 cmp "$run/production.before" "$run/production.after"
 tries=0;until pidof emulationstation > "$run/es-restored.txt";do tries=$((tries+1));[ "$tries" -lt 30 ];sleep 1;done
 for f in "$work"/*;do cmp "$f" "$run/${f##*/}";done
 for f in "$work"/*;do rm "$f";done;rmdir "$work"
 tar -czf "$base/$name.tgz" -C "$base" "$name"
 sync
 printf 'DONE %s\n' "$name" >> affinity-history.txt
 sleep 20
}
configs="stock $best";[ "$best" != stock ] || configs=stock
for config in $configs;do for round in 1 2;do
 if [ "$round" = 1 ];then modes='native production';pins='off pinned';else modes='production native';pins='pinned off';fi
 for mode in $modes;do for pin in $pins;do
  name=D-$round-$config-$mode-$pin
  printf 'RUNNING %s\n' "$name" > affinity-status.txt
  variant=current;[ "$mode" != production ] || variant=O1
  sh "$base/run.sh" "$name" "$mode" "$config" "$pin" "$variant"
  finish "$name"
 done;done
done;done
# Separate opt-in shutdown logging supplements, not unchanged-B performance rows.
for config in gpu cpu both;do
 [ "$config" != "$best" ] || continue
 name=B-supplement-$config-production
 printf 'RUNNING %s\n' "$name" > affinity-status.txt
 sh "$base/run.sh" "$name" production "$config" off O1
 finish "$name"
done
printf 'COMPLETE\n' > affinity-status.txt
