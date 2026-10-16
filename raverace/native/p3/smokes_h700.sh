#!/bin/sh
set -eu
cd /userdata/system/rave-racer-native-p3-20261016
cat /proc/uptime > resume-start.uptime
sha256sum -c /userdata/system/rave-racer-native-stage1b-20261015/protected-files-final.sha256 > resume-protected-before.txt
for mode in nice rr; do
 echo smoke-$mode > resume-status.txt
 sh run_h700_resume.sh r-smoke-$mode r-priority smoke "$mode"
done
tar -czf resume-smokes.tgz r-smoke-nice r-smoke-rr resume-start.uptime resume-protected-before.txt
echo SMOKES_READY > resume-status.txt
