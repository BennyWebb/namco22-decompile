#!/bin/sh
# Install beside rave-gles2; keep the normal Ports launcher unchanged.
app_dir=$(CDPATH= cd -- "$(dirname -- "$0")/rave-gles2" && pwd) || exit 1
cd "$app_dir" || exit 1
if [ "${SDL_VIDEODRIVER+x}" != x ]; then SDL_VIDEODRIVER=mali; fi
export SDL_VIDEODRIVER
{
 printf '\n=== Experimental threaded Rave Racer: %s ===\n' "$(date)"
 ./rr-thread-final-arm64 extracted --interactive --timing independent \
  --skip-geometry-on-dropped-frames on --texture-budget-mib 64 --vbo-upload frame "$@"
 result=$?
 printf 'Application exit code: %s\n' "$result"
 exit "$result"
} >> "$app_dir/rave-threaded.log" 2>&1
