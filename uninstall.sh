#!/usr/bin/env bash

set -euo pipefail

LIB_DIR="/usr/local/lib"
LIB_PATTERN="$LIB_DIR/libbulba_engine.so*"

shopt -s nullglob
LIB_FILES=( $LIB_PATTERN )

if ((${#LIB_FILES[@]} == 0)); then
    echo "Library not found in $LIB_DIR; nothing to uninstall."
    exit 0
fi

echo "Removing Bulba Engine library:"
printf '  %s\n' "${LIB_FILES[@]}"

sudo rm -f -- "${LIB_FILES[@]}"
sudo ldconfig

echo "Bulba Engine library uninstalled successfully."
