#!/bin/sh
set -eu
base=/userdata/system/rave-racer-native-stage1b-20261015
name=${1:-C-panel}
case "$name" in C-panel|C-panel-observed) :;;*) exit 2;;esac
run=$base/$name
work=/tmp/rr-stage1b-$name
mkdir "$run" "$work"
gpu=/sys/class/devfreq/gpu
{ cat "$gpu/governor" "$gpu/min_freq" "$gpu/max_freq";for c in 0 1 2 3;do cat /sys/devices/system/cpu/cpu$c/cpufreq/scaling_governor;done; } > "$run/clocks.before"
sha256sum /userdata/roms/ports/rave-gles2/rr_gles2 > "$run/production.before"
stopped=0;child=;monitor=
restore(){
 set +e
 if [ -n "$child" ] && kill -0 "$child" 2>/dev/null;then kill -TERM "$child";wait "$child";fi
 if [ -n "$monitor" ];then kill "$monitor" 2>/dev/null;wait "$monitor" 2>/dev/null;fi
 # This probe never changes governors or frequencies.
 { cat "$gpu/governor" "$gpu/min_freq" "$gpu/max_freq";for c in 0 1 2 3;do cat /sys/devices/system/cpu/cpu$c/cpufreq/scaling_governor;done; } > "$run/clocks.after"
 cp "$work"/* "$run/" 2>/dev/null
 if [ "$stopped" = 1 ];then /etc/init.d/S31emulationstation start > "$run/es-start.txt" 2>&1;fi
 sha256sum /userdata/roms/ports/rave-gles2/rr_gles2 > "$run/production.after"
}
trap restore EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM
{ cat /proc/meminfo;df -k /tmp;du -sk /tmp; } > "$run/preflight.txt"
if pidof emulationstation >/dev/null;then stopped=1;/etc/init.d/S31emulationstation stop > "$run/es-stop.txt" 2>&1;fi
{ for f in /sys/class/graphics/fb0/*;do if [ -f "$f" ] && [ -r "$f" ];then echo "PATH $f";head -c 4096 "$f" 2>/dev/null || :;fi;done;for f in /sys/class/disp/disp/attr/sys /sys/class/disp/disp/attr/lcd /sys/kernel/debug/clk/clk_summary;do if [ -r "$f" ];then echo "PATH $f";cat "$f";fi;done; } > "$run/display-driver.txt"
SDL_VIDEODRIVER=mali XDG_RUNTIME_DIR=/var/run "$base/panel_probe" > "$work/run.txt" 2>&1 & child=$!
"$base/clock_sampler" "$child" > "$work/clocks.txt" 2>&1 & monitor=$!
set +e;wait "$child";result=$?;set -e;child=
printf '%s\n' "$result" > "$work/exit.txt"
exit "$result"
