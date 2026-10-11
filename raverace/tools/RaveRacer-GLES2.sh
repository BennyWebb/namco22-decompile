#!/bin/sh
# Ports launcher lives beside the rave-gles2 directory, not inside it.
app_dir=$(CDPATH= cd -- "$(dirname -- "$0")/rave-gles2" && pwd) || exit 1
cd "$app_dir" || exit 1
if [ "${SDL_VIDEODRIVER+x}" != x ]; then
    SDL_VIDEODRIVER=mali
fi
export SDL_VIDEODRIVER
{
    printf '\n=== Rave Racer GLES2 original textures: %s ===\n' "$(date)"
    printf 'SDL_VIDEODRIVER=%s\n' "$SDL_VIDEODRIVER"
    ./rr_gles2 extracted --interactive --vbo-upload frame "$@"
    result=$?
    printf 'Application exit code: %s\n' "$result"
    exit "$result"
} >> "$app_dir/rave-gles2.log" 2>&1
