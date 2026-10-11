#!/bin/sh
# Same real-ROM city/race sequence in every rendering mode. 301 measured frames.
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
    printf '\n=== GLES2 city benchmark: %s mode=%s frames=1400..1700 vbo=frame ===\n' "$(date)" "$mode"
    ./rr_gles2 extracted --frames 1700 --coin 180 --gas 200 \
        --steer 300:2304 --steer 700:2048 --unpaced --profile-from 1400 \
        --vbo-upload frame --render-mode "$mode" "$@"
    result=$?
    printf 'Application exit code: %s\n' "$result"
    exit "$result"
} >> "$app_dir/$mode-city-benchmark.log" 2>&1
