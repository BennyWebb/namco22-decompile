#!/bin/sh
# One-step rollback: remove only the separate entry; preserve native saves/evidence.
set -eu
app=/userdata/system/rave-racer-native-g2
if [ -d "$app/active" ];then echo 'Close Rave Racer Native before removal.' >&2;exit 1;fi
rm -f -- '/userdata/roms/ports/Rave Racer (Native).sh'
echo 'Native Ports entry removed. Production untouched; native saves/cache/logs retained.'
