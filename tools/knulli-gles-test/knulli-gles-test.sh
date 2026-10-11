#!/bin/sh
# Keep this launcher alongside the executable in the Ports directory.
app_dir=$(CDPATH= cd -- "$(dirname -- "$0")/knulli-gles-test" && pwd) || exit 1
cd "$app_dir" || exit 1
if [ "${SDL_VIDEODRIVER+x}" != x ]; then
    SDL_VIDEODRIVER=mali
fi
export SDL_VIDEODRIVER
log_file="$app_dir/knulli-gles-test.log"
{
    printf '\n=== Knulli GLES test: %s ===\n' "$(date)"
    printf 'SDL_VIDEODRIVER=%s\n' "$SDL_VIDEODRIVER"
    ./knulli-gles-test "$@"
    result=$?
    printf 'Application exit code: %s\n' "$result"
    exit "$result"
} >> "$log_file" 2>&1
