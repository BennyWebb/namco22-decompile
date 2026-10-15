#!/bin/sh
# Validation only; hashing/CPU timings are never performance results.
set -eu
base=/userdata/system/rave-racer-native-stage1b-20261015
pin=$1
cd "$base"
for variant in current O1 O2 O2-nosp;do
 name=E-validation-$variant-native
 printf 'RUNNING %s\n' "$name" > flag-validation-status.txt
 set +e
 sh "$base/run.sh" "$name" native both "$pin" "$variant" hash
 result=$?
 set -e
 sh "$base/finish_run.sh" "$name"
 printf 'DONE %s exit=%s\n' "$name" "$result" >> flag-validation-history.txt
 sleep 20
done
printf 'COMPLETE\n' > flag-validation-status.txt
