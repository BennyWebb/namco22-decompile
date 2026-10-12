#!/bin/sh
# Investigation only. Plain scores have no CSV/PMU/query/telemetry instrumentation.
set -u
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd) || exit 1
cd "${RR_AUDIT_APP_DIR:-$script_dir/rave-gles2}" || exit 1
out=${RESULTS_DIR:-"$PWD/architecture-audit-$(date +%Y%m%d-%H%M%S)-$$"}
[ ! -e "$out" ] || exit 1
[ -x ./rr_gles2-audit-plain ] && [ -x ./rr_gles2-audit-profile ] || exit 1
mkdir -p "$out" || exit 1
export SDL_VIDEODRIVER=${SDL_VIDEODRIVER:-mali}
{ date;uname -a;sha256sum ./rr_gles2-audit-*;cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor;cat /sys/class/devfreq/gpu/governor;} > "$out/manifest.txt"
failed=0;child=;monitor=
trap 'if [ -n "$child" ];then kill "$child" 2>/dev/null;fi;if [ -n "$monitor" ];then kill "$monitor" 2>/dev/null;fi;exit 130' INT TERM HUP
run_case() {
 phase=$1;trial=$2
 name="$workload-$phase-$trial";exe=./rr_gles2-audit-profile;telemetry=yes
 case "$phase" in
  plain) exe=./rr_gles2-audit-plain;telemetry=no;set --;;
  profile) set -- --profile-stages --audit-csv "$out/$name.csv" --audit-gpu-timers --sample-simulation "$out/$name-samples.tsv";;
  detail) set -- --profile-stages --audit-csv "$out/$name.csv" --audit-detail --audit-gpu-timers --sample-simulation "$out/$name-samples.tsv";;
  half) set -- --profile-stages --audit-csv "$out/$name.csv" --audit-gpu-timers --audit-half-resolution;;
  flat) set -- --profile-stages --audit-csv "$out/$name.csv" --audit-gpu-timers --audit-flat-fragment;;
  nodraw) set -- --profile-stages --audit-csv "$out/$name.csv" --audit-gpu-timers --audit-no-draw;;
  diagnostic) set -- --profile-stages --audit-csv "$out/$name.csv" --audit-gpu-timers --render-mode diagnostic;;
  white) set -- --profile-stages --audit-csv "$out/$name.csv" --audit-gpu-timers --render-mode white-batches;;
  norebind) set -- --profile-stages --audit-csv "$out/$name.csv" --audit-gpu-timers --render-mode textured-no-rebind;;
 esac
 echo "Running $name (1400..$frames)"
 "$exe" extracted --frames "$frames" --coin 180 --gas 200 --steer 300:2304 --steer 700:2048 \
  --timing independent --skip-geometry-on-dropped-frames on --texture-budget-mib 64 \
  --vbo-upload frame --profile-from 1400 --simulation-profile-from 1400 --profile-memory "$@" > "$out/$name.json" 2> "$out/$name.log" &
 child=$!
 if [ "$telemetry" = yes ];then python3 /userdata/roms/ports/audit_system_monitor.py "$child" "$out/$name-system.jsonl" & monitor=$!;fi
 wait "$child";status=$?;child=
 if [ -n "$monitor" ];then wait "$monitor";monitor=;fi
 echo "$status" > "$out/$name.exit"
 if [ "$status" -ne 0 ] || ! grep -q '"traps":0,"unmapped":0,"romwrites":0' "$out/$name.json" || ! grep -q '"dsp_fault":false,"sound_fault":false' "$out/$name.json";then failed=1;fi
 echo "$name exit=$status"
}
for workload in city race;do
 case "$workload" in city) frames=1700;;race) frames=3600;;esac
 for trial in 1 2 3;do
  case "$trial" in 2) order='profile plain';;*) order='plain profile';;esac
  for phase in $order;do run_case "$phase" "$trial";done
 done
 # Same scripted windows: controls alter presentation only, never simulation.
 for phase in half flat nodraw diagnostic white norebind;do
  for trial in 1 2;do run_case "$phase" "$trial";done
 done
 run_case detail 1
 for state in "$out/$workload-"*.json;do cmp "$out/$workload-plain-1.json" "$state" || failed=1;done
done
sha256sum "$out"/*.json > "$out/state-sha256.txt"
echo "Architecture audit: $out"
exit "$failed"
