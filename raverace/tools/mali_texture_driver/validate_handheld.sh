set -eu
work=/userdata/system/rave-racer-world-atlas-20261014
out="$work/driver-retain-proof-$(date +%Y%m%d-%H%M%S)-$$"
mkdir -p "$out"
restore(){ rc=$?;trap - EXIT;env -u ENG_WORLD_ARRAY_RETAIN -u ENG_DRIVER_LAST_TICK -u ENG_MALI_CALL_TRACE /etc/init.d/S31emulationstation start > "$out/es-restore.log" 2>&1 || rc=1;echo "$rc" > "$out/exit";exit "$rc"; }
trap restore EXIT
trap 'exit 130' INT TERM HUP
export SDL_VIDEODRIVER=mali XDG_RUNTIME_DIR=/var/run
/etc/init.d/S31emulationstation stop
count=0
while pidof emulationstation >/dev/null || pgrep -f '^/bin/bash /usr/bin/emulationstation-standalone' >/dev/null;do count=$((count+1));[ "$count" -le 20 ];sleep 1;/etc/init.d/S31emulationstation stop;done
ENG_WORLD_ARRAY_RETAIN=1 GLES_WORLD_VALIDATE="$out" ENG_GPU_HUD_VERIFY=1 timeout -k 5 600 "$work/rr_mali_retain" /userdata/roms/ports/rave-gles2/extracted --replay-input /userdata/system/rave-racer-replays/race-20261013-122242-25136.rec --frames 5639 --timing independent --test-present-every 3 --skip-geometry-on-dropped-frames on --texture-budget-mib 64 --vbo-upload frame --gles-world-atlas group --audio off --audio-checksum-from 1 --input-checkpoints "$out/checkpoints.jsonl" --capture-frame 5639 --capture "$out/final.ppm" > "$out/state.json" 2> "$out/render.log"
cmp "$work/reference-5639.json" "$out/state.json"
! grep -q "WORLD-VALIDATE-FAIL\|WORLD-PIXEL" "$out/render.log"
grep -q "pcm_fnv1a64=a18439c518e08e3d voice_fnv1a64=c12e04d613b280fc final_fnv1a64=a588e1f884d1c5f8" "$out/render.log"
echo "Checked: $out"
