#!/bin/sh
set -eu
base=/userdata/system/rave-racer-native-p2-20261016
ref=/userdata/system/rave-racer-native-p1b-20261016/validation-q2
cd "$base"
run(){ echo "$1" > v2-status.txt; sh run_h700.sh "$@"; }
validate(){
 step=$1
 run "v2-validation-$step" "v2-$step-validation" validation
 for file in states.jsonl command-sha256.csv image-sha256.csv;do cmp "$ref/$file" "v2-validation-$step/$file";done
}
if [ -f v2-validation-play/es-restored.txt ];then
 for file in states.jsonl command-sha256.csv image-sha256.csv;do cmp "$ref/$file" "v2-validation-play/$file";done
else validate play;fi
run v2-play-1 v2-play-play headline
run v2-profile-play-run v2-profile-play profile
for step in a b c d e;do
 validate "$step"
 run "v2-timing-$step" "v2-$step-play" headline
 run "v2-profile-$step-run" "v2-profile-$step" profile
 tar -czf "v2-step-$step.tgz" "v2-validation-$step" "v2-timing-$step" "v2-profile-$step-run"
 sync
done
echo AWAITING_EVIDENCE_SELECTION > v2-status.txt
