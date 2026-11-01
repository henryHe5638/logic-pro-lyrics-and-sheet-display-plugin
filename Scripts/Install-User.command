#!/bin/bash
set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
DEST="$HOME/Library/Audio/Plug-Ins/Components"
COMPONENT="LyricsAndSheet.component"

if [[ ! -d "$DIR/$COMPONENT" ]]; then
  echo "没有找到 $COMPONENT，请把本脚本和插件放在同一个文件夹里。"
  read -n 1 -s -r -p "按任意键关闭窗口..." || true
  exit 1
fi

mkdir -p "$DEST"
rm -rf "$DEST/$COMPONENT"
cp -R "$DIR/$COMPONENT" "$DEST/"

xattr -dr com.apple.quarantine "$DEST/$COMPONENT" 2>/dev/null || true
codesign --force --deep --sign - "$DEST/$COMPONENT" 2>/dev/null || true
killall -9 AudioComponentRegistrar 2>/dev/null || true

echo
echo "已安装到: $DEST"
echo "请完全退出并重新打开 Logic Pro，然后在音频效果中找到 Local > LyricsAndSheet。"
echo
read -n 1 -s -r -p "按任意键关闭窗口..." || true
echo
