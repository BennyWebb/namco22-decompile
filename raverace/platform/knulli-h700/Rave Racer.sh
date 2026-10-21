#!/bin/sh
set -eu
export PYTHONDONTWRITEBYTECODE=1
app_dir=$(CDPATH= cd -- "$(dirname -- "$0")/rave-racer" && pwd)
cd "$app_dir"
exec python3 launcher/portable.py "$@"
