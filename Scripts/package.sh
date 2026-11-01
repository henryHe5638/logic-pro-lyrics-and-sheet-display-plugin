#!/bin/bash
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VERSION="1.0.1"
PRODUCT="LyricsAndSheet"
BUILD_DIR="${BUILD_DIR:-$ROOT/.build/cmake-universal}"
DIST="$ROOT/dist"
ARCHS="${ARCHS:-arm64;x86_64}"

echo "== 构建 $ARCHS =="
cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES="$ARCHS"
cmake --build "$BUILD_DIR" --target LogicLyricsAndSheet_AU -j "$(sysctl -n hw.ncpu)"

COMPONENT="$BUILD_DIR/LogicLyricsAndSheet_artefacts/Release/AU/$PRODUCT.component"

if [[ ! -d "$COMPONENT" ]]; then
  echo "找不到构建产物: $COMPONENT" >&2
  exit 1
fi

echo "== 打包 =="
rm -rf "$DIST"
mkdir -p "$DIST/payload" "$DIST/zip" "$DIST/pkg-scripts"

cp -R "$COMPONENT" "$DIST/payload/"
cp -R "$COMPONENT" "$DIST/zip/"
cp "$ROOT/Scripts/Install-User.command" "$DIST/zip/"
cp "$ROOT/Scripts/README-install.txt" "$DIST/zip/"
chmod +x "$DIST/zip/Install-User.command"

xattr -cr "$DIST/payload/$PRODUCT.component" "$DIST/zip/$PRODUCT.component" 2>/dev/null || true

cat > "$DIST/pkg-scripts/postinstall" <<'POSTINSTALL'
#!/bin/bash
COMPONENT="/Library/Audio/Plug-Ins/Components/LyricsAndSheet.component"
xattr -dr com.apple.quarantine "$COMPONENT" 2>/dev/null || true
codesign --force --deep --sign - "$COMPONENT" 2>/dev/null || true
killall -9 AudioComponentRegistrar 2>/dev/null || true
exit 0
POSTINSTALL
chmod +x "$DIST/pkg-scripts/postinstall"

pkgbuild --root "$DIST/payload" \
         --install-location "/Library/Audio/Plug-Ins/Components" \
         --identifier "local.logiclyricsandsheet" \
         --version "$VERSION" \
         --scripts "$DIST/pkg-scripts" \
         "$DIST/LogicLyricsAndSheet-$VERSION.pkg"

(cd "$DIST/zip" && zip -qry "../LogicLyricsAndSheet-$VERSION.zip" .)

echo
lipo -info "$COMPONENT/Contents/MacOS/$PRODUCT"
echo
echo "生成完成:"
ls -lh "$DIST"/*.pkg "$DIST"/*.zip
