#!/bin/sh
set -eu
base=/userdata/system/rave-racer-native-live-4a-20261015/stale-m2-tmp-archive
test ! -e "$base"
mkdir "$base"
for name in rr-m2rev-hud rr-m2rev-second rr-m2rev-scene3600 rr-m2rev-paired rr-m2rev-full rr-m2rev-small rr-m2rev-scene rr-m2-results-final rr-m2-results; do
 test -d "/tmp/$name"
 test ! -L "/tmp/$name"
 cp -R "/tmp/$name" "$base/" || exit 2
 (cd /tmp; find "$name" -type f -exec sha256sum {} \;) > "$base/$name.sha256"
 (cd "$base"; sha256sum -c "$name.sha256") > "$base/$name.verified"
 # Only remove the explicitly listed diagnostic tree after verifying its complete file set.
 test -z "$(find "/tmp/$name" -type l)"
 rm -r "/tmp/$name"
done
df -h /tmp > "$base/tmp-after.txt"
