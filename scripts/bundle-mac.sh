#!/bin/bash
# Bundle an arm64 ashencrown binary into a macOS app with the Qt frameworks it needs.
# Usage: scripts/bundle-mac.sh /path/to/ashencrown /path/to/macos-qt /path/to/AshenCrown.app
set -euo pipefail

BIN="${1:?binary}"
QT="${2:?qt prefix}"
APP="${3:?dest app}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OTOOL="${OTOOL:-llvm-otool}"
INSTALL_NAME_TOOL="${INSTALL_NAME_TOOL:-llvm-install-name-tool}"
LIPO="${LIPO:-llvm-lipo}"
SIGN="${CODESIGN:-${RCODESIGN:-rcodesign}}"
SIGN_NAME="${ASHENCROWN_SIGN_NAME:-TheWhiteWitchTM}"
SIGN_PEM="${ASHENCROWN_SIGN_PEM:-${BLOGY_SIGN_PEM:-$HOME/.config/blogy/codesign/${SIGN_NAME}.pem}}"

if [[ ! -f "$BIN" ]]; then
    echo "ashencrown: missing $BIN" >&2
    exit 1
fi
if [[ ! -d "$QT/lib/cmake/Qt6" || ! -d "$QT/plugins" || ! -d "$QT/qml" ]]; then
    echo "ashencrown: $QT does not look like a macOS Qt" >&2
    exit 1
fi
for tool in "$OTOOL" "$INSTALL_NAME_TOOL" "$LIPO"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "ashencrown: $tool is not on PATH" >&2
        exit 1
    fi
done

VER="$(head -n 1 "$ROOT/VERSION" | tr -d '\r' | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')"
QT_LIB="$(cd "$QT/lib" && pwd)"

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Frameworks" "$APP/Contents/PlugIns" "$APP/Contents/Resources"
cp -a "$BIN" "$APP/Contents/MacOS/ashencrown"
chmod 755 "$APP/Contents/MacOS/ashencrown"

cat > "$APP/Contents/Resources/qt.conf" << 'EOF'
[Paths]
Plugins = PlugIns
QmlImports = Resources/qml
EOF

python3 - "$ROOT/assets/ui/icon.png" "$APP/Contents/Resources/ashencrown.icns" << 'PY'
import struct, sys
from pathlib import Path
src = Path(sys.argv[1])
out = Path(sys.argv[2])
data = src.read_bytes()
if data[:8] != b"\x89PNG\r\n\x1a\n":
    sys.exit("ashencrown: icon is not a png")
chunk = b"ic08" + struct.pack(">I", 8 + len(data)) + data
out.write_bytes(b"icns" + struct.pack(">I", 8 + len(chunk)) + chunk)
PY

cat > "$APP/Contents/Info.plist" << EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleDevelopmentRegion</key>
    <string>en</string>
    <key>CFBundleDisplayName</key>
    <string>Ashen Crown</string>
    <key>CFBundleExecutable</key>
    <string>ashencrown</string>
    <key>CFBundleIconFile</key>
    <string>ashencrown</string>
    <key>CFBundleIdentifier</key>
    <string>tm.thewhitewitch.ashencrown</string>
    <key>CFBundleInfoDictionaryVersion</key>
    <string>6.0</string>
    <key>CFBundleName</key>
    <string>Ashen Crown</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>${VER}</string>
    <key>CFBundleVersion</key>
    <string>${VER}</string>
    <key>LSMinimumSystemVersion</key>
    <string>14.4</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
EOF

add_plugin() {
    local rel="$1"
    local src="$QT/plugins/$rel"
    if [[ -f "$src" ]]; then
        mkdir -p "$APP/Contents/PlugIns/$(dirname "$rel")"
        cp -a "$src" "$APP/Contents/PlugIns/$rel"
    fi
}
add_plugin_dir() {
    local rel="$1"
    local src="$QT/plugins/$rel"
    [[ -d "$src" ]] || return 0
    mkdir -p "$APP/Contents/PlugIns/$rel"
    cp -a "$src"/. "$APP/Contents/PlugIns/$rel/"
}

add_plugin platforms/libqcocoa.dylib
add_plugin platforms/libqoffscreen.dylib
add_plugin platforms/libqminimal.dylib
add_plugin_dir imageformats
add_plugin_dir iconengines
add_plugin_dir tls
add_plugin_dir networkinformation
add_plugin_dir multimedia

ASHENCROWN_SKIP_QML="${ASHENCROWN_SKIP_QML:-QtWebEngine,QtWebChannel}" \
    "$ROOT/scripts/copy-qml.sh" "$QT/qml" "$APP/Contents/Resources/qml" "$ROOT/qml"

thin_arm64() {
    local file="$1"
    local info
    info="$("$LIPO" -info "$file" 2>/dev/null || true)"
    if [[ "$info" == *"Architectures in the fat file"* && "$info" == *"arm64"* ]]; then
        "$LIPO" "$file" -thin arm64 -output "$file.thin"
        mv "$file.thin" "$file"
    fi
}

is_macho() {
    local kind
    kind="$(file -b "$1" 2>/dev/null || true)"
    [[ "$kind" == *"Mach-O"* ]]
}

resolve_qt() {
    local dep="$1"
    case "$dep" in
        /usr/lib/*|/System/*|@executable_path/*|@loader_path/*)
            return 1
            ;;
    esac
    local rel=""
    if [[ "$dep" == @rpath/* ]]; then
        rel="${dep#@rpath/}"
    elif [[ "$dep" == *.framework/* ]]; then
        rel="$(printf '%s\n' "$dep" | sed -n 's/.*\/\([^/]*\.framework\/.*\)/\1/p')"
    elif [[ "$dep" == /* && -e "$dep" ]]; then
        local canon qtcanon
        canon="$(readlink -f "$dep")"
        qtcanon="$(readlink -f "$QT")"
        if [[ "$canon" == "$qtcanon/"* ]]; then
            rel="${canon#"$qtcanon"/lib/}"
            if [[ "$canon" != "$qtcanon/lib/"* ]]; then
                rel="$(basename "$canon")"
            fi
        else
            rel="$(basename "$dep")"
        fi
    else
        rel="$(basename "$dep")"
    fi
    [[ -n "$rel" ]] || return 1
    if [[ -e "$QT_LIB/$rel" ]]; then
        printf '%s\n' "$rel"
        return 0
    fi
    local base
    base="$(basename "$rel")"
    if [[ -e "$QT_LIB/$base" ]]; then
        printf '%s\n' "$base"
        return 0
    fi
    return 1
}

copied_rel=()
has_rel() {
    local rel="$1"
    local have
    for have in "${copied_rel[@]+"${copied_rel[@]}"}"; do
        [[ "$have" == "$rel" ]] && return 0
    done
    return 1
}

copy_rel() {
    local rel="$1"
    if has_rel "$rel"; then
        return 0
    fi
    copied_rel+=("$rel")
    local src="$QT_LIB/$rel"
    if [[ "$rel" == *.framework/* ]]; then
        local fw_name="${rel%%/*}"
        local dest_fw="$APP/Contents/Frameworks/$fw_name"
        if [[ ! -d "$dest_fw" ]]; then
            local src_fw="$QT_LIB/$fw_name"
            mkdir -p "$dest_fw/Versions"
            if [[ -d "$src_fw/Versions/A" ]]; then
                mkdir -p "$dest_fw/Versions/A"
                local item base
                for item in "$src_fw/Versions/A"/*; do
                    base="$(basename "$item")"
                    [[ "$base" == "Headers" ]] && continue
                    cp -a "$item" "$dest_fw/Versions/A/"
                done
                ln -sfn A "$dest_fw/Versions/Current"
                local link
                for link in "$src_fw"/*; do
                    base="$(basename "$link")"
                    [[ "$base" == "Versions" || "$base" == "Headers" ]] && continue
                    if [[ -L "$link" ]]; then
                        ln -sfn "$(readlink "$link")" "$dest_fw/$base"
                    fi
                done
            else
                cp -a "$src_fw" "$dest_fw"
            fi
        fi
    else
        cp -a "$src" "$APP/Contents/Frameworks/$(basename "$rel")"
    fi
}

queue=()
while IFS= read -r macho; do
    queue+=("$macho")
done < <(find "$APP" -type f)

declare -A seen=()
while [[ ${#queue[@]} -gt 0 ]]; do
    file="${queue[0]}"
    queue=("${queue[@]:1}")
    [[ -n "${seen[$file]:-}" ]] && continue
    seen["$file"]=1
    [[ -f "$file" ]] || continue
    is_macho "$file" || continue
    while IFS= read -r dep; do
        [[ -n "$dep" ]] || continue
        rel="$(resolve_qt "$dep" || true)"
        [[ -n "$rel" ]] || continue
        copy_rel "$rel"
        dest=""
        if [[ "$rel" == *.framework/* ]]; then
            dest="$APP/Contents/Frameworks/$rel"
        else
            dest="$APP/Contents/Frameworks/$(basename "$rel")"
        fi
        if [[ -n "$dest" && -z "${seen[$dest]:-}" ]]; then
            queue+=("$dest")
        fi
    done < <("$OTOOL" -L "$file" 2>/dev/null | sed -n 's/^[[:space:]]*\([^[:space:]]*\) (compatibility version.*/\1/p')
done

while IFS= read -r file; do
    is_macho "$file" || continue
    thin_arm64 "$file"
done < <(find "$APP" -type f)

rpaths_of() {
    "$OTOOL" -l "$1" | awk '
        $1 == "cmd" && $2 == "LC_RPATH" { want = 1; next }
        want && $1 == "path" { print $2; want = 0 }
    '
}

while IFS= read -r file; do
    is_macho "$file" || continue
    rel_from_lib=""
    case "$file" in
        "$APP/Contents/Frameworks/"*.framework/*)
            rel_from_lib="${file#"$APP/Contents/Frameworks/"}"
            "$INSTALL_NAME_TOOL" -id "@rpath/$rel_from_lib" "$file"
            ;;
        "$APP/Contents/Frameworks/"*)
            "$INSTALL_NAME_TOOL" -id "@rpath/$(basename "$file")" "$file"
            ;;
    esac

    while IFS= read -r dep; do
        [[ -n "$dep" ]] || continue
        rel="$(resolve_qt "$dep" || true)"
        [[ -n "$rel" ]] || continue
        new="@rpath/$rel"
        if [[ "$dep" != "$new" ]]; then
            "$INSTALL_NAME_TOOL" -change "$dep" "$new" "$file"
        fi
    done < <("$OTOOL" -L "$file" 2>/dev/null | sed -n 's/^[[:space:]]*\([^[:space:]]*\) (compatibility version.*/\1/p')

    fw_dir="$APP/Contents/Frameworks"
    rel_up="$(realpath --relative-to="$(dirname "$file")" "$fw_dir")"
    want="@loader_path/$rel_up"
    have=0
    while IFS= read -r rp; do
        [[ "$rp" == "$want" ]] && have=1
        case "$rp" in
            @*) ;;
            *) "$INSTALL_NAME_TOOL" -delete_rpath "$rp" "$file" || true ;;
        esac
    done < <(rpaths_of "$file")
    if [[ "$have" -eq 0 ]]; then
        "$INSTALL_NAME_TOOL" -add_rpath "$want" "$file"
    fi
done < <(find "$APP" -type f)

if command -v "$SIGN" >/dev/null 2>&1; then
    if [[ ! -f "$SIGN_PEM" ]]; then
        mkdir -p "$(dirname "$SIGN_PEM")"
        "$SIGN" generate-self-signed-certificate \
            --person-name "$SIGN_NAME" \
            --team-id "$SIGN_NAME" \
            --profile developer-id-application \
            --validity-days 3650 \
            --pem-unified-file "$SIGN_PEM"
        chmod 600 "$SIGN_PEM"
    fi
    "$SIGN" sign --pem-file "$SIGN_PEM" "$APP"
else
    echo "ashencrown: $SIGN is not on PATH, so the app is unsigned" >&2
    echo "ashencrown: arm64 macOS will refuse to launch an unsigned binary" >&2
    exit 1
fi

missing=0
need() {
    if ! compgen -G "$APP/$1" >/dev/null; then
        echo "ashencrown: mac bundle is missing $1" >&2
        missing=1
    fi
}
need Contents/MacOS/ashencrown
need Contents/Info.plist
need Contents/Resources/qt.conf
need Contents/Resources/ashencrown.icns
need Contents/Frameworks/QtCore.framework
need Contents/Frameworks/QtGui.framework
need Contents/Frameworks/QtQml.framework
need Contents/Frameworks/QtQuick.framework
need Contents/Frameworks/QtNetwork.framework
need Contents/Frameworks/QtMultimedia.framework
need Contents/Frameworks/QtWebSockets.framework
need Contents/PlugIns/platforms/libqcocoa.dylib
need Contents/PlugIns/imageformats/libqjpeg.dylib
need Contents/PlugIns/tls/libqsecuretransportbackend.dylib
need "Contents/PlugIns/multimedia/*ffmpeg*"
need Contents/Resources/qml/QtQuick/qmldir
need Contents/Resources/qml/QtQuick/Controls/Basic/qmldir
need Contents/Resources/qml/QtQuick/Effects/qmldir

if grep -a -F "$QT_LIB" "$APP/Contents/MacOS/ashencrown" >/dev/null 2>&1; then
    echo "ashencrown: the binary still references $QT_LIB" >&2
    missing=1
fi

if [[ "$missing" -ne 0 ]]; then
    exit 1
fi

echo "ashencrown: wrote $APP ($(du -sh "$APP" | awk '{print $1}'))"
