#!/bin/sh
# Primary Knulli Ports launcher. Original game HUD/audio, overlay initially off.
app_dir=$(CDPATH= cd -- "$(dirname -- "$0")/rave-gles2" && pwd) || exit 1
cd "$app_dir" || exit 1
if [ "${SDL_VIDEODRIVER+x}" != x ]; then SDL_VIDEODRIVER=mali; fi
if [ "${XDG_RUNTIME_DIR+x}" != x ]; then XDG_RUNTIME_DIR=/var/run; fi
export SDL_VIDEODRIVER XDG_RUNTIME_DIR
{
 printf '\n=== Rave Racer: %s ===\n' "$(date)"
 ./rr_gles2 extracted --interactive --timing independent \
  --skip-geometry-on-dropped-frames on --texture-budget-mib 64 --vbo-upload frame \
  --audio on --overlay off "$@"
 result=$?
 printf 'Application exit code: %s\n' "$result"
 exit "$result"
} >> "$app_dir/rave-racer.log" 2>&1
