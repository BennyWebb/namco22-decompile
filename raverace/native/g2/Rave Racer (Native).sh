#!/bin/sh
# Separate Native entry. Production paths are read-only inputs.
app=/userdata/system/rave-racer-native-g2
export PYTHONDONTWRITEBYTECODE=1
exec /usr/bin/python3 "$app/session.py" "$@" > "$app/startup.log" 2>&1
