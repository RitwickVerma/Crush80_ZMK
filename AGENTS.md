# AGENTS.md

Firmware repo for the Wobkey Crush 80 keyboard (Telink TLSR9511, RISC-V) on ZMK/Zephyr: the board port, drivers, patches and flashing tools. A bad flash can leave a physical keyboard unusable, so most hard rules below are about that.

## Hard rules

1. **Flashing needs the user.** Never run `update.sh`, `install_zmk.sh`, `scripts/dfu.py`, `scripts/flash_combined.py`, `scripts/flash_ota.py`, `scripts/force_recovery.py`, `west flash`, or anything that writes to the keyboard. Build, test and hand over the image path in `dist/`; the user flashes with a second keyboard at hand. (`.claude/hooks/guard-bash.sh` blocks these for Claude Code.)
2. **Never write flash at or above `0xFE000`.** It holds this board's factory RF calibration and Bluetooth MAC, which exist nowhere else. The tools refuse that range; do not add a code path that bypasses it (`docs/hardware.md`, Flash layout).
3. **Never commit** `*.bin`, `*.hex`, `*.elf`, vendor firmware dumps, `dist/`, `zmk/lib/*.a` (the Telink BLE blob is proprietary, fetched by `fetch_ble_blob.sh`), or `zmk/zmk-src/`. `.gitignore` covers them; do not `git add -f` them.
4. **`build.sh` never overwrites `config/crush80.keymap`**; it copies the chosen keymap into the workspace only (overwriting once destroyed uncommitted work). Keep it that way.
5. Raise `CONFIG_CRUSH80_KEYMAP_REV` (`zmk/Kconfig`) whenever the layer list changes (add, remove, reorder, or switching to `keymaps/example.keymap`). It clears VIA/Studio live edits saved for the old layout on first boot; forgetting it puts edits on the wrong layers.

## Build

One-time: `bash setup.sh` (Zephyr SDK 0.17.0 in `~/zephyr-sdk-0.17.0`, west workspace in `~/Projects/crush80-workspace`, Python deps; about 10 min, about 10 GB). udev rule: `udev/99-wobkey-crush80.rules` (needs root, the user installs it).

```bash
bash build.sh                       # app (+ OTA bridge) -> dist/crush80-zmk-app.signed.bin, dist/crush80-ota-bridge.bin
bash build.sh --build-mcuboot       # also MCUboot + combined image (needed once, for a first install)
bash build.sh --skip-bridge         # faster rebuild
CRUSH80_KEYMAP=path/to.keymap bash build.sh    # another keymap, e.g. keymaps/example.keymap
```

- Keymap source priority in `build.sh`: `CRUSH80_KEYMAP`, then `~/.config/DOTFILES/keybindings/crush80-zmk/crush80.keymap` if that dir exists, then `config/crush80.keymap`. Check which one a build used before reporting results.
- `build.sh` regenerates `config/via.json` and `zmk/src/via/via_def.h` through `scripts/gen_via_def.py` from `zmk/boards/crush80/crush80.dts`; if `git status` shows them changed, commit them with the layout change.
- `patches/` fix Zephyr, ZMK, MCUboot and hal_telink; `patches/apply-patches.sh` applies them (called by `setup.sh`). ZMK, hal_telink and MCUboot are pinned by commit in `zmk/west.yml`; bump a pin together with its patches.
- The firmware build itself stays local (SDK is too big for CI).

## Test (no Zephyr SDK, no hardware)

```bash
bash zmk/tests/run_host_tests.sh    # VIA keycode round trip, macro player, keymap-rev migration (ASan+UBSan), then scripts/check_keymap.py
python3 scripts/check_keymap.py [file.keymap]    # static keymap check alone
```

- Needs a C compiler and ZMK's headers: `ZMK_INCLUDE=<zmk>/app/include` (default `~/Projects/crush80-workspace/zmk-src/app/include`, created by `setup.sh`). CI fetches the ZMK commit pinned in `zmk/west.yml` instead.
- CI (`.gitlab-ci.yml`) runs `host-tests` and `scripts` (`bash -n`, `shellcheck --severity=error *.sh patches/*.sh zmk/tests/*.sh`, `python3 -m py_compile scripts/*.py`). Run the same before pushing.
- New firmware logic that can be compiled on the host gets a test under `zmk/tests/` against the stand-ins in `zmk/tests/mock/`.

## Keymap rules (`config/crush80.keymap`)

- 88 bindings per layer, in matrix order; `check_keymap.py` fails on a wrong count.
- Layers: 0 Base, 1 Mac, 2 Fn, 3 MacFn, 4 Num, 5 Macro, 6-8 spare (`status = "reserved"`, must be last). The `#define` layer numbers must match the layer's position. ZMK resolves a key from the highest active layer down, so the order is deliberate (Fn above Mac, MacFn above Fn, Num above MacFn). Details: `zmk/boards/crush80/keymap.md`.
- Base is a plain 80% layout: no hold-taps, no combos; extras sit behind Fn. Combos and hold-tap timings cannot be edited in VIA or Studio, only in the keymap file.
- Every behaviour referenced must be built-in or defined in the file; layer parameters (`&mo`, `&tog`, `&to`, `&sl`) must be a `#define`d layer.
- Timers are accurate (`CONFIG_SYS_CLOCK_TICKS_PER_SEC=8000`); older notes about shortened tapping terms are obsolete.

## Layout

| Path | What |
|---|---|
| `config/` | `crush80.keymap`, generated `via.json` |
| `zmk/` | ZMK module: board DTS (`zmk/boards/crush80/`), drivers (USB, BLE, LED strip, battery, watchdog), `src/` (rainy_rgb, OpenRGB, VIA, flash management), `tests/` |
| `conf/` | Build configs: app, MCUboot, install bridge |
| `patches/` | Zephyr / ZMK / MCUboot / hal_telink patches |
| `keymaps/` | `stock.keymap`, `example.keymap` (home-row mods) |
| `scripts/` | `dfu.py`, `force_recovery.py`, `flash_*.py`, `prepare_ota.py`, `gen_via_def.py`, `check_keymap.py` |
| `docs/` | `flashing.md`, `hardware.md`, `via.md`, `openrgb.md` |

## Flashing and recovery (for the user; the agent only reads this)

- **Stock keyboard**: `bash build.sh --build-mcuboot && bash install_zmk.sh` (stage 1 sends the bridge through the stock updater, stage 2 writes MCUboot + ZMK to flash `0x0`). Do not unplug during stage 2; if interrupted, rerun the command `install_zmk.sh` prints.
- **Update a ZMK keyboard**: enter USB recovery (`ZEPHYR MCUBOOT`, USB `2fe3:0100`) with Fn+Esc, or `python3 scripts/force_recovery.py` over Bluetooth then replug (use the workspace venv: `source ~/Projects/crush80-workspace/.venv/bin/activate`; system pip is refused on Arch). Then `bash update.sh --build` (or `bash update.sh` for `dist/`). About 11 s for 290 KB; MCUboot rejects a bad image and stays in recovery, so retry.
- **Recovery table**: misbehaving but boots, enter recovery and flash a known-good image (`update.sh --firmware ...`); shows as `ZEPHYR MCUBOOT` only, no valid image, flash with `update.sh`; nothing enumerates, hardware path with a Telink Burning EVK on the SWS header (`docs/hardware.md`). `force_recovery.py --backup FILE` first reads the running image over Bluetooth (about 10 min, read-only) so it can be restored.
- Going back to stock has no one-command path (`docs/flashing.md`).

## Conventions

- Conventional commits (`feat(keymap):`, `fix(keymap_rev):`, `revert(...)`, `docs:`); merges to `main` through GitLab MRs.
- Remote is named `gitlab` (default branch `main`); `upstream` is the original port. History is preserved from the fork.
- Shell scripts are bash with `set -euo pipefail`, portable to macOS bash 3.2 (see the `${arr[@]+...}` idiom in `update.sh`). `scripts/dfu.py` is standard library only (keep it that way); the other tools use the workspace venv from `requirements.txt` (`hidapi`, `pyserial`, `cbor2`, `dbus-fast`).
- Docs live with the change: update `README.md` / `docs/` in the same commit when behaviour changes.
- Public repo: no personal data, no vendor firmware, no proprietary blobs.
