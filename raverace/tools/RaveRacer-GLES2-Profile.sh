#!/bin/sh
# Short Ports test; mode may also be supplied as the first SSH argument.
app_dir=$(CDPATH= cd -- "$(dirname -- "$0")/rave-gles2" && pwd) || exit 1
cd "$app_dir" || exit 1
mode=${1:-textured}
if [ "$#" -gt 0 ]; then shift; fi
case "$mode" in
    textured|diagnostic|white-batches|textured-no-rebind) ;;
    *) printf 'Unknown render mode: %s\n' "$mode" >&2; exit 2 ;;
esac
if [ "${SDL_VIDEODRIVER+x}" != x ]; then SDL_VIDEODRIVER=mali; fi
export SDL_VIDEODRIVER
{
    printf '\n=== GLES2 diagnostic: %s mode=%s ===\n' "$(date)" "$mode"
    ./rr_gles2 extracted --frames 300 --coin 180 --gas 200 --steer 300:2304 \
        --unpaced --profile-from 100 --render-mode "$mode" "$@"
    result=$?
    printf 'Application exit code: %s\n' "$result"
    exit "$result"
} >> "$app_dir/$mode-profile.log" 2>&1
