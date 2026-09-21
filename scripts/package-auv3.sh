#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
[[ "$(uname -s)" == Darwin ]] || { echo "Requires macOS" >&2; exit 1; }

build_root="vst3/build-macos/OpenFADFlipShift_artefacts/Release"
dist_root="dist/auv3"
stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT

find_bundle() {
  local pattern="$1"
  find "$build_root" -type d -name "$pattern" -print -quit
}

standalone="$(find_bundle 'openFAD FlipShift.app')"
extension="$(find_bundle 'openFAD FlipShift.appex')"
[[ -n "$standalone" && -d "$standalone" ]] || { echo "AUv3 standalone app not found under $build_root" >&2; exit 1; }
[[ -n "$extension" && -d "$extension" ]] || { echo "AUv3 app extension not found under $build_root" >&2; exit 1; }

rm -rf "$dist_root"
mkdir -p "$dist_root" "$stage/OpenFAD-FlipShift-AUv3"
ditto "$standalone" "$stage/OpenFAD-FlipShift-AUv3/openFAD FlipShift.app"
ditto "$extension" "$stage/OpenFAD-FlipShift-AUv3/openFAD FlipShift.appex"

app_binary="$stage/OpenFAD-FlipShift-AUv3/openFAD FlipShift.app/Contents/MacOS/openFAD FlipShift"
extension_binary="$stage/OpenFAD-FlipShift-AUv3/openFAD FlipShift.appex/Contents/MacOS/openFAD FlipShift"
[[ -f "$app_binary" ]] || { echo "Standalone executable is missing" >&2; exit 1; }
[[ -f "$extension_binary" ]] || { echo "AUv3 executable is missing" >&2; exit 1; }
lipo "$app_binary" -verify_arch arm64 x86_64
lipo "$extension_binary" -verify_arch arm64 x86_64
plutil -lint "$stage/OpenFAD-FlipShift-AUv3/openFAD FlipShift.app/Contents/Info.plist"
plutil -lint "$stage/OpenFAD-FlipShift-AUv3/openFAD FlipShift.appex/Contents/Info.plist"

# CI intentionally produces an unsigned review package. Signing/notarization
# belongs in a release job with Apple certificates and provisioning data.
codesign --verify --deep --strict "$stage/OpenFAD-FlipShift-AUv3/openFAD FlipShift.app" 2>/dev/null || true

version="$(sed -n 's/.*project(OpenFADFlipShift VERSION \([^ ]*\).*/\1/p' vst3/CMakeLists.txt)"
[[ -n "$version" ]] || { echo "Could not read project version" >&2; exit 1; }
name="openFAD-FlipShift-${version}-AUv3-macOS-unsigned"
ditto -c -k --sequesterRsrc "$stage/OpenFAD-FlipShift-AUv3" "$dist_root/$name.zip"
find "$stage/OpenFAD-FlipShift-AUv3" -maxdepth 5 -print | sort > "$dist_root/$name-contents.txt"
git rev-parse HEAD > "$dist_root/BUILD-COMMIT.txt"
(cd "$dist_root" && shasum -a 256 "$name.zip" > SHA256SUMS.txt)
echo "Packaged AUv3: $dist_root/$name.zip"
