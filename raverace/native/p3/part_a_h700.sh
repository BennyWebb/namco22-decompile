#!/bin/sh
set -eu
cd /userdata/system/rave-racer-native-p3-20261016
cat /proc/uptime > device-session-start.uptime
sha256sum -c /userdata/system/rave-racer-native-stage1b-20261015/protected-files-final.sha256 > protected-before.txt
for name in control a1 a2;do
 echo "$name" > status.txt
 case $name in control) binary=p2-control;priority=off;;a1) binary=priority;priority=nice;;a2) binary=priority;priority=rr;;esac
 sh run_h700.sh "$name" "$binary" headline "$priority"
done
tar -czf part-a.tgz control a1 a2 status.txt protected-before.txt device-session-start.uptime
echo PART_A_COMPLETE > status.txt
