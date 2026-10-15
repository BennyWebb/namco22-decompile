#!/bin/sh
# Called only after a diagnostic child and its cleanup trap finish.
set -eu
base=/userdata/system/rave-racer-native-stage1b-20261015
name=$1;run=$base/$name;work=/tmp/rr-stage1b-$name
cat "$run/gpu.original" "$run/cpu0.original" "$run/cpu1.original" "$run/cpu2.original" "$run/cpu3.original" > "$run/restoration.expected"
cmp "$run/restoration.expected" "$run/restored.txt"
cmp "$run/production.before" "$run/production.after"
tries=0;until pidof emulationstation > "$run/es-restored.txt";do tries=$((tries+1));[ "$tries" -lt 30 ];sleep 1;done
for f in "$work"/*;do cmp "$f" "$run/${f##*/}";done
for f in "$work"/*;do rm "$f";done
rmdir "$work"
tar -czf "$base/$name.tgz" -C "$base" "$name"
sync
