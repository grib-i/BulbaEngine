#!/usr/bin/env bash

set -euo pipefail

SEARCH_ROOTS=(
    /usr/local
    /usr/include
    /usr/lib
    /lib
    /opt
    /usr/share
)

ROOTS=()

for root in "${SEARCH_ROOTS[@]}"; do
    [[ -d "$root" ]] && ROOTS+=("$root")
done

if ((${#ROOTS[@]} == 0)); then
    echo "[INFO] No installation directories found."
    exit 0
fi

echo "[INFO] Searching for Bulba Engine files..."

mapfile -d '' -t MATCHES < <(
    sudo find "${ROOTS[@]}" \
        \( -type f -o -type l \) \
        \( -iname '*bulba_engine*' \
            -o -path '/usr/local/include/bulba/*' \
            -o -path '/usr/include/bulba/*' \) \
        -print0 2>/dev/null |
        sort -zu
)

if ((${#MATCHES[@]} == 0)); then
    echo "[INFO] No Bulba Engine files found."
    exit 0
fi

echo
echo "The following files were found:"
printf '  %s\n' "${MATCHES[@]}"

echo
echo "Found ${#MATCHES[@]} file(s)."

read -r -p "Do you want to remove these files? [y/N] " answer

case "$answer" in
    [yY]|[yY][eE][sS])
        ;;
    *)
        echo "[INFO] Uninstallation cancelled."
        exit 0
        ;;
esac

echo
echo "[INFO] Removing Bulba Engine files..."

sudo rm -f -- "${MATCHES[@]}"
sudo ldconfig

echo "[SUCCESS] Bulba Engine uninstalled successfully."
