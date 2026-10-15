#!/bin/sh
set -eu
base=/userdata/system/rave-racer-native-stage1b-20261015
child=;monitor=
cleanup(){
 set +e
 if [ -n "$child" ] && kill -0 "$child" 2>/dev/null;then kill "$child";wait "$child";fi
 if [ -n "$monitor" ];then kill "$monitor" 2>/dev/null;wait "$monitor" 2>/dev/null;fi
}
trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM
for mode in unset pinned;do
 run=$base/D-affinity-validation-$mode;work=/tmp/rr-stage1b-D-affinity-validation-$mode
 mkdir "$run" "$work"
 { cat /sys/class/devfreq/gpu/governor /sys/class/devfreq/gpu/min_freq /sys/class/devfreq/gpu/max_freq;for c in 0 1 2 3;do cat /sys/devices/system/cpu/cpu$c/cpufreq/scaling_governor;done; } > "$run/clocks.before"
 unset LD_PRELOAD RR_AFFINITY
 set -- env LD_PRELOAD="$base/affinity.so"
 if [ "$mode" = pinned ];then set -- "$@" RR_AFFINITY=main:0,prep:1,present:2;fi
 "$base/trace_affinity" "$@" "$base/affinity_smoke" > "$work/run.txt" 2>&1 & child=$!
 "$base/clock_sampler" "$child" > "$work/clocks.txt" 2>&1 & monitor=$!
 set +e;wait "$child";result=$?;kill "$monitor" 2>/dev/null;wait "$monitor" 2>/dev/null;child=;monitor=;set -e
 echo "$result" > "$work/exit.txt"
 { cat /sys/class/devfreq/gpu/governor /sys/class/devfreq/gpu/min_freq /sys/class/devfreq/gpu/max_freq;for c in 0 1 2 3;do cat /sys/devices/system/cpu/cpu$c/cpufreq/scaling_governor;done; } > "$run/clocks.after"
 cmp "$run/clocks.before" "$run/clocks.after"
 pidof emulationstation > "$run/es-unchanged.txt"
 cp "$work"/* "$run/"
 cat "$run/run.txt"
 [ "$result" -eq 0 ]
 if [ "$mode" = unset ];then grep -q 'sched_setaffinity_entry_and_exit_stops=0 child_exit=0' "$run/run.txt";fi
 if [ "$mode" = pinned ];then grep -q '^audio-helper: 0 1 2 3$' "$run/run.txt";fi
done
