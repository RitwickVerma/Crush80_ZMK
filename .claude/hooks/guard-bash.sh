#!/usr/bin/env bash
# PreToolUse(Bash) guard: blocks every flashing/recovery command (update.sh, install_zmk.sh, scripts/dfu.py, flash_*.py, force_recovery.py, west flash, dfu-util, ...). Flashing needs the user.
# Reads the hook JSON on stdin; exit 2 (stderr goes to Claude) blocks the command.
set -uo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# shellcheck source=lib-cmd.sh
source "$HERE/lib-cmd.sh"

command -v jq >/dev/null 2>&1 || { echo "guard-bash: jq is missing, command not checked" >&2; exit 1; }
input=$(cat)
cmd=$(printf '%s' "$input" | jq -r '.tool_input.command // empty' 2>/dev/null) || exit 0
[ -n "$cmd" ] || exit 0
cwd=$(printf '%s' "$input" | jq -r '.cwd // empty' 2>/dev/null)
[ -n "$cwd" ] || cwd=$(pwd)

block() {
    {
        echo "BLOCKED: $1"
        echo "  command segment: $2"
        echo "  Build with 'bash build.sh', check with 'bash zmk/tests/run_host_tests.sh'; ask the user to flash."
    } >&2
    exit 2
}

while IFS= read -r seg; do
    [ -n "$seg" ] || continue
    parse_seg "$seg"
    [ -n "$PROG" ] || continue
    case "$PROG" in
    install_zmk.sh | update.sh | dfu.py | flash_combined.py | flash_ota.py | force_recovery.py)
        block "$PROG writes firmware/flash on the keyboard. Flashing needs the user (second keyboard at hand, USB recovery mode)." "$seg"
        ;;
    west)
        has_arg flash debug attach && block "west flash writes the keyboard" "$seg"
        ;;
    dfu-util | openocd | pyocd | picotool | wchisp | tlsr-flash | TelinkBDT | bdt)
        block "$PROG flashes hardware" "$seg"
        ;;
    dd)
        printf '%s' "$seg" | grep -qE 'of=/dev/' && block "dd to a device node" "$seg"
        ;;
    esac
done < <(printf '%s\n' "$cmd" | all_segments)
exit 0
