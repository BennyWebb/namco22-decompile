#!/bin/sh
# Run only after evidence-supported final candidate is selected and validated.
set -eu
base=/userdata/system/rave-racer-native-p2-20261016
cd "$base";binary=$1;first=$2
[ -f "$first/es-restored.txt" ]
[ "$(cat "$first/exit.txt")" = 0 ]
expected=$(sha256sum "$base/$binary");actual=$(head -n 1 "$first/binary.sha256");[ "$expected" = "$actual" ]
printf 'final-1 aliases %s; same existing complete timing population, not an additional run.\n' "$first" > final-alias.txt
run(){ echo "$1" > headline-status.txt;sh run_h700.sh "$@"; }
for round in 2 3;do
 run "baseline-$round" baseline headline
 run "v2-play-$round" v2-play-play headline
 run "final-$round" "$binary" headline
done
tar -czf headline.tgz "$first" final-alias.txt baseline-2 v2-play-2 final-2 baseline-3 v2-play-3 final-3
sync
echo soak-1512000 > headline-status.txt
sh soak_h700.sh "$binary" 1512000
if sh qualify_soak_h700.sh "$base/soak-1512000" 1512000;then
 echo 'Unflagged at1512000; no lower-clock repeat required.' > soak-clock-decision.txt
else
 echo 'Flagged or invalid clock coverage at1512000; retaining and repeating at1416000.' > soak-clock-decision.txt
 echo soak-1416000 > headline-status.txt
 sh soak_h700.sh "$binary" 1416000
 sh qualify_soak_h700.sh "$base/soak-1416000" 1416000 || echo 'Lower frequency soak also flagged; no further frequency changes authorised.' >> soak-clock-decision.txt
fi
echo COMPLETE > headline-status.txt
