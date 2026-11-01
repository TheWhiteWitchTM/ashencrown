#!/bin/bash
cd "$(dirname "$0")"
export LD_LIBRARY_PATH="/home/andreas/Qt/6.12.0/gcc_64/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="/home/andreas/Qt/6.12.0/gcc_64/plugins"
export QML_IMPORT_PATH="/home/andreas/Qt/6.12.0/gcc_64/qml"
export QTWEBENGINEPROCESS_PATH="/home/andreas/Qt/6.12.0/gcc_64/libexec/QtWebEngineProcess"
export DISPLAY="${DISPLAY:-:1}"
exec ./build/ashencrown "$@"
