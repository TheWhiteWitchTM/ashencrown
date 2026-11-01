#!/bin/bash
# Build the Linux, Windows, and macOS packages for the version in VERSION.
# The macOS app is self-signed as TheWhiteWitchTM, the same way Blogy is.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

VER="$(head -n 1 "$ROOT/VERSION" | tr -d '\r' | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"
SLUG="$(printf '%s' "$VER" | tr '[:upper:]' '[:lower:]' | tr ' _' '-' | tr -cd 'a-z0-9.-')"
TRIPLE="$(printf '%s' "$VER" | grep -oE '^[0-9]+\.[0-9]+\.[0-9]+')"
if [[ -z "$VER" || -z "$SLUG" || -z "$TRIPLE" ]]; then
    echo "ashencrown: VERSION must look like 0.5.2" >&2
    exit 1
fi
if ! grep -qF "$VER" "$ROOT/src/version.h"; then
    echo "ashencrown: src/version.h does not contain '$VER'" >&2
    exit 1
fi

QT_HOST="${QT_HOST:-${QT_PREFIX:-$HOME/Qt/6.12.0/gcc_64}}"
QT_MINGW="${QT_MINGW:-$HOME/Qt/6.12.0/mingw_64}"
QT_MAC="${QT_MAC:-$HOME/Qt/6.12.0/macos}"
export QT_MAC
if [[ ! -e "$QT_HOST/lib/libQt6Core.so" ]]; then
    echo "ashencrown: Qt is not at $QT_HOST" >&2
    exit 1
fi
JOBS="${JOBS:-$(nproc)}"
CACHE="${ASHENCROWN_CACHE:-$HOME/.cache/ashencrown}"
OUT="$ROOT/release"
rm -rf "$OUT"
mkdir -p "$OUT"

"$ROOT/scripts/fetch-release-deps.sh"

pack() {
    local stage="$1"
    local generator="$2"
    local filename="$3"
    local cfg="$OUT/${filename}-CPackConfig.cmake"
    cat > "$cfg" << EOF
set(CPACK_PACKAGE_NAME "ashencrown")
set(CPACK_PACKAGE_VENDOR "The White Witch")
set(CPACK_PACKAGE_VERSION "${TRIPLE}")
set(CPACK_PACKAGE_FILE_NAME "${filename}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Battle chess. Every capture is a fight. ${VER}.")
set(CPACK_PACKAGE_DESCRIPTION "Battle chess. Every capture is a fight. ${VER}.")
set(CPACK_RESOURCE_FILE_LICENSE "${ROOT}/LICENSE")
set(CPACK_GENERATOR "${generator}")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)
set(CPACK_INSTALLED_DIRECTORIES "${stage};/")
set(CPACK_PACKAGE_DIRECTORY "${OUT}")
set(CPACK_VERBATIM_VARIABLES ON)
EOF
    (cd "$OUT" && cpack --config "$cfg")
    rm -f "$cfg"
}

CMAKE_COMMON=(
    -DCMAKE_BUILD_TYPE=Release
    -DASHENCROWN_WITH_WEBENGINE=OFF
)

echo "ashencrown: linux build $VER"
cmake -S "$ROOT" -B "$ROOT/build-linux" \
    "${CMAKE_COMMON[@]}" \
    -DCMAKE_PREFIX_PATH="$QT_HOST"
cmake --build "$ROOT/build-linux" --parallel "$JOBS"
echo "ashencrown: self-test"
QT_QPA_PLATFORM=offscreen "$ROOT/build-linux/ashencrown" --selftest
echo "ashencrown: linux bundle"
ASHENCROWN_BIN="$ROOT/build-linux/ashencrown" QT_PREFIX="$QT_HOST" "$ROOT/scripts/bundle-dist.sh"
cp -a "$ROOT/LICENSE" "$ROOT/dist/LICENSE"
cat > "$ROOT/dist/README.txt" << EOF
Ashen Crown ${VER}
Unpack this folder and run ./ashencrown.
This build is for 64-bit Linux (Pop!_OS and Ubuntu 24.04).
Qt is in this folder. Sound uses the system's audio.
Buy me a coffee opens in your browser.
Settings are saved under ~/.config/AshenCrown.
EOF
echo "ashencrown: bundle self-test"
QT_QPA_PLATFORM=offscreen "$ROOT/dist/ashencrown" --selftest
echo "ashencrown: bundle window check"
QT_QPA_PLATFORM=offscreen "$ROOT/dist/ashencrown" --check
pack "$ROOT/dist" TGZ "ashencrown-${SLUG}-linux-x86_64"

if [[ "${ASHENCROWN_SKIP_WINDOWS:-}" == "1" ]]; then
    echo "ashencrown: skipped the windows build"
else
    if [[ ! -d "$QT_MINGW/lib/cmake/Qt6" ]]; then
        echo "ashencrown: MinGW Qt is not at $QT_MINGW" >&2
        exit 1
    fi
    echo "ashencrown: windows build"
    cmake -S "$ROOT" -B "$ROOT/build-windows" \
        "${CMAKE_COMMON[@]}" \
        -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/mingw-w64.cmake" \
        -DQT_HOST_PATH="$QT_HOST" \
        -DQT_HOST_PATH_CMAKE_DIR="$QT_HOST/lib/cmake" \
        -DCMAKE_PREFIX_PATH="$QT_MINGW" \
        -DOPENSSL_ROOT_DIR="$CACHE/openssl-win"
    cmake --build "$ROOT/build-windows" --parallel "$JOBS"
    echo "ashencrown: windows bundle"
    "$ROOT/scripts/bundle-win.sh" "$ROOT/build-windows/ashencrown.exe" "$QT_MINGW" "$ROOT/build-windows/dist"
    cp -a "$ROOT/LICENSE" "$ROOT/build-windows/dist/LICENSE"
    cat > "$ROOT/build-windows/dist/README.txt" << EOF
Ashen Crown ${VER}
Unzip this folder and run ashencrown.exe.
This build is for 64-bit Windows.
Buy me a coffee opens in your browser.
Settings are saved in %APPDATA%\\AshenCrown.
EOF
    pack "$ROOT/build-windows/dist" ZIP "ashencrown-${SLUG}-windows-x86_64"
fi

if [[ "${ASHENCROWN_SKIP_MAC:-}" == "1" ]]; then
    echo "ashencrown: skipped the mac build"
else
    if [[ ! -d "$QT_MAC/lib/cmake/Qt6" ]]; then
        echo "ashencrown: macOS Qt is not at $QT_MAC" >&2
        exit 1
    fi
    echo "ashencrown: mac build"
    cmake -S "$ROOT" -B "$ROOT/build-mac" \
        "${CMAKE_COMMON[@]}" \
        -DCMAKE_TOOLCHAIN_FILE="$ROOT/cmake/macos-arm64.cmake" \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=14.4 \
        -DQT_HOST_PATH="$QT_HOST" \
        -DQT_HOST_PATH_CMAKE_DIR="$QT_HOST/lib/cmake" \
        -DCMAKE_PREFIX_PATH="$QT_MAC" \
        -DOPENSSL_ROOT_DIR="$CACHE/openssl-mac"
    cmake --build "$ROOT/build-mac" --parallel "$JOBS"
    echo "ashencrown: mac bundle"
    "$ROOT/scripts/bundle-mac.sh" "$ROOT/build-mac/ashencrown" "$QT_MAC" "$ROOT/build-mac/dist/AshenCrown.app"
    cp -a "$ROOT/LICENSE" "$ROOT/build-mac/dist/LICENSE"
    cat > "$ROOT/build-mac/dist/README.txt" << EOF
Ashen Crown ${VER}
Open AshenCrown.app. The first time, Control-click it and choose Open.
If macOS says the app is damaged, run: xattr -dr com.apple.quarantine AshenCrown.app
This build is for Apple Silicon and needs macOS 14.4.
It is self-signed as TheWhiteWitchTM. It is not notarized.
Buy me a coffee opens in your browser.
Settings are saved in ~/Library/Preferences/AshenCrown.
EOF
    mac_name="ashencrown-${SLUG}-macos-arm64"
    mac_tmp="$(mktemp -d)"
    mkdir -p "$mac_tmp/$mac_name"
    cp -a "$ROOT/build-mac/dist"/. "$mac_tmp/$mac_name/"
    rm -f "$OUT/${mac_name}.zip"
    (cd "$mac_tmp" && zip -qry "$OUT/${mac_name}.zip" "$mac_name")
    rm -rf "$mac_tmp"
fi

rm -rf "$OUT/_CPack_Packages"
echo "ashencrown: packages"
ls -lh "$OUT"
