#!/bin/sh
# Run ONLY after every admitted variant passes host and ARM validation.
set -eu
base=/userdata/system/rave-racer-native-stage1b-20261015
bestgov=$1;bestpin=$2
cd "$base"
for round in ${RR_FLAG_ROUNDS:-1};do
 case "$round" in
 1) cases='stock:O1 best:O1 best:O2 best:O2-nosp';;
 2) cases='best:O2-nosp best:O2 best:O1 stock:O1';;
 3) cases='best:O2 stock:O1 best:O2-nosp best:O1';;
 esac
 for entry in $cases;do
  scope=${entry%:*};variant=${entry#*:}
  if [ ! -f "$base/validated-$variant" ];then printf 'DISQUALIFIED %s\n' "$variant" >> flag-performance-history.txt;continue;fi
  config=$bestgov;pin=$bestpin
  if [ "$scope" = stock ];then config=stock;pin=off;fi
  name=E-$round-$scope-$variant-native
  if [ -d "$base/$name" ];then
   [ -f "$base/$name.tgz" ] && [ "$(cat "$base/$name/exit.txt")" = 0 ]
   printf 'RETAINED %s\n' "$name" >> flag-performance-history.txt
   continue
  fi
  printf 'RUNNING %s\n' "$name" > flag-performance-status.txt
  sh "$base/run.sh" "$name" native "$config" "$pin" "$variant"
  sh "$base/finish_run.sh" "$name"
  printf 'DONE %s\n' "$name" >> flag-performance-history.txt
  sleep 20
 done
done
printf 'COMPLETE\n' > flag-performance-status.txt
