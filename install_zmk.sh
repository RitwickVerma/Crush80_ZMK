#!/bin/bash
# Install ZMK firmware on a Crush 80 running stock Evision firmware.
#
# Two-stage process (all over USB, no EVK/SWS needed):
#   1. OTA-flash a monolithic "bridge" firmware via the stock OTA protocol
#      (B91 boot ROM dual-bank boot: bridge runs from bank 1 at 0x40000)
#   2. The bridge exposes mcumgr on USB serial; scripts/flash_combined.py writes the
#      full image (MCUboot + ZMK) to flash 0x0 through its flash_mgmt group
#
# Prerequisites:
#   - Keyboard connected via USB, running stock firmware (VID 320f:5055)
#   - Build artifacts in dist/:  bash build.sh
#   - Built images: bash build.sh --build-mcuboot (dist/crush80-ota-bridge.bin, dist/crush80-combined.bin)
#
# Usage:
#   ./install_zmk.sh              # interactive (asks for confirmation)
#   ./install_zmk.sh -y           # non-interactive (skip confirmation)
#   ./install_zmk.sh --bridge path/to/bridge.bin --zmk path/to/combined.bin

set -euo pipefail
cd "$(dirname "$0")"

# ── Colors (disabled when stdout is not a terminal) ──────────
if [[ -t 1 ]]; then
    BOLD='\033[1m' RED='\033[0;31m' GREEN='\033[0;32m'
    YELLOW='\033[1;33m' CYAN='\033[0;36m' NC='\033[0m'
else
    BOLD='' RED='' GREEN='' YELLOW='' CYAN='' NC=''
fi
info()   { echo -e "${CYAN}[INFO]${NC} $*"; }
ok()     { echo -e "${GREEN}[ OK ]${NC} $*"; }
warn()   { echo -e "${YELLOW}[WARN]${NC} $*" >&2; }
err()    { echo -e "${RED}[ERR ]${NC} $*" >&2; }
die()    { err "$@"; exit 1; }
header() { echo -e "\n${BOLD}$*${NC}"; }

# ── Defaults ─────────────────────────────────────────────────
BRIDGE_IMAGE="dist/crush80-ota-bridge.bin"
ZMK_IMAGE="dist/crush80-combined.bin"
STOCK_VID="320f"
STOCK_PID="5055"
ZMK_VID="1d50"
ZMK_PID="615e"
AUTO_YES=0

# Serial port: --port / SERIAL_PORT, else auto-detected (find_serial_port)
SERIAL_PORT="${SERIAL_PORT:-}"

# ── Parse arguments ──────────────────────────────────────────
while [[ $# -gt 0 ]]; do
    case "$1" in
        --bridge)    BRIDGE_IMAGE="$2"; shift 2 ;;
        --zmk)       ZMK_IMAGE="$2"; shift 2 ;;
        --port)      SERIAL_PORT="$2"; shift 2 ;;
        -y|--yes)    AUTO_YES=1; shift ;;
        --help|-h)
            echo "Usage: $0 [-y] [--bridge OTA_IMAGE] [--zmk COMBINED_IMAGE] [--port SERIAL_PORT]"
            echo "  SERIAL_PORT: e.g. /dev/ttyACM0 (Linux) or /dev/cu.usbmodemXXXX (macOS);"
            echo "               default: auto-detect the bridge (USB 1d50:615e)"
            exit 0 ;;
        *) die "Unknown option: $1" ;;
    esac
done

# ── Validate prerequisites ───────────────────────────────────
for f in "$BRIDGE_IMAGE" "$ZMK_IMAGE"; do
    [[ -f "$f" ]] || die "Required file not found: $f (run: bash build.sh --build-mcuboot)"
done


check_usb() {
    local vid="$1" pid="$2"
    if command -v lsusb &>/dev/null; then
        lsusb -d "$vid:$pid" >/dev/null 2>&1
    elif [[ "$(uname)" == "Darwin" ]]; then
        # macOS: system_profiler prints "Product ID: 0x..." and, on the next
        # line, "Vendor ID: 0x...", so match the pair across lines.
        system_profiler SPUSBDataType 2>/dev/null \
            | grep -i -A2 "Product ID: 0x${pid}" | grep -qi "Vendor ID: 0x${vid}"
    else
        return 1
    fi
}

wait_for_usb() {
    local vid="$1" pid="$2" desc="$3" timeout="${4:-30}"
    info "Waiting for $desc..."
    local elapsed=0
    while ! check_usb "$vid" "$pid"; do
        sleep 1
        elapsed=$((elapsed + 1))
        if [[ $elapsed -ge $timeout ]]; then
            die "Timeout waiting for $desc after ${timeout}s"
        fi
    done
    ok "$desc detected (${elapsed}s)"
}

wait_for_serial() {
    local port="$1" timeout="${2:-15}"
    # If port is empty, try to auto-detect
    if [[ -z "$port" ]]; then
        info "Auto-detecting serial port..."
        port="$(find_serial_port)"
        if [[ -z "$port" ]]; then
            info "Waiting for serial device to appear..."
            local elapsed=0
            while [[ -z "$port" ]]; do
                sleep 1
                elapsed=$((elapsed + 1))
                port="$(find_serial_port)"
                if [[ $elapsed -ge $timeout ]]; then
                    die "Timeout: no serial port found after ${timeout}s"
                fi
            done
        fi
        SERIAL_PORT="$port"
    else
        info "Waiting for serial port $port..."
        local elapsed=0
        while [[ ! -e "$port" ]]; do
            sleep 1
            elapsed=$((elapsed + 1))
            if [[ $elapsed -ge $timeout ]]; then
                die "Timeout waiting for $port after ${timeout}s"
            fi
        done
    fi
    sleep 2
    ok "Serial port ready: $SERIAL_PORT"
}

find_serial_port() {
    # Only ever return the bridge's own port: flash commands sent to some other
    # serial device would at best fail and at worst do damage.
    local dev
    if [[ "$(uname)" == "Darwin" ]]; then
        # macOS names ports by location, not ID; accept a single candidate only.
        local ports=(/dev/cu.usbmodem*)
        if [[ ${#ports[@]} -eq 1 && -e "${ports[0]}" ]]; then echo "${ports[0]}"; fi
        return 0
    fi
    for dev in /dev/ttyACM*; do
        [[ -e "$dev" ]] || continue
        local props
        props="$(udevadm info -q property -n "$dev" 2>/dev/null)"
        if grep -qx "ID_VENDOR_ID=${ZMK_VID}" <<<"$props" && grep -qx "ID_MODEL_ID=${ZMK_PID}" <<<"$props"; then
            echo "$dev"
            return 0
        fi
    done
}

# ── Recovery guidance on failure ─────────────────────────────
cleanup_msg() {
    local rc=$?
    [[ $rc -eq 0 ]] && return
    echo ""
    warn "Installation did not complete."
    if check_usb "$ZMK_VID" "$ZMK_PID"; then
        info "Bridge/ZMK firmware is running. You can retry Stage 2:"
        info "  python3 scripts/flash_combined.py $ZMK_IMAGE --port $SERIAL_PORT"
    else
        info "Try unplugging the keyboard, waiting 5s, and re-running this script."
    fi
}
trap cleanup_msg EXIT

# ── Step 0: Check current state ──────────────────────────────
START_TIME=$SECONDS

header "=== Crush 80: Install ZMK ==="

if check_usb "$ZMK_VID" "$ZMK_PID"; then
    info "Keyboard is already running ZMK (or the install bridge)."
    info "  ZMK:    update as in docs/flashing.md (recovery, then bash update.sh)"
    info "  Bridge: finish an interrupted install with:"
    info "          python3 scripts/flash_combined.py $ZMK_IMAGE"
    exit 0
fi

if ! check_usb "$STOCK_VID" "$STOCK_PID"; then
    die "Keyboard not found. Expected stock firmware (${STOCK_VID}:${STOCK_PID}) on USB."
fi

info "Stock firmware detected."
info "  Bridge: $BRIDGE_IMAGE ($(stat -c%s "$BRIDGE_IMAGE" 2>/dev/null || stat -f%z "$BRIDGE_IMAGE") bytes)"
info "  ZMK:    $ZMK_IMAGE ($(stat -c%s "$ZMK_IMAGE" 2>/dev/null || stat -f%z "$ZMK_IMAGE") bytes)"

if [[ $AUTO_YES -eq 0 ]]; then
    echo ""
    info "This will replace the stock firmware with ZMK."
    info "Going back to stock is documented in docs/flashing.md."
    read -rp "Proceed? [y/N] " confirm
    if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
        info "Aborted."
        exit 0
    fi
fi

# ── Stage 1: OTA flash the bridge firmware ───────────────────
header "=== Stage 1/2: OTA flash bridge firmware ==="
python3 scripts/flash_ota.py --force --yes "$BRIDGE_IMAGE"

info "Bridge flashed. Keyboard will reboot..."
wait_for_usb "$ZMK_VID" "$ZMK_PID" "bridge firmware" 30
wait_for_serial "$SERIAL_PORT" 15

# ── Stage 2: write MCUboot + ZMK to flash 0x0 through the bridge ────────
# The bridge has no MCUboot image manager; flash_combined.py stages the image
# at 0x80000 and commits it with the bridge's flash_mgmt trampoline, which
# copies it to 0x0 and resets into MCUboot -> ZMK.
header "=== Stage 2/2: Write MCUboot + ZMK ==="
python3 scripts/flash_combined.py "$ZMK_IMAGE" --port "$SERIAL_PORT"

# The bridge and ZMK share 1d50:615e, so wait for the device to go away first:
# returning on "present" would report success while flash is still being copied.
info "Waiting for the keyboard to restart (do NOT unplug)..."
for _ in $(seq 1 90); do check_usb "$ZMK_VID" "$ZMK_PID" || break; sleep 1; done
wait_for_usb "$ZMK_VID" "$ZMK_PID" "ZMK firmware" 60

ELAPSED=$((SECONDS - START_TIME))
echo ""
ok "Installation complete (${ELAPSED}s)"
info "Keyboard is running ZMK firmware."
info "Update later: docs/flashing.md"
