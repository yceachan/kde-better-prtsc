#!/usr/bin/env bash
set -euo pipefail

root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ $# -ne 1 ]]; then
    echo "Usage: $0 /absolute/path/to/Snipaste.AppImage" >&2
    exit 2
fi
image="$(realpath -e -- "$1")"
[[ -x "$image" ]] || { echo "AppImage must be executable: $image" >&2; exit 1; }
[[ "$(uname -m)" == x86_64 ]] || { echo "Only x86_64 is supported." >&2; exit 1; }

sdk="${XDG_CACHE_HOME:-$HOME/.cache}/kde-better-prtsc/qt624"
headers="$sdk/usr/include/x86_64-linux-gnu/qt6"
if [[ ! -f "$headers/QtCore/qconfig.h" ]]; then
    mkdir -p -- "$(dirname -- "$sdk")"
    stage="$(mktemp -d "$(dirname -- "$sdk")/headers.XXXXXX")"
    trap 'rm -rf -- "$stage"' EXIT
    curl -fL --retry 2 \
        'https://archive.ubuntu.com/ubuntu/pool/universe/q/qt6-base/qt6-base-dev_6.2.4+dfsg-2ubuntu1.1_amd64.deb' \
        -o "$stage/headers.deb"
    printf '%s  %s\n' \
        c52261b95152064a18a687338a2fc800c00454209876c35806492944865d3269 \
        "$stage/headers.deb" | sha256sum -c -
    dpkg-deb -x "$stage/headers.deb" "$stage/sdk"
    mv -- "$stage/sdk" "$sdk"
    rm -rf -- "$stage"
    trap - EXIT
fi

runtime="$root/.runtime"
mkdir -p -- "$runtime"
if [[ -d "$runtime/squashfs-root" ]]; then
    (cd -- "$runtime" && sha256sum --check --status appimage.sha256) || {
        echo "Prepared AppImage changed; remove .runtime and run prepare again." >&2
        exit 1
    }
    [[ "$(readlink -f -- "$runtime/source.AppImage")" == "$image" ]] || {
        echo "A different AppImage is already prepared; remove .runtime first." >&2
        exit 1
    }
else
    stage="$(mktemp -d "$runtime/extract.XXXXXX")"
    trap 'rm -rf -- "$stage"' EXIT
    (cd -- "$stage" && "$image" --appimage-extract > /dev/null)
    [[ -x "$stage/squashfs-root/AppRun" ]] || {
        echo "AppImage did not contain an executable AppRun." >&2
        exit 1
    }
    mv -- "$stage/squashfs-root" "$runtime/squashfs-root"
    ln -s -- "$image" "$runtime/source.AppImage"
    (cd -- "$runtime" && sha256sum source.AppImage > appimage.sha256)
    rm -rf -- "$stage"
    trap - EXIT
fi

cmake -S "$root" -B "$root/build" -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DSNIPASTE_APPDIR="$runtime/squashfs-root" -DQT624_HEADERS="$headers"
cmake --build "$root/build"
ctest --test-dir "$root/build" --output-on-failure
echo "Ready. After quitting the existing Snipaste, run: $root/scripts/run-snipaste.sh"
