#!/bin/sh
set -eu
cd /userdata/system/rave-racer-native-p3-20261016
binary=$1;first=$2
[ -f r-validation-final/parity.json ] && [ -f "$first-smoke/smoke-proof.json" ]
[ "$(head -1 "$first/binary.sha256")" = "$(sha256sum "$binary" | sed "s|  $binary$|  /userdata/system/rave-racer-native-p3-20261016/$binary|")" ]
printf 'final-1 aliases %s; same complete timing population, counted once.\n' "$first" > resume-final-alias.txt
echo FINAL_2 > resume-status.txt
sh run_h700_resume.sh r-final-2 "$binary" headline off
cmp r-final-2/states.jsonl /userdata/system/rave-racer-native-p1b-20261016/validation-q2/states.jsonl
tar -czf resume-final-2.tgz r-final-2 resume-final-alias.txt
echo SOAK > resume-status.txt
sh soak_h700_resume.sh "$binary" off
echo COMPLETE > resume-status.txt
