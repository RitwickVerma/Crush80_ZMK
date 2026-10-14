#!/bin/bash
# Update a Crush 80 that already runs ZMK.
#
#   1. Put the keyboard in bootloader mode: Fn+Esc (it re-enumerates as MCUBOOT).
#   2. bash update.sh            # flash dist/crush80-zmk-app.signed.bin
#      bash update.sh --build    # build first, then flash
#      bash update.sh --firmware path/to/image.signed.bin
#
# The upload goes to MCUboot's serial recovery (scripts/dfu.py, standard library
# only). The bootloader checks the image before booting it; if the image is bad
# the keyboard simply stays in bootloader mode and you can flash again.
set -euo pipefail

REPO_DIR="$(cd "$(dirname "$0")" && pwd)"
FIRMWARE="${CRUSH80_FIRMWARE:-$REPO_DIR/dist/crush80-zmk-app.signed.bin}"
DO_BUILD=false

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build)      DO_BUILD=true; shift ;;
        --firmware)   FIRMWARE="$2"; shift 2 ;;
        --firmware=*) FIRMWARE="${1#--firmware=}"; shift ;;
        -h|--help)    sed -n '2,11p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *)            echo "Unknown option: $1" >&2; exit 1 ;;
    esac
done

if [[ "$DO_BUILD" == true ]]; then
    bash "$REPO_DIR/build.sh" --skip-bridge
fi

[[ -f "$FIRMWARE" ]] || { echo "Firmware not found: $FIRMWARE (run: bash build.sh)" >&2; exit 1; }

exec python3 "$REPO_DIR/scripts/dfu.py" "$FIRMWARE"
