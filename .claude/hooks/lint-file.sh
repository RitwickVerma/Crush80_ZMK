#!/usr/bin/env bash
# PostToolUse(Edit|Write|MultiEdit): no clang-format config exists, so the CI script checks instead: bash -n, a Python syntax parse, and check_keymap.py for a .keymap.
# Only the edited file, only inside this repo. Exit 0 when clean or when the tool is missing;
# exit 2 (output goes to Claude) only on a real error in that file.
set -uo pipefail
command -v jq >/dev/null 2>&1 || exit 0
input=$(cat)
f=$(printf '%s' "$input" | jq -r '.tool_input.file_path // empty' 2>/dev/null)
[ -n "$f" ] && [ -f "$f" ] || exit 0
root="${CLAUDE_PROJECT_DIR:-$(pwd)}"
case "$f" in "$root"/*) ;; *) exit 0 ;; esac
cd "$root" || exit 0
fail() { { echo "$1"; printf '%s\n' "$2"; } >&2; exit 2; }

case "$f" in
*.sh)
    out=$(bash -n "$f" 2>&1) || fail "bash -n: syntax error in $f" "$out"
    ;;
*.py)
    # ast.parse = a syntax check without writing a __pycache__
    out=$(python3 -c 'import ast,sys; ast.parse(open(sys.argv[1]).read(), sys.argv[1])' "$f" 2>&1) || fail "Python syntax error in $f" "$out"
    ;;
*.keymap)
    # check_keymap.py needs the ZMK sources for the built-in behaviour list; skip when absent
    zi="${ZMK_INCLUDE:-$HOME/Projects/crush80-workspace/zmk-src/app/include}"
    [ -d "$zi" ] || exit 0
    out=$(ZMK_INCLUDE="$zi" python3 scripts/check_keymap.py "$f" 2>&1) || fail "check_keymap.py rejected $f" "$out"
    ;;
esac
exit 0
