#!/bin/bash
# Build and run the host test for the VIA keycode translation.
# Needs a C compiler and the ZMK sources (for dt-bindings/zmk/keys.h):
#   ZMK_INCLUDE=path/to/zmk/app/include  (default: the build workspace)
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ZMK_INCLUDE="${ZMK_INCLUDE:-$HOME/Projects/crush80-workspace/zmk-src/app/include}"
BIN="$(mktemp)"
trap 'rm -f "$BIN"' EXIT

cc -std=c11 -Wall -Wextra -Werror -I "$HERE/../src/via" -I "$ZMK_INCLUDE" \
    "$HERE/via_xlat_test.c" "$HERE/../src/via/via_xlat.c" -o "$BIN"
"$BIN"
