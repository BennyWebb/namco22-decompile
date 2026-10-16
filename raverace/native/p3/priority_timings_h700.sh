#!/bin/sh
set -eu
cd /userdata/system/rave-racer-native-p3-20261016
[ -f r-smoke-nice/smoke-proof.json ] && [ -f r-smoke-rr/smoke-proof.json ]
for mode in nice rr;do
 case $mode in nice) name=r-a1;;rr) name=r-a2;;esac
 echo "$name" > resume-status.txt
 sh run_h700_resume.sh "$name" r-priority headline "$mode"
 cmp "$name/states.jsonl" /userdata/system/rave-racer-native-p1b-20261016/validation-q2/states.jsonl
 [ "$(grep -c '^\[P3-PRIORITY\]' "$name/run.txt")" = 3 ]
 ! grep '^\[P3-PRIORITY\].*error=[1-9]' "$name/run.txt"
done
tar -czf resume-priorities.tgz r-a1 r-a2
echo PRIORITIES_READY > resume-status.txt
