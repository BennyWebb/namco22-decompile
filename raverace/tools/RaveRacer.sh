#!/bin/sh
# Primary Knulli Ports launcher. Original game HUD/audio, overlay initially off.
app_dir=$(CDPATH= cd -- "$(dirname -- "$0")/rave-gles2" && pwd) || exit 1
cd "$app_dir" || exit 1
if [ "${SDL_VIDEODRIVER+x}" != x ]; then SDL_VIDEODRIVER=mali; fi
if [ "${XDG_RUNTIME_DIR+x}" != x ]; then XDG_RUNTIME_DIR=/var/run; fi
export SDL_VIDEODRIVER XDG_RUNTIME_DIR
# H700 production build defaults to the validated glyph-quad HUD.
# gpu-hud-mode=off selects the original CPU HUD without replacing the binary.
unset ENG_GPU_HUD_VERIFY ENG_GPU_HUD_CAPTURE ENG_GPU_HUD_CAPTURE_TICKS ENG_HUD_GPU_TRACE
unset ENG_TEXTURE_PREDECODE_MANIFEST ENG_TEXTURE_PREDECODE_ORIENTED ENG_TEXTURE_PREDECODE_VERIFY
unset ENG_TEXTURE_GPU_PREWARM ENG_TEXTURE_GPU_RECORD ENG_TEXTURE_MISS_TRACE
gpu_hud_mode=default
if [ -f "$app_dir/gpu-hud-mode" ]; then IFS= read -r gpu_hud_mode < "$app_dir/gpu-hud-mode" || :; fi
case "$gpu_hud_mode" in
 default|'') unset ENG_GPU_HUD ENG_GPU_HUD_GEOMETRY ;;
 on) export ENG_GPU_HUD=1 ENG_GPU_HUD_GEOMETRY=1 ;;
 off) export ENG_GPU_HUD=0 ENG_GPU_HUD_GEOMETRY=0 ;;
 *) echo "Unknown gpu-hud-mode: $gpu_hud_mode" >&2; exit 2 ;;
esac
# Use the device-specific saved mapping if supplied by installation.
if [ -f "$app_dir/controller-input.cfg" ]; then
 set -- --es-input "$app_dir/controller-input.cfg" "$@"
fi
{
 printf '\n=== Rave Racer: %s ===\n' "$(date)"
 # Optional experimental recording; absent/off retains the known-good binary.
 binary=./rr_gles2
 replay_mode=off
 if [ -f "$app_dir/replay-mode" ]; then IFS= read -r replay_mode < "$app_dir/replay-mode" || :; fi
 case "$replay_mode" in
  off|'') ;;
  record|record-once)
   if [ "$replay_mode" = record-once ]; then printf "off\n" > "$app_dir/replay-mode" || exit 1; fi
   binary=/userdata/system/rave-racer-replay/rr_replay
   replay_dir=/userdata/system/rave-racer-replays
   mkdir -p "$replay_dir" || exit 1
   replay="$replay_dir/race-$(date +%Y%m%d-%H%M%S)-$$.rec"
   set -- --record-input "$replay" "$@"
   (set -C; sha256sum "$binary" ./rr_gles2 > "$replay.build.sha256") || exit 1
   printf 'Recording from boot: %s; START+SELECT finishes and saves.\n' "$replay"
   ;;
  *) printf 'Unknown replay-mode: %s\n' "$replay_mode";exit 2;;
 esac
 "$binary" extracted --interactive --timing independent \
  --skip-geometry-on-dropped-frames on --texture-budget-mib 64 --vbo-upload frame \
  --audio on --overlay off "$@"
 result=$?
 printf 'Application exit code: %s\n' "$result"
 exit "$result"
} >> "$app_dir/rave-racer.log" 2>&1
