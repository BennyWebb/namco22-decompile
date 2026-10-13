#!/bin/sh
set -eu
work=${RR_MULTITEXTURE_TEST_DIR:-/userdata/system/rave-racer-multitexture-resume-20261013}
out=${RESULTS_DIR:-"$work/gpu-dense-$(date +%Y%m%d-%H%M%S)-$$"}
[ ! -e "$out" ] || { echo "Results already exist: $out" >&2;exit 2; }
mkdir -p "$out"
restore_es=0
restore(){ rc=$?;trap - EXIT;if [ "$restore_es" = 1 ];then /etc/init.d/S31emulationstation start > "$out/es-restore.log" 2>&1 || rc=1;fi;echo "$rc" > "$out/exit";exit "$rc"; }
trap restore EXIT
trap 'exit 130' INT TERM HUP
if pidof rr_gles2 rr_baseline rr_batched rr_checks rr_gpu >/dev/null;then exit 4;fi
export SDL_VIDEODRIVER=mali XDG_RUNTIME_DIR=/var/run GLES_PROFILE_FROM=1800 GLES_PROFILE_TO=5639 GLES_MULTI_GPU_FROM=1800 GLES_MULTI_GPU_TO=5639
if pidof emulationstation >/dev/null;then restore_es=1;/etc/init.d/S31emulationstation stop;fi
for mode in off shader batch;do
 "$work/bin/rr_gpu" /userdata/roms/ports/rave-gles2/extracted --replay-input /userdata/system/rave-racer-replays/race-20261013-122242-25136.rec --replay-checks off --frames 5639 --timing independent --skip-geometry-on-dropped-frames on --texture-budget-mib 64 --vbo-upload frame --audio on --overlay off --gles-multitexture "$mode" --gles-multitexture-gpu-trace "$out/race-$mode.gpu.csv" --profile-from 1800 --thread-profile "$out/race-$mode.presentations.csv" --replay-timing "$out/race-$mode.ticks.csv" > "$out/race-$mode.json" 2> "$out/race-$mode.log"
 cmp /userdata/system/rave-racer-multicore/reference-5639.json "$out/race-$mode.json"
done
unset GLES_PROFILE_FROM GLES_PROFILE_TO GLES_MULTI_GPU_FROM GLES_MULTI_GPU_TO
for pair in 1 2 3;do
 if [ $((pair % 2)) -eq 1 ];then variants='baseline candidate';else variants='candidate baseline';fi
 for variant in $variants;do
  if [ "$variant" = baseline ];then binary="$work/bin/rr_baseline";mode=off;else binary="$work/bin/rr_batched";mode=batch;fi
  "$binary" /userdata/roms/ports/rave-gles2/extracted --frames 1700 --coin 180 --gas 200 --steer 300:2304 --steer 700:2048 --timing independent --skip-geometry-on-dropped-frames on --texture-budget-mib 64 --vbo-upload frame --audio on --overlay off --gles-multitexture "$mode" --profile-from 1400 --thread-profile "$out/dense-$variant-$pair.presentations.csv" > "$out/dense-$variant-$pair.json" 2> "$out/dense-$variant-$pair.log"
 done
 cmp "$out/dense-baseline-$pair.json" "$out/dense-candidate-$pair.json"
done
