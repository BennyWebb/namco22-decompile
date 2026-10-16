#!/bin/sh
set -eu
cd /userdata/system/rave-racer-native-p3-20261016
echo VALIDATING_FINAL > resume-status.txt
sh run_h700_resume.sh r-validation-final "$1" validation off
cmp r-validation-final/states.jsonl /userdata/system/rave-racer-native-p1b-20261016/validation-q2/states.jsonl
cmp r-validation-final/command-sha256.csv /userdata/system/rave-racer-native-p2-20261016/v2-validation-b/command-sha256.csv
cmp r-validation-final/image-sha256.csv /userdata/system/rave-racer-native-p2-20261016/v2-validation-b/image-sha256.csv
tar -czf resume-validation.tgz r-validation-final
echo VALIDATION_READY > resume-status.txt
