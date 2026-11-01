#!/bin/bash
# Copy the QML modules the game imports. The scanner is the Linux host tool.
# Usage: scripts/copy-qml.sh /path/to/qt/qml /path/to/dest/qml /path/to/source/qml
set -euo pipefail

QT_QML="${1:?qt qml}"
DEST="${2:?dest qml}"
SRC_ROOT="${3:?source qml}"
SCANNER="${QMLIMPORTSCANNER:-$HOME/Qt/6.12.0/gcc_64/libexec/qmlimportscanner}"
SKIP="${ASHENCROWN_SKIP_QML:-QtWebEngine,QtWebChannel}"

if [[ ! -x "$SCANNER" ]]; then
    echo "ashencrown: qmlimportscanner is not at $SCANNER" >&2
    exit 1
fi
if [[ ! -d "$QT_QML" || ! -d "$SRC_ROOT" ]]; then
    echo "ashencrown: qml paths are missing" >&2
    exit 1
fi

rm -rf "$DEST"
mkdir -p "$DEST"
scan="$(mktemp)"
"$SCANNER" -rootPath "$SRC_ROOT" -importPath "$QT_QML" > "$scan"
python3 - "$QT_QML" "$DEST" "$scan" "$SKIP" << 'PY'
import json, os, shutil, sys
from pathlib import Path

qt_qml = Path(sys.argv[1]).resolve()
dest_root = Path(sys.argv[2]).resolve()
skip = {part.strip() for part in sys.argv[4].split(",") if part.strip()}
mods = json.loads(Path(sys.argv[3]).read_text())

def skipped(name):
    return any(name == part or name.startswith(part + ".") for part in skip)

chosen = []
for mod in mods:
    name = mod.get("name") or ""
    raw = mod.get("path") or ""
    if not name or not raw or skipped(name):
        continue
    path = Path(raw).resolve()
    if not path.is_dir() or qt_qml not in path.parents and path != qt_qml:
        continue
    chosen.append(path)

chosen_set = set(chosen)
for src in chosen:
    rel = src.relative_to(qt_qml)
    dest = dest_root / rel
    dest.mkdir(parents=True, exist_ok=True)
    for child in src.iterdir():
        target = dest / child.name
        if child.is_dir():
            child_res = child.resolve()
            qmldir = child / "qmldir"
            if qmldir.is_file() and child_res not in chosen_set:
                continue
            if qmldir.is_file() and child_res in chosen_set:
                continue
            shutil.copytree(child, target, dirs_exist_ok=True, symlinks=True)
        else:
            if child.is_symlink():
                if target.exists() or target.is_symlink():
                    target.unlink()
                target.symlink_to(os.readlink(child))
            else:
                shutil.copy2(child, target)
print(f"ashencrown: qml modules {len(chosen)}")
PY
rm -f "$scan"
if [[ ! -f "$DEST/QtQuick/qmldir" ]]; then
    echo "ashencrown: qml copy missed QtQuick" >&2
    exit 1
fi
