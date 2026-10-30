#!/usr/bin/env bash
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
appdir="$root/.runtime/squashfs-root"
patch="$root/build/libsnipaste-kwin.so"
[[ -x "$appdir/AppRun" && -f "$patch" ]] || {
    echo "Run scripts/prepare.sh /path/to/Snipaste.AppImage first." >&2
    exit 1
}
if pgrep -u "$(id -u)" -x Snipaste > /dev/null; then
    echo "Snipaste is already running. Quit it from the tray before loading the patch." >&2
    exit 1
fi

# Preload only after extraction: the AppImage runtime itself has no bundled Qt path.
export APPDIR="$appdir"
export APPIMAGE
APPIMAGE="$(realpath -e -- "$root/.runtime/source.AppImage")"
# This AppImage uses xcb under KDE Wayland; select its bundled Fcitx5 plugin.
export QT_IM_MODULE=fcitx
export LD_LIBRARY_PATH="$appdir/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export LD_PRELOAD="$patch${LD_PRELOAD:+:$LD_PRELOAD}"
exec "$appdir/AppRun" "$@"
