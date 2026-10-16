#!/bin/sh
set -eu
base=/userdata/system/rave-racer-native-p1b-20261016
cd "$base"
echo VALIDATION_RUNNING > experiment-status.txt
sh "$base/run_h700.sh" validation-q2 2 0 validation
cmp validation-q2/states.jsonl validation-q2-initial/states.jsonl
cmp validation-q2/command-sha256.csv validation-q2-initial/command-sha256.csv
cmp validation-q2/image-sha256.csv validation-q2-initial/image-sha256.csv
grep -q 'completed=3 expected=3' validation-q2/run.txt
grep -q 'dropped_records=0 .*errors=0 packets=4207' validation-q2/run.txt
tar -czf validation-q2.tgz validation-q2;sync
echo MATRIX_RUNNING > experiment-status.txt
sh "$base/matrix_h700.sh"
echo TRACE_RUNNING > experiment-status.txt
sh "$base/run_h700.sh" trace-q2 2 0 trace
cmp trace-q2/states.jsonl validation-q2/states.jsonl
cmp trace-q2/command-sha256.csv validation-q2/command-sha256.csv
tar -czf trace-q2.tgz trace-q2;sync
echo SOAK_RUNNING > experiment-status.txt
sh "$base/soak_h700.sh"
tar -czf soak-stock-q2.tgz soak-stock-q2
sha256sum -c /userdata/system/rave-racer-native-stage1b-20261015/protected-files-final.sha256 > protected-final.txt
pidof emulationstation > es-final.txt
! pgrep -x rr_p1b
! pgrep -x memory_sampler
sync
echo COMPLETE > experiment-status.txt
