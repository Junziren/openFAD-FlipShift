#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
[[ "$(uname -s)" == Darwin ]] || { echo "Requires macOS"; exit 1; }
build="vst3/build-macos/OpenFADFlipShift_artefacts/Release"
stage="$(mktemp -d)"
trap 'rm -rf "$stage"' EXIT
mkdir -p dist/macos "$stage/payload/Library/Audio/Plug-Ins/VST3" "$stage/payload/Library/Audio/Plug-Ins/Components"
for format in VST3 AU; do
  extension=vst3
  destination=VST3
  if [[ "$format" == AU ]]; then extension=component; destination=Components; fi
  source="$build/$format/openFAD FlipShift.$extension"
  target="$stage/payload/Library/Audio/Plug-Ins/$destination/openFAD FlipShift.$extension"
  test -d "$source"
  ditto "$source" "$target"
  binary="$target/Contents/MacOS/openFAD FlipShift"
  lipo "$binary" -verify_arch arm64 x86_64
  codesign --force --sign - "$target"
  codesign --verify --strict --verbose=2 "$target"
done
version="$(sed -n 's/.*project(OpenFADFlipShift VERSION \([^ ]*\).*/\1/p' vst3/CMakeLists.txt)"
test -n "$version"
name="openFAD-FlipShift-${version}-macOS-Universal-unsigned"
pkgbuild --root "$stage/payload" --identifier com.unpurebloom.openfad.flipshift.pkg \
  --version "$version" --install-location / "dist/macos/$name.pkg"
pkgutil --payload-files "dist/macos/$name.pkg" > dist/macos/package-contents.txt
cp MACOS_INSTALL.md "$stage/payload/README-macOS.md"
cp LICENSING.md "$stage/payload/LICENSING.md"
ditto -c -k --sequesterRsrc "$stage/payload" "dist/macos/$name.zip"
git rev-parse HEAD > dist/macos/BUILD-COMMIT.txt
cp MACOS_INSTALL.md dist/macos/README-macOS.md
(cd dist/macos && shasum -a 256 "$name.pkg" "$name.zip" > SHA256SUMS.txt)
