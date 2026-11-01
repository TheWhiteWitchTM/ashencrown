#!/bin/bash
# Static OpenSSL 3 for the Windows and macOS builds, and Qt Multimedia for both.
# Linux uses the system libcrypto and the Qt already under ~/Qt/6.12.0/gcc_64.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
CACHE="${ASHENCROWN_CACHE:-$HOME/.cache/ashencrown}"
QT_WIN="${QT_MINGW:-$HOME/Qt/6.12.0/mingw_64}"
QT_MAC="${QT_MAC:-$HOME/Qt/6.12.0/macos}"
SEVEN="${SEVEN:-$HOME/.local/bin/7zz}"
OSS_VER="${OSS_VER:-3.5.4}"
mkdir -p "$CACHE"

log() { echo "ashencrown: $*"; }

if [[ ! -x "$SEVEN" ]]; then
    echo "ashencrown: 7zz is not at $SEVEN" >&2
    exit 1
fi

oss_tgz="$CACHE/openssl-${OSS_VER}.tar.gz"
oss_src="$CACHE/openssl-${OSS_VER}"
if [[ ! -f "$oss_tgz" ]]; then
    log "download OpenSSL ${OSS_VER}"
    curl -fL --retry 3 -o "$oss_tgz.part" \
        "https://github.com/openssl/openssl/releases/download/openssl-${OSS_VER}/openssl-${OSS_VER}.tar.gz"
    mv "$oss_tgz.part" "$oss_tgz"
fi
if [[ ! -d "$oss_src" ]]; then
    tar -xzf "$oss_tgz" -C "$CACHE"
fi

build_openssl() {
    local prefix="$1"
    shift
    if [[ -f "$prefix/lib/libcrypto.a" ]]; then
        log "openssl ready at $prefix"
        return 0
    fi
    local work
    work="$(mktemp -d "$CACHE/openssl-build.XXXXXX")"
    log "build OpenSSL in $prefix"
    (
        cd "$work"
        "$oss_src/Configure" "$@" --prefix="$prefix" --libdir=lib no-shared no-tests
        make -j"$(nproc)"
        make install_sw
    )
    rm -rf "$work"
}

build_openssl "$CACHE/openssl-win" mingw64 --cross-compile-prefix=x86_64-w64-mingw32-

if [[ ! -f "$CACHE/openssl-mac/lib/libcrypto.a" ]]; then
    log "build OpenSSL for macOS arm64"
    work="$(mktemp -d "$CACHE/openssl-build.XXXXXX")"
    (
        cd "$work"
        export PATH="$HOME/.local/share/xmac-sdk/bin:$PATH"
        export MACOSX_DEPLOYMENT_TARGET=14.4
        export CC=arm64-apple-darwin-cc
        export CXX=arm64-apple-darwin-c++
        export AR="${AR:-llvm-ar}"
        export RANLIB="${RANLIB:-llvm-ranlib}"
        "$oss_src/Configure" darwin64-arm64-cc no-shared no-tests \
            --prefix="$CACHE/openssl-mac" --libdir=lib \
            -mmacosx-version-min=14.4
        make -j"$(nproc)"
        make install_sw
    )
    rm -rf "$work"
else
    log "openssl ready at $CACHE/openssl-mac"
fi

fetch_qt() {
    local url="$1"
    local destqt="$2"
    local marker="$destqt/lib/cmake/Qt6Multimedia/Qt6MultimediaConfig.cmake"
    if [[ -f "$marker" ]]; then
        log "Qt Multimedia already in $destqt"
        return 0
    fi
    local name arc work found prefix
    name="$(basename "$url")"
    arc="$CACHE/$name"
    if [[ ! -f "$arc" ]]; then
        log "download $name"
        curl -fL --retry 3 -o "$arc.part" "$url"
        mv "$arc.part" "$arc"
    fi
    work="$(mktemp -d)"
    "$SEVEN" x -y "-o$work" "$arc" >/dev/null
    found="$(find "$work" -name Qt6MultimediaConfig.cmake | head -n 1)"
    if [[ -z "$found" ]]; then
        echo "ashencrown: Qt Multimedia was not in $name" >&2
        exit 1
    fi
    prefix="$(cd "$(dirname "$found")/../../.." && pwd)"
    cp -a "$prefix"/. "$destqt"/
    rm -rf "$work"
    if [[ ! -f "$marker" ]]; then
        echo "ashencrown: Qt Multimedia did not land in $destqt" >&2
        exit 1
    fi
    log "installed Qt Multimedia into $destqt"
}

fetch_qt \
    "https://download.qt.io/online/qtsdkrepository/windows_x86/desktop/qt6_6120/qt6_6120_mingw/qt.qt6.6120.addons.qtmultimedia.win64_mingw/6.12.0-0-202609280346qtmultimedia-Windows-Windows_11_24H2-Mingw-Windows-Windows_11_24H2-X86_64.7z" \
    "$QT_WIN"
fetch_qt \
    "https://download.qt.io/online/qtsdkrepository/mac_x64/desktop/qt6_6120/qt6_6120/qt.qt6.6120.addons.qtmultimedia.clang_64/6.12.0-0-202609280346qtmultimedia-MacOS-MacOS_26-Clang-MacOS-MacOS_26-X86_64-ARM64.7z" \
    "$QT_MAC"
