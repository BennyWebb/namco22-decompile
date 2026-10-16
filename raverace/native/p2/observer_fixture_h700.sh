#!/bin/sh
set -eu
base=/userdata/system/rave-racer-native-p2-20261016
run=$base/observer-fixture;mkdir "$run";cd "$run"
monitor=;one=;two=
trap 'kill ${monitor:-} ${one:-} ${two:-} 2>/dev/null || true' EXIT
sleep 20 & one=$!;sleep 20 & two=$!
echo "$one" > target.pid
"$base/memory_sampler" "$$" "$run/target.pid" > memory.txt 2>&1 & monitor=$!
sleep 1;echo "$two" > target.pid;sleep 1;echo 0 > target.pid;sleep 1
kill -TERM "$monitor";wait "$monitor";monitor=
for target in "$one" "$two";do grep -q "TARGET_PID $target" memory.txt;grep -q "FILE /proc/$target/task/$target/stat" memory.txt;done
grep -q 'TARGET_PID 0' memory.txt
echo 'PASS explicit child PID switching and main-thread records' > result.txt
