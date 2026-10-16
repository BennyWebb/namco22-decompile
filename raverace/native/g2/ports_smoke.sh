#!/bin/sh
# Outer SSH-only verification harness; the normal Ports launcher never manages ES.
set -eu
b=/userdata/system/rave-racer-native-g2-20261016
app=/userdata/system/rave-racer-native-g2
r=$b/ports-smoke
mkdir "$r"
clocks(){ for c in 0 1 2 3;do p=/sys/devices/system/cpu/cpu$c/cpufreq;cat "$p/scaling_governor" "$p/scaling_min_freq" "$p/scaling_max_freq";done;cat /sys/class/devfreq/gpu/governor /sys/class/devfreq/gpu/min_freq /sys/class/devfreq/gpu/max_freq; }
clocks > "$r/clocks.before"
sha256sum -c "$b/protected.sha256" > "$r/protected.before"
cat /proc/meminfo > "$r/memory.before"
du -sk /tmp > "$r/tmp.before"
child=
cleanup(){
 set +e
 if [ -n "$child" ] && kill -0 "$child" 2>/dev/null;then kill -TERM "$child";wait "$child";fi
 clocks > "$r/clocks.after"
 sha256sum -c "$b/protected.sha256" > "$r/protected.after"
 env -i PATH=/usr/bin:/bin:/usr/sbin:/sbin HOME=/root /etc/init.d/S31emulationstation start > "$r/es-start.txt" 2>&1
 sleep 2
 pidof emulationstation > "$r/es-restored.txt"
 cmp "$r/clocks.before" "$r/clocks.after" > "$r/clock-compare.txt" 2>&1
 cat "$app/latest-summary.json" > "$r/summary.json"
 cat "$app/latest-summary.txt" > "$r/summary.txt"
 cat "$app/clock-journal.json" > "$r/clock-journal.json"
}
trap cleanup EXIT
trap 'exit 143' TERM
/etc/init.d/S31emulationstation stop > "$r/es-stop.txt" 2>&1
'/userdata/roms/ports/Rave Racer (Native).sh' --smoke &
child=$!
wait "$child";code=$?;child=
echo "$code" > "$r/exit.txt"
