#!/bin/sh
set -eu
base=/userdata/system/rave-racer-native-p2-20261016
ref=/userdata/system/rave-racer-native-p1b-20261016/validation-q2
cd "$base"
echo mix-validation > mix-status.txt
sh run_h700.sh mix-validation-run mix-validation validation
for file in states.jsonl command-sha256.csv image-sha256.csv;do cmp "$ref/$file" "mix-validation-run/$file";done
echo mix-timing > mix-status.txt
sh run_h700.sh mix-timing-run mix-play headline
echo mix-profile > mix-status.txt
sh run_h700.sh mix-profile-run mix-profile profile
tar -czf mix-evidence.tgz mix-validation-run mix-timing-run mix-profile-run
sync
echo AWAITING_EVIDENCE_SELECTION > mix-status.txt
