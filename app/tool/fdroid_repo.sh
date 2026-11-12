#!/usr/bin/env bash
# Adds one APK to the F-Droid repo kept in a directory (the gh-pages checkout) and regenerates
# and signs its index. Needs fdroidserver, and ANDROID_HOME with build-tools.
#
#   fdroid_repo.sh <pages-dir> <apk> <version-code> <repo-url>
# Env: FDROID_KEYSTORE (path), FDROID_KEYSTORE_PASSWORD. The key alias is "fdroid".
set -euo pipefail
pages=$(cd "$1" && pwd); apk=$(cd "$(dirname "$2")" && pwd)/$(basename "$2"); code=$3; url=$4
here=$(cd "$(dirname "$0")/../.." && pwd)

mkdir -p "$pages/fdroid/repo/icons" "$pages/fdroid/metadata/org.poiw.doora/en-US"
cd "$pages/fdroid"
printf "tmp/\nsrclibs/\n" > .gitignore
cp "$apk" "repo/org.poiw.doora_$code.apk"
cp "$here/web/static/poiw.png" repo/icons/icon.png
cp "$here/web/static/poiw.png" metadata/org.poiw.doora/en-US/icon.png

cat > metadata/org.poiw.doora.yml <<YML
Categories: [Security]
License: Unknown
SourceCode: https://github.com/poiw-org/os
AuthorName: po/iw
WebSite: https://poiw.org
Name: Doora
Summary: The door lock of the po/iw hackerspace
Description: |-
  Your virtual keys, and the unlock. Walk up to the door and open it from your phone over Bluetooth.
YML

cat > config.yml <<YML
repo_url: $url
repo_name: po/iw
repo_description: Apps from the po/iw hackerspace.
repo_icon: icon.png
repo_keyalias: fdroid
keystore: $FDROID_KEYSTORE
keystorepass: $FDROID_KEYSTORE_PASSWORD
keypass: $FDROID_KEYSTORE_PASSWORD
keydname: CN=po/iw F-Droid, O=po-iw
YML
chmod 600 config.yml

fdroid update
rm -f config.yml
