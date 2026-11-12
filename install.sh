#!/usr/bin/env bash
# Installs Hymnal for this user on Linux: ~/.local/share/Hymnal, ~/.local/bin/hymnal and a menu entry.
set -e
cd "$(dirname "$0")"
DEST="$HOME/.local/share/Hymnal"
mkdir -p "$DEST" "$HOME/.local/bin" "$HOME/.local/share/applications"
cp -r build/Hymnal build/assets "$DEST/"
[ -d engine ] && cp -r engine "$DEST/"
printf '#!/bin/sh\nexec "%s/Hymnal" "$@"\n' "$DEST" > "$HOME/.local/bin/hymnal"
chmod +x "$HOME/.local/bin/hymnal"
printf '[Desktop Entry]\nName=Hymnal\nComment=Songs with vocals, made on your own PC\nExec=%s/.local/bin/hymnal %%f\nIcon=%s/assets/logo.png\nType=Application\nCategories=AudioVideo;Audio;\n' "$HOME" "$DEST" > "$HOME/.local/share/applications/hymnal.desktop"
echo "Installed: $DEST (run: hymnal)"
