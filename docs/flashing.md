# Flashing, updating and recovery

Everything here works over the keyboard's own USB cable and Bluetooth. No
debugger is needed except for the last-resort option at the end.

> **Flashing third-party firmware can leave your keyboard unusable until it is
> reflashed.** Keep a second keyboard at hand the first time. Never write flash
> at or above `0xFE000`: that holds this board's factory RF calibration and
> Bluetooth MAC, which exist nowhere else. The tools here refuse that range.

## One-time setup

```bash
bash setup.sh                                   # SDK, west workspace, Python deps
sudo cp udev/99-wobkey-crush80.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

The udev rule gives your desktop user access to the keyboard's serial and HID
interfaces (stock, ZMK and bootloader), so no step below needs `sudo`.

## Install ZMK on a stock keyboard

```bash
bash build.sh --build-mcuboot                   # bridge + MCUboot + app -> dist/
bash install_zmk.sh
```

1. **Stage 1** sends a small *bridge* firmware through the stock firmware's own
   update protocol (USB HID). The stock updater performs no signature check.
2. **Stage 2** has the bridge write MCUboot plus ZMK to flash `0x0`
   (`scripts/flash_combined.py`): it stages the image at `0x80000`, then a RAM
   routine copies it into place and resets.

Do not unplug while stage 2 runs. If it is interrupted, the bridge is still
there; rerun the command `install_zmk.sh` prints.

## Update a keyboard that runs ZMK

1. Put the keyboard in USB recovery, where it shows up as `ZEPHYR MCUBOOT`
   (USB `2fe3:0100`):

   ```bash
   # needs dbus-fast and cbor2, which setup.sh puts in the workspace venv
   # (a system-wide pip install is refused on Arch, Debian and Homebrew Pythons):
   source ~/Projects/crush80-workspace/.venv/bin/activate
   python3 scripts/force_recovery.py      # over Bluetooth, then unplug and replug USB
   ```

   **Or press Fn+Esc.** It erases the first sector of the app image (the same thing
   `force_recovery.py` does over Bluetooth), so MCUboot finds no image and stays in
   recovery. The keyboard types nothing until you flash an image. The older approach, a
   boot-mode flag in the last 16 bytes of RAM (`0x9FFF0`), never worked: the flag does
   not survive the chip's reset. It reaches `MCUBOOT` (`2fe3:0100`) in about 6 seconds with no replug. If it ever
   fails, `force_recovery.py` still works but needs the keyboard paired over Bluetooth.
2. Flash:

   ```bash
   bash update.sh --build        # or: bash update.sh   (uses dist/)
   ```

`update.sh` runs `scripts/dfu.py`, which talks to MCUboot directly (Python
standard library only, about 11 s for a 290 KB image), checks the image, and
resets the keyboard into it. MCUboot validates an image before booting it; a bad
image leaves the keyboard in bootloader mode, ready for another attempt.

## Recovery

| Situation | What to do |
|---|---|
| Keyboard boots but misbehaves | Enter recovery (below), then flash a known-good image with `update.sh --firmware ...` |
| Keyboard needs recovery mode | `python3 scripts/force_recovery.py` over Bluetooth (below) |
| Keyboard shows up as `ZEPHYR MCUBOOT` and nothing else | It has no valid image: flash one with `update.sh` |
| Nothing enumerates at all | Hardware path: Telink Burning EVK on the SWS header ([hardware.md](hardware.md)) |

### Entering recovery over Bluetooth

`scripts/force_recovery.py` uses the paired Bluetooth link instead of USB. It
erases one 4 KB sector, the header of the running image, so that MCUboot finds
no app on the next power-up and stays in USB recovery:

```bash
# needs dbus-fast and cbor2, which setup.sh puts in the workspace venv
# (a system-wide pip install is refused on Arch, Debian and Homebrew Pythons):
source ~/Projects/crush80-workspace/.venv/bin/activate
python3 scripts/force_recovery.py --backup my-current-firmware.signed.bin
# unplug and replug USB -> ZEPHYR MCUBOOT appears
python3 scripts/dfu.py dist/crush80-zmk-app.signed.bin
```

`--backup` first reads the running image over Bluetooth (read-only, about 10
minutes), so you can always flash exactly what you had back with `dfu.py`.

Bulk *writes* over Bluetooth are not reliable on this board (the link drops
after a few kilobytes while flash is erased), which is why recovery erases a
single sector and does the real flashing over USB.

## Going back to stock

There is no one-command restore yet. The stock firmware image is not
redistributable (extract it from Wobkey's official updater), and writing it to
flash `0x0` needs a firmware that exposes the `flash_mgmt` group over USB serial,
such as the install bridge. With a Telink Burning EVK the stock image can be
written directly over SWS.
