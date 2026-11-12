#!/bin/sh
# Builds and runs door-sim.swift. macOS kills a process that touches Bluetooth without a usage
# string, so the binary gets an Info.plist embedded. Args pass through: [door-id] [result-code].
set -e
dir=$(cd "$(dirname "$0")" && pwd)
out=${TMPDIR:-/tmp}/doora-door-sim
mkdir -p "$out"
cat > "$out/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
<key>CFBundleIdentifier</key><string>app.doora.door-sim</string>
<key>CFBundleName</key><string>Doora door simulator</string>
<key>NSBluetoothAlwaysUsageDescription</key><string>Pretends to be the door so the app can be tested.</string>
</dict></plist>
EOF
swiftc -O "$dir/door-sim.swift" -o "$out/door-sim" \
  -Xlinker -sectcreate -Xlinker __TEXT -Xlinker __info_plist -Xlinker "$out/Info.plist"
exec "$out/door-sim" "$@"
