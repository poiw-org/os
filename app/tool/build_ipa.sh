#!/bin/sh
# Builds the iOS app in release mode, unsigned, and packages it as build/doora.ipa (replacing the
# old one). Sign it at install time (AltStore, Sideloadly) or with your own profile.
set -e
app=$(cd "$(dirname "$0")/.." && pwd)
cd "$app"
flutter build ios --release --no-codesign
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir "$work/Payload"
cp -R build/ios/iphoneos/Runner.app "$work/Payload/"
(cd "$work" && zip -qry doora.ipa Payload)
mv "$work/doora.ipa" build/doora.ipa
echo "Built $app/build/doora.ipa"
