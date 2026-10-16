#!/bin/sh
# A single named smoke/timing; never implicitly retries or expands the matrix.
set -eu
cd /userdata/system/rave-racer-native-p3-20261016
name=$1;binary=$2;mode=$3;priority=$4
echo "$name" > resume-status.txt
if [ "$mode" = headline ];then [ -f "${name}-smoke/smoke-proof.json" ];fi
sh run_h700_resume.sh "$name" "$binary" "$mode" "$priority"
if [ "$mode" = headline ];then cmp "$name/states.jsonl" /userdata/system/rave-racer-native-p1b-20261016/validation-q2/states.jsonl;fi
tar -czf "$name.tgz" "$name"
echo "$name-READY" > resume-status.txt
