#!/bin/bash
# Build dist/ so Ashen Crown runs on a machine that does not have this Qt installed.
# The folder carries the binary, the Qt libraries it links, the QML modules, and the plugins.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

BIN="${ASHENCROWN_BIN:-$ROOT/build/ashencrown}"
if [[ ! -x "$BIN" ]]; then
    echo "ashencrown: build the binary first" >&2
    exit 1
fi

if [[ -n "${QT_PREFIX:-}" && -e "$QT_PREFIX/lib/libQt6Core.so" ]]; then
    qt_prefix="$(cd "$QT_PREFIX" && pwd)"
    qt_lib="$qt_prefix/lib"
else
    core="$(ldd "$BIN" | awk '/libQt6Core\.so/{print $3; exit}')"
    if [[ -z "$core" || ! -e "$core" ]]; then
        echo "ashencrown: could not locate libQt6Core for the binary" >&2
        exit 1
    fi
    qt_lib="$(cd "$(dirname "$core")" && pwd)"
    qt_prefix="$(cd "$qt_lib/.." && pwd)"
fi

qt_plugins="$qt_prefix/plugins"
if [[ ! -d "$qt_plugins" ]]; then
    echo "ashencrown: no plugins under $qt_prefix" >&2
    exit 1
fi

rm -rf "$ROOT/dist"
mkdir -p "$ROOT/dist/bin" "$ROOT/dist/lib" "$ROOT/dist/plugins"

cp -a "$BIN" "$ROOT/dist/bin/ashencrown"
cat > "$ROOT/dist/bin/qt.conf" << 'EOF'
[Paths]
Prefix = ..
Plugins = plugins
Libraries = lib
QmlImports = qml
EOF

plugins=()
add_plugin() {
    local rel="$1"
    local src="$qt_plugins/$rel"
    if [[ -f "$src" ]]; then
        mkdir -p "$ROOT/dist/plugins/$(dirname "$rel")"
        cp -a "$src" "$ROOT/dist/plugins/$rel"
        plugins+=("$ROOT/dist/plugins/$rel")
    fi
}
add_plugin_dir() {
    local rel="$1"
    local src="$qt_plugins/$rel"
    [[ -d "$src" ]] || return 0
    mkdir -p "$ROOT/dist/plugins/$rel"
    cp -a "$src"/. "$ROOT/dist/plugins/$rel/"
    local so
    while IFS= read -r so; do
        plugins+=("$so")
    done < <(find "$ROOT/dist/plugins/$rel" -name '*.so' -o -name '*.so.*')
}

add_plugin platforms/libqoffscreen.so
add_plugin platforms/libqminimal.so
add_plugin platforms/libqxcb.so
add_plugin platforms/libqwayland.so
add_plugin platforms/libqwayland-generic.so
add_plugin platforms/libqwayland-egl.so
add_plugin_dir xcbglintegrations
add_plugin_dir wayland-shell-integration
add_plugin_dir wayland-decoration-client
add_plugin_dir wayland-graphics-integration-client
add_plugin_dir platforminputcontexts
add_plugin_dir imageformats
add_plugin_dir iconengines
add_plugin_dir tls
add_plugin_dir networkinformation
add_plugin_dir multimedia

ASHENCROWN_SKIP_QML="${ASHENCROWN_SKIP_QML:-QtWebEngine,QtWebChannel}" \
    "$ROOT/scripts/copy-qml.sh" "$qt_prefix/qml" "$ROOT/dist/qml" "$ROOT/qml"
while IFS= read -r so; do
    plugins+=("$so")
done < <(find "$ROOT/dist/qml" -name '*.so' -o -name '*.so.*')

copy_lib() {
    local src="$1"
    local real base soname
    real="$(readlink -f "$src")"
    base="$(basename "$real")"
    if [[ ! -e "$ROOT/dist/lib/$base" ]]; then
        cp -a "$real" "$ROOT/dist/lib/$base"
    fi
    soname="$(readelf -d "$real" | awk -F'[][]' '/SONAME/{print $2; exit}')"
    if [[ -n "$soname" && ! -e "$ROOT/dist/lib/$soname" ]]; then
        ln -sfn "$base" "$ROOT/dist/lib/$soname"
    fi
}

declare -A seen=()
queue=("$ROOT/dist/bin/ashencrown" "${plugins[@]}")
while [[ ${#queue[@]} -gt 0 ]]; do
    bin="${queue[0]}"
    queue=("${queue[@]:1}")
    [[ -n "${seen[$bin]:-}" ]] && continue
    seen["$bin"]=1
    while IFS= read -r line; do
        [[ -n "$line" ]] || continue
        soname="${line%% *}"
        resolved=""
        if [[ "$line" == *" => /"* ]]; then
            resolved="$(awk '{print $3}' <<< "$line")"
        fi
        if [[ -e "$qt_lib/$soname" ]]; then
            dep="$qt_lib/$soname"
        elif [[ -n "$resolved" && "$resolved" == "$qt_lib"/* ]]; then
            dep="$resolved"
        else
            continue
        fi
        copy_lib "$dep"
        queue+=("$(readlink -f "$dep")")
    done < <(ldd "$bin" || true)
done

cat > "$ROOT/dist/ashencrown" << 'EOF'
#!/bin/sh
# Launch the bundled game. Qt libraries and plugins live next to this script.
set -e
HERE=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ "${QT_QPA_PLATFORM:-}" = "offscreen" ] && [ -n "${WAYLAND_DISPLAY:-}${DISPLAY:-}" ]; then
    unset QT_QPA_PLATFORM
    unset QT_ENABLE_HIGHDPI_SCALING
fi
export LD_LIBRARY_PATH="$HERE/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$HERE/plugins"
export QML_IMPORT_PATH="$HERE/qml"
export QT_QPA_PLATFORM_PLUGIN_PATH="$HERE/plugins/platforms"
exec "$HERE/bin/ashencrown" "$@"
EOF
chmod 0755 "$ROOT/dist/ashencrown" "$ROOT/dist/bin/ashencrown"

missing=0
need() {
    if ! compgen -G "$ROOT/dist/$1" >/dev/null; then
        echo "ashencrown: dist is missing $1" >&2
        missing=1
    fi
}
need lib/libQt6Core.so.6
need lib/libQt6Gui.so.6
need lib/libQt6Qml.so.6
need lib/libQt6Quick.so.6
need lib/libQt6Network.so.6
need lib/libQt6Multimedia.so.6
need lib/libQt6WebSockets.so.6
need plugins/platforms/libqoffscreen.so
need plugins/platforms/libqxcb.so
need plugins/tls/libqopensslbackend.so
need plugins/imageformats/libqjpeg.so
need "plugins/multimedia/*ffmpeg*"
need qml/QtQuick/qmldir
need qml/QtQuick/Controls/Basic/qmldir
need qml/QtQuick/Effects/qmldir
need qml/QtQuick/Layouts/qmldir
if [[ "$missing" -ne 0 ]]; then
    exit 1
fi
if compgen -G "$ROOT/dist/lib/*WebEngine*" >/dev/null; then
    echo "ashencrown: dist pulled in Qt WebEngine" >&2
    exit 1
fi

echo "ashencrown: wrote $ROOT/dist ($(du -sh "$ROOT/dist" | awk '{print $1}'))"
echo "ashencrown: launch with $ROOT/dist/ashencrown"
