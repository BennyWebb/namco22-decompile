#!/bin/sh
set -eu
work=${RR_MULTITEXTURE_TEST_DIR:-/userdata/system/rave-racer-multitexture-resume-20261013}
out=${RESULTS_DIR:-"$work/fixed-correctness-$(date +%Y%m%d-%H%M%S)-$$"}
[ ! -e "$out" ] || { echo "Results already exist: $out" >&2;exit 2; }
mkdir -p "$out"
restore_es=0
restore(){ rc=$?;trap - EXIT;if [ "$restore_es" = 1 ];then /etc/init.d/S31emulationstation start > "$out/es-restore.log" 2>&1 || rc=1;fi;echo "$rc" > "$out/exit";exit "$rc"; }
trap restore EXIT
trap 'exit 130' INT TERM HUP
if pidof rr_gles2 rr_baseline rr_batched rr_checks rr_gpu >/dev/null;then exit 4;fi
export SDL_VIDEODRIVER=mali XDG_RUNTIME_DIR=/var/run GLES_TEXTURE_SEMANTIC_TRACE=1
if pidof emulationstation >/dev/null;then restore_es=1;/etc/init.d/S31emulationstation stop;fi
for mode in off shader batch;do
 mkdir -p "$out/$mode"
 export GLES_CAPTURE_DIRECTORY="$out/$mode" GLES_CAPTURE_TICKS=600,1500,1701,1902,1965,2001,2499,2502,3600,4800,5637,6006
 export GLES_TIER_TRACE="$out/$mode/tier.txt" GLES_TIER_KEY=1871,576,593,79,12,103,0
 "$work/bin/rr_checks" /userdata/roms/ports/rave-gles2/extracted --replay-input /userdata/system/rave-racer-replays/race-20261013-122242-25136.rec --frames 6006 --timing independent --test-present-every 3 --skip-geometry-on-dropped-frames on --texture-budget-mib 64 --vbo-upload frame --audio off --audio-checksum-from 1 --gles-multitexture "$mode" --capture "$out/$mode/final.ppm" --capture-frame 6006 > "$out/$mode/state.json" 2> "$out/$mode/run.log"
 grep -E '^\[(AUDIO-CHECK|BOARD-STATE|GEOMETRY|PRESENTATION-GEOMETRY|TEXTURE-SEMANTICS)\]' "$out/$mode/run.log" > "$out/$mode/digests.txt"
 if [ "$mode" != off ];then
  cmp "$out/off/state.json" "$out/$mode/state.json"
  cmp "$out/off/digests.txt" "$out/$mode/digests.txt"
  for ppm in "$out/off/"*.ppm;do cmp "$ppm" "$out/$mode/${ppm##*/}";done
 fi
done
unset GLES_CAPTURE_DIRECTORY GLES_CAPTURE_TICKS GLES_TIER_TRACE
export GLES_MULTI_VALIDATE="$out" GLES_MULTI_VALIDATE_TO=1700
"$work/bin/rr_checks" /userdata/roms/ports/rave-gles2/extracted --frames 1700 --coin 180 --gas 200 --steer 300:2304 --steer 700:2048 --timing independent --skip-geometry-on-dropped-frames on --texture-budget-mib 8 --vbo-upload frame --audio off --audio-checksum-from 1 --gles-multitexture batch > "$out/mutation-stress.json" 2> "$out/mutation-stress.log"
echo 'fixed cadence pixels, ordered resource events, simulation, board, PCM and voice checks passed'
