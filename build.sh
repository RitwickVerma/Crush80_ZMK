#!/bin/bash
# Build all Wobkey Crush 80 ZMK firmware targets.
# Run from repo root: bash build.sh
# Supports macOS and Linux.
#
# Outputs go to dist/ (gitignored):
#   dist/crush80-zmk-app.signed.bin ← updates: bash update.sh (MCUboot recovery)
#   dist/crush80-ota-bridge.bin     ← first install, stage 1 (install_zmk.sh)
#   dist/crush80-combined.bin       ← first install, stage 2 (needs MCUboot)
#   dist/crush80-mcuboot.bin        ← MCUboot alone, for SWS/hardware recovery
#
# Options:
#   bash build.sh --skip-bridge    skip OTA bridge build (faster rebuild)
#   bash build.sh --build-mcuboot  also build MCUboot (needed once, for the
#                                  combined image a first install writes)
#   bash build.sh --skip-mcuboot   skip MCUboot build (the default)

set -euo pipefail

REPO_DIR="$(cd "$(dirname "$0")" && pwd)"
SKIP_BRIDGE=false
SKIP_MCUBOOT=true   # MCUboot: built separately or use pre-built copy
                    # Build explicitly with: bash build.sh --build-mcuboot

for arg in "$@"; do
    case $arg in
        --skip-bridge)    SKIP_BRIDGE=true ;;
        --skip-mcuboot)   SKIP_MCUBOOT=true ;;
        --build-mcuboot)  SKIP_MCUBOOT=false ;;
    esac
done

# ── Pick the keymap ──────────────────────────────────────────────────────────
# Priority:
#   1. CRUSH80_KEYMAP env var (explicit path to a .keymap file)
#   2. Dotfiles directory (~/.config/DOTFILES/keybindings/crush80-zmk/crush80.keymap)
#   3. config/crush80.keymap (the board keymap; also what keymap-editor edits)
# The chosen file is copied into the workspace only; the repo copy is never
# overwritten (copying over it once destroyed uncommitted keymap work).
DOTFILES_ZMK="${CRUSH80_ZMK_CONFIG:-$HOME/.config/DOTFILES/keybindings/crush80-zmk}"
KEYMAP_SRC="$REPO_DIR/config/crush80.keymap"

if [ -n "${CRUSH80_KEYMAP:-}" ] && [ -f "$CRUSH80_KEYMAP" ]; then
    KEYMAP_SRC="$CRUSH80_KEYMAP"
    # If CRUSH80_APP_CONF is also set, use that
    if [ -n "${CRUSH80_APP_CONF:-}" ] && [ -f "$CRUSH80_APP_CONF" ]; then
        cp "$CRUSH80_APP_CONF" "$REPO_DIR/conf/app.conf"
    fi
elif [ -d "$DOTFILES_ZMK" ]; then
    echo "Syncing config from $DOTFILES_ZMK..."
    [ -f "$DOTFILES_ZMK/crush80.keymap" ] && KEYMAP_SRC="$DOTFILES_ZMK/crush80.keymap"
    [ -f "$DOTFILES_ZMK/app.conf" ] && cp "$DOTFILES_ZMK/app.conf" "$REPO_DIR/conf/app.conf"
fi
[ -f "$KEYMAP_SRC" ] || { echo "ERROR: keymap not found: $KEYMAP_SRC" >&2; exit 1; }
echo "Using keymap: $KEYMAP_SRC"

# ── Locate west workspace ────────────────────────────────────────────────────
if [ -f "$REPO_DIR/.workspace_path" ]; then
    # shellcheck source=/dev/null
    source "$REPO_DIR/.workspace_path"
elif [ -d "$HOME/Projects/crush80-workspace/.west" ]; then
    WORKSPACE_DIR="$HOME/Projects/crush80-workspace"
else
    echo "ERROR: West workspace not found. Run: bash setup.sh"
    exit 1
fi

export PATH="$WORKSPACE_DIR/.venv/bin:$HOME/miniforge3/bin:$HOME/.local/bin:$HOME/go/bin:/usr/local/bin:/usr/bin:$PATH"
# West may be installed in Python user site
# NOTE: `python3 -m site --user-base` exits 1 inside a venv on Python 3.14
# (user site-packages disabled) even though it still prints the path, so
# guard it to avoid killing the script under `set -e`.
WEST_USER_BIN="$(python3 -m site --user-base 2>/dev/null)/bin" || true
[[ -d "$WEST_USER_BIN" ]] && export PATH="$WEST_USER_BIN:$PATH"

export ZEPHYR_SDK_INSTALL_DIR="$HOME/zephyr-sdk-0.17.0"

echo "Workspace: $WORKSPACE_DIR"
echo "Repo:      $REPO_DIR"

# ── Sync board files into workspace ──────────────────────────────────────────
echo ""
echo "Syncing board files..."
cp -r "$REPO_DIR/zmk/boards/crush80/"* "$WORKSPACE_DIR/zmk/boards/crush80/"
cp "$KEYMAP_SRC" "$WORKSPACE_DIR/zmk/boards/crush80/crush80.keymap"

# Sync WS2812 LED strip driver + binding
mkdir -p "$WORKSPACE_DIR/zmk/drivers/led_strip"
cp -r "$REPO_DIR/zmk/drivers/led_strip/"* "$WORKSPACE_DIR/zmk/drivers/led_strip/"
mkdir -p "$WORKSPACE_DIR/zmk/dts/bindings/led-strip"
cp -r "$REPO_DIR/zmk/dts/bindings/led-strip/"* "$WORKSPACE_DIR/zmk/dts/bindings/led-strip/"

# Sync platform drivers (usb, bluetooth, sensor, watchdog)
for d in usb bluetooth sensor watchdog; do
    mkdir -p "$WORKSPACE_DIR/zmk/drivers/$d"
    cp -r "$REPO_DIR/zmk/drivers/$d/"* "$WORKSPACE_DIR/zmk/drivers/$d/"
done

# Sync DTS bindings for platform drivers
for d in usb bluetooth sensor watchdog; do
    mkdir -p "$WORKSPACE_DIR/zmk/dts/bindings/$d"
    cp -r "$REPO_DIR/zmk/dts/bindings/$d/"* "$WORKSPACE_DIR/zmk/dts/bindings/$d/"
done

# Sync module root files (CMakeLists.txt, Kconfig, module.yml, src/, include/)
cp "$REPO_DIR/zmk/CMakeLists.txt" "$WORKSPACE_DIR/zmk/"
cp "$REPO_DIR/zmk/Kconfig" "$WORKSPACE_DIR/zmk/"
mkdir -p "$WORKSPACE_DIR/zmk/zephyr"
cp "$REPO_DIR/zmk/zephyr/module.yml" "$WORKSPACE_DIR/zmk/zephyr/"
mkdir -p "$WORKSPACE_DIR/zmk/src" "$WORKSPACE_DIR/zmk/include"
cp "$REPO_DIR/zmk/src/"* "$WORKSPACE_DIR/zmk/src/" 2>/dev/null || true
cp -r "$REPO_DIR/zmk/include/"* "$WORKSPACE_DIR/zmk/include/" 2>/dev/null || true

# Sync rainy_rgb engine (rainy_rgb src + behaviors + dtsi + behavior binding)
mkdir -p "$WORKSPACE_DIR/zmk/src/rainy_rgb" "$WORKSPACE_DIR/zmk/src/behaviors"
cp -r "$REPO_DIR/zmk/src/rainy_rgb/"* "$WORKSPACE_DIR/zmk/src/rainy_rgb/"
cp -r "$REPO_DIR/zmk/src/behaviors/"* "$WORKSPACE_DIR/zmk/src/behaviors/"
mkdir -p "$WORKSPACE_DIR/zmk/dts" "$WORKSPACE_DIR/zmk/dts/bindings/behaviors"
cp "$REPO_DIR/zmk/dts/rainy_rgb.dtsi" "$WORKSPACE_DIR/zmk/dts/"
cp -r "$REPO_DIR/zmk/dts/bindings/behaviors/"* "$WORKSPACE_DIR/zmk/dts/bindings/behaviors/"

# Sync conf files
cp "$REPO_DIR/conf/"* "$WORKSPACE_DIR/conf/"

echo "  Done."

cd "$WORKSPACE_DIR"

CONF="$REPO_DIR/conf/app.conf"
OVERLAY="$WORKSPACE_DIR/conf/app.overlay"

# Crush 80 specific overrides. CONFIG_ZMK_KEYBOARD_NAME repeats app.conf's
# value on purpose: a CRUSH80_APP_CONF without it still gets the right name.
OVERRIDE_CONF="$(mktemp /tmp/crush80_override_XXXXXX)"
cat > "$OVERRIDE_CONF" << 'EOF'
CONFIG_ZMK_KEYBOARD_NAME="Crush 80"
EOF

# ── MCUboot ──────────────────────────────────────────────────────────────────
if [ "$SKIP_MCUBOOT" = false ]; then
    echo ""
    echo "[1/3] Building MCUboot..."
    west build \
        -s bootloader/mcuboot/boot/zephyr \
        -b crush80 \
        -d build-mcuboot \
        --pristine \
        -- \
        -DEXTRA_CONF_FILE="$WORKSPACE_DIR/conf/mcuboot.conf" \
        -DDTC_OVERLAY_FILE="$WORKSPACE_DIR/conf/mcuboot.overlay" \
        -DBOARD_ROOT="$WORKSPACE_DIR/zmk" \
        -DDTS_ROOT="$WORKSPACE_DIR/zmk"
    echo "  MCUboot: OK"
fi

# ── OTA bridge ───────────────────────────────────────────────────────────────
if [ "$SKIP_BRIDGE" = false ]; then
    echo ""
    echo "[2/3] Building OTA bridge..."
    # Bridge uses only ota-bridge.conf — NOT app.conf (which enables MCUboot)
    BRIDGE_OVERRIDE="$(mktemp /tmp/crush80_bridge_XXXXXX)"
    printf 'CONFIG_ZMK_KEYBOARD_NAME="Crush 80 Bridge"\n' > "$BRIDGE_OVERRIDE"
    west build \
        -s zmk-src/app \
        -b crush80 \
        -d build-bridge \
        --pristine \
        -- \
        -DEXTRA_CONF_FILE="$WORKSPACE_DIR/conf/ota-bridge.conf;$BRIDGE_OVERRIDE" \
        -DDTC_OVERLAY_FILE="$WORKSPACE_DIR/conf/bridge.overlay" \
        -DBOARD_ROOT="$WORKSPACE_DIR/zmk" \
        -DDTS_ROOT="$WORKSPACE_DIR/zmk"
    rm -f "$BRIDGE_OVERRIDE"
    echo "  OTA bridge: OK"
fi

# ── ZMK application ──────────────────────────────────────────────────────────
echo ""
echo "[3/3] Building ZMK application..."
west build \
    -s zmk-src/app \
    -b crush80 \
    -d build-crush80 \
    --pristine \
    -- \
    -DEXTRA_CONF_FILE="$CONF;$OVERRIDE_CONF" \
    -DDTC_OVERLAY_FILE="$OVERLAY" \
    -DBOARD_ROOT="$WORKSPACE_DIR/zmk" \
    -DDTS_ROOT="$WORKSPACE_DIR/zmk"
echo "  ZMK app: OK"

rm -f "$OVERRIDE_CONF"

# ── Collect artifacts into dist/ ─────────────────────────────────────────────
echo ""
echo "Collecting artifacts to dist/..."
DIST="$WORKSPACE_DIR/dist"
mkdir -p "$DIST"

rm -f "$DIST"/*.bin
# OTA bridge, wrapped for the stock firmware's OTA handler (Telink header + CRC)
if [ "$SKIP_BRIDGE" = false ]; then
    python3 "$REPO_DIR/scripts/prepare_ota.py" build-bridge/zephyr/zmk.bin \
        -o "$DIST/crush80-ota-bridge.bin"
fi
cp "build-crush80/zephyr/zmk.signed.bin" "$DIST/crush80-zmk-app.signed.bin"
# Full image for flash 0x0 (MCUboot padded to 64 KB, then the signed app):
# what install_zmk.sh writes through the bridge.
if [ -f "build-mcuboot/zephyr/zephyr.bin" ]; then
    cp "build-mcuboot/zephyr/zephyr.bin" "$DIST/crush80-mcuboot.bin"
    python3 - "$DIST" <<'PYEOF'
import sys
d = sys.argv[1]
boot = open("build-mcuboot/zephyr/zephyr.bin", "rb").read()
app = open("build-crush80/zephyr/zmk.signed.bin", "rb").read()
assert len(boot) <= 0x10000, f"MCUboot is {len(boot)} B, larger than its 64 KB partition"
open(f"{d}/crush80-combined.bin", "wb").write(boot + b"\xff" * (0x10000 - len(boot)) + app)
PYEOF
fi

# Also copy to repo dist/ for easy Windows access
mkdir -p "$REPO_DIR/dist"
rm -f "$REPO_DIR/dist/"*.bin
cp "$DIST"/*.bin "$REPO_DIR/dist/" 2>/dev/null || true

echo ""
echo "=============================================="
echo "  Build complete!"
echo ""
ls -1 "$REPO_DIR/dist/"
echo ""
echo "  Update a keyboard running ZMK:  see docs/flashing.md, then  bash update.sh"
echo "  First install from stock:       bash install_zmk.sh"
if [ ! -f "$DIST/crush80-combined.bin" ]; then
    echo "    (needs crush80-combined.bin: run  bash build.sh --build-mcuboot  once)"
fi
echo "=============================================="
