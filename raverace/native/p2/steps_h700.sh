#!/bin/sh
set -eu
base=/userdata/system/rave-racer-native-p2-20261016
ref=/userdata/system/rave-racer-native-p1b-20261016/validation-q2
cd "$base"
echo profile-play > steps-status.txt
[ -f profile-play-run/es-restored.txt ] || sh run_h700.sh profile-play-run profile-play profile
for step in a b c d e;do
 echo "validation-$step" > steps-status.txt
 sh run_h700.sh validation-$step "$step-validation" validation
 cmp "$ref/states.jsonl" "validation-$step/states.jsonl"
 cmp "$ref/command-sha256.csv" "validation-$step/command-sha256.csv"
 cmp "$ref/image-sha256.csv" "validation-$step/image-sha256.csv"
 echo "timing-$step" > steps-status.txt
 sh run_h700.sh timing-$step "$step-play" headline
 echo "profile-$step" > steps-status.txt
 sh run_h700.sh "profile-$step-run" "profile-$step" profile
 # Archive only between runs, never alongside a game.
 tar -czf "step-$step.tgz" "validation-$step" "timing-$step" "profile-$step-run"
 sync
done
tar -czf profile-play-run.tgz profile-play-run
sync
echo COMPLETE > steps-status.txt
