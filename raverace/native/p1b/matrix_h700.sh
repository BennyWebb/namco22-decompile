#!/bin/sh
set -eu
base=/userdata/system/rave-racer-native-p1b-20261016
cd "$base"
for round in 1 2 3;do
 case "$round" in 1|3) depths='2 3';;2) depths='3 2';;esac
 for depth in $depths;do
  name=reference-$round-q$depth-off
  echo "RUNNING $name" > matrix-status.txt
  sh "$base/run_h700.sh" "$name" "$depth" 0 headline
  cmp "$name/states.jsonl" validation-q2/states.jsonl
  cmp "$name/command-sha256.csv" validation-q2/command-sha256.csv
  echo "DONE $name" >> matrix-history.txt
  tar -czf "$name.tgz" "$name";sync
 done
 name=reference-$round-q2-on
 echo "RUNNING $name" > matrix-status.txt
 sh "$base/run_h700.sh" "$name" 2 1 headline
 cmp "$name/states.jsonl" validation-q2/states.jsonl
 cmp "$name/command-sha256.csv" validation-q2/command-sha256.csv
 echo "DONE $name" >> matrix-history.txt
 tar -czf "$name.tgz" "$name";sync
done
name=observer-q2
 echo "RUNNING $name" > matrix-status.txt
 sh "$base/run_h700.sh" "$name" 2 0 observer
 cmp "$name/states.jsonl" validation-q2/states.jsonl
 cmp "$name/command-sha256.csv" validation-q2/command-sha256.csv
 echo "DONE $name" >> matrix-history.txt
 tar -czf "$name.tgz" "$name";sync
echo COMPLETE > matrix-status.txt
