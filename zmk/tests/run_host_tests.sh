#!/bin/bash
# Build and run every host test (no Zephyr or hardware needed): the VIA keycode
# translation, the macro player and the keymap revision migration, plus the static
# keymap check (scripts/check_keymap.py). The firmware sources
# are compiled against the small Zephyr stand-ins in mock/ and run under
# AddressSanitizer + UBSan, so an out-of-bounds access fails the test.
# Needs a C compiler and the ZMK sources (for dt-bindings/zmk/keys.h):
#   ZMK_INCLUDE=path/to/zmk/app/include  (default: the build workspace)
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
ZMK_INCLUDE="${ZMK_INCLUDE:-$HOME/Projects/crush80-workspace/zmk-src/app/include}"
BIN="$(mktemp)"
trap 'rm -f "$BIN"' EXIT

CFLAGS=(-std=gnu11 -Wall -Wextra -Wno-sign-compare -Wno-unused-parameter -Werror -g -fsanitize=address,undefined -fno-sanitize-recover=all)

run() { # name, extra sources...
    local name="$1"
    shift
    cc "${CFLAGS[@]}" -I "$HERE/mock" -I "$HERE/../src/via" -I "$ZMK_INCLUDE" -I "$HERE" \
        "$HERE/${name}_test.c" "$@" -o "$BIN"
    "$BIN"
}

run via_xlat "$HERE/../src/via/via_xlat.c"
run via_macro "$HERE/../src/via/via_xlat.c" "$HERE/mock/mock.c"
run keymap_rev "$HERE/mock/mock.c"
ZMK_INCLUDE="$ZMK_INCLUDE" python3 "$HERE/../../scripts/check_keymap.py"
