#!/bin/sh
# One continuous >=30min stock-clock replay-loop session; no cooldown between laps.
# Replay processes restart original state each lap; no single-process leak claim.
set -eu
base=/userdata/system/rave-racer-native-p1b-20261016
assets=/userdata/system/rave-racer-native-live-4a-20261015
run=$base/soak-stock-q2;mkdir "$run"
game=;monitor=;stopped=0
clocks(){ cat /sys/class/devfreq/gpu/governor /sys/class/devfreq/gpu/min_freq /sys/class/devfreq/gpu/max_freq;for c in 0 1 2 3;do cat /sys/devices/system/cpu/cpu$c/cpufreq/scaling_governor;done; }
clocks > "$run/clocks.before"
[ "$(sed -n 1p "$run/clocks.before")" = simple_ondemand ]
[ "$(sed -n 2p "$run/clocks.before")" = 0 ]
[ "$(sed -n 3p "$run/clocks.before")" = 0 ]
for n in 4 5 6 7;do [ "$(sed -n "${n}p" "$run/clocks.before")" = schedutil ];done
sha256sum /userdata/roms/ports/rave-gles2/rr_gles2 > "$run/production.before"
cleanup(){ set +e
 if [ -n "$game" ] && kill -0 "$game" 2>/dev/null;then kill -TERM "$game";wait "$game";fi
 if [ -n "$monitor" ];then kill -TERM "$monitor";wait "$monitor";fi
 clocks > "$run/clocks.after";sha256sum /userdata/roms/ports/rave-gles2/rr_gles2 > "$run/production.after"
 if [ "$stopped" = 1 ];then env -i PATH=/usr/bin:/bin:/usr/sbin:/sbin HOME=/root /etc/init.d/S31emulationstation start > "$run/es-start.txt" 2>&1;fi
 tries=0;until pidof emulationstation > "$run/es-restored.txt";do tries=$((tries+1));[ "$tries" -lt 30 ]||break;sleep 1;done
 cmp "$run/clocks.before" "$run/clocks.after";cmp "$run/production.before" "$run/production.after"
}
trap cleanup EXIT;trap 'exit 143' TERM;trap 'exit 130' INT;trap 'exit 129' HUP
while :;do read temp < /sys/class/thermal/thermal_zone0/temp;echo "$temp" >> "$run/cooldown.txt";[ "$temp" -lt 50000 ]&&break;sleep 5;done
echo "$temp" > "$run/temperature.start"
{ cat /proc/meminfo;df -k /tmp /userdata;du -sk /tmp; } > "$run/preflight.txt"
[ "$(awk '/MemAvailable:/{print $2}' /proc/meminfo)" -gt 600000 ]||exit 3
sync
if pidof emulationstation >/dev/null;then stopped=1;/etc/init.d/S31emulationstation stop > "$run/es-stop.txt" 2>&1;fi
export ENG_P1B_VALVE_PERIODS=2
export SDL_VIDEODRIVER=mali XDG_RUNTIME_DIR=/var/run ENG_GPU_HUD=1 ENG_GPU_HUD_GEOMETRY=1 ENG_P1_QUEUE=2 ENG_P1B_VALVE=0
export ENG_LIVE_PACK="$assets/pack" ENG_LIVE_SHADERS="$assets/shaders" ENG_LIVE_LOOKUP="$assets/live.lookup" ENG_P1B_WARM_PACKET="$base/warm-tick1800.live"
unset ENG_P1B_TEST_PRESENT_TICK ENG_P1B_TEST_PRESENT_MS LD_PRELOAD RR_AFFINITY ENG_P1_FENCE ENG_P1_VALIDATION ENG_LIVE_CAPTURE ENG_LIVE_COMMAND_DUMP ENG_P1B_IMAGE_HASH ENG_P1B_RECEIPTS
read began rest < /proc/uptime;echo "$began" > "$run/start.uptime";start=${began%.*}
"$base/memory_sampler" "$$" follow > "$run/memory.txt" 2>&1 & monitor=$!
lap=0
while :;do
 lap=$((lap+1));dir=$run/lap-$lap;mkdir "$dir";cd "$dir"
 read began rest < /proc/uptime;echo "$began" > "$dir/start.uptime"
 ENG_P1B_RECEIPTS="$dir/command-sha256.csv" "$base/rr_p1b" /userdata/roms/ports/rave-gles2/extracted --presentation-thread on --timing independent --audio on --thread-profile "$dir/thread.csv" --profile-simulation --simulation-profile-from 1800 --replay-input /userdata/system/rave-racer-replays/race-20261013-122242-25136.rec --input-checkpoints "$dir/states.jsonl" --replay-timing "$dir/ticks.csv" > "$dir/run.txt" 2>&1 & game=$!
 set +e;wait "$game";result=$?;set -e;game=;echo "$result" > "$dir/exit.txt";[ "$result" = 0 ]||exit "$result"
 cmp "$dir/states.jsonl" "$base/validation-q2/states.jsonl";cmp "$dir/command-sha256.csv" "$base/validation-q2/command-sha256.csv"
 read ended rest < /proc/uptime;echo "$ended" > "$dir/end.uptime";now=${ended%.*};printf 'lap=%s end=%s elapsed=%s checks=PASS\n' "$lap" "$ended" "$((now-start))" >> "$run/history.txt"
 [ "$((now-start))" -ge 1800 ]&&break
done
echo "$ended" > "$run/end.uptime";echo 0 > "$run/exit.txt";sync
