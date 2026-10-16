# Editing the keymap with VIA

The firmware speaks the [VIA](https://www.caniusevia.com) protocol (version 12)
on a third USB interface, so the VIA configurator can edit the keymap while the
keyboard is plugged in. It edits the same keymap ZMK Studio edits, so changes
from either show up in the other and survive reboots.

Use [usevia.app](https://usevia.app) (Chrome or Edge, USB cable) or the VIA
desktop app. Bluetooth is not supported: VIA only talks over USB.

## One-time setup: load the definition

VIA does not know this keyboard, so it needs the definition file once:

1. Open usevia.app, go to **Settings** and turn on **Show Design tab**.
2. Open the **Design** tab, choose **Load Draft Definition**, and select
   [`config/via.json`](../config/via.json).
3. Connect the keyboard (**Authorize new device**, pick "Crush 80").

The file is generated from the board's devicetree by `scripts/gen_via_def.py`
(`build.sh` runs it), so it always matches the firmware. It has been checked
with VIA's own definition validator (`@the-via/reader`).

## What works

- **Keys and layers**: read and write every key on all 12 layer slots (the nine
  in the keymap and the three ZMK Studio can fill). Basic keys, modifier combos,
  media keys, `MO`, `TG`, `TO` and `LT(layer, key)`.
- **ZMK-only keys**: Bluetooth profiles 1-5, clear profile, USB / Bluetooth
  output, RGB controls, battery level and Studio unlock appear as VIA custom
  keycodes (`BT1`, `OUT`, `RGBEFF`, ...) in the **Special** tab. The list is
  `zmk/src/via/custom_keycodes.def`.
- **Macros**: sixteen, `M0`-`M15`, edited in VIA's *Macros* tab and assigned like
  any key. They hold 1 KB in total, are saved to flash, and play text (US
  layout), single taps, key down/up and delays, with the timing of ZMK's own
  macros (30 ms taps, 15 ms between actions). A macro already playing is not
  restarted; saving macros stops it and releases any held key. ZMK Studio shows
  them as the *VIA Macro* behavior with a number.
- **Key tester**, once enabled (below).

Edits apply at once and are saved to flash five seconds after the last one.

## What does not

Lighting menus and encoders are not available. Lighting is controlled with the
Fn keys and OpenRGB; combos, tap dance and hold-taps stay in
`config/crush80.keymap`. Macros are plain key sequences: they cannot run other
behaviors (layer changes, Bluetooth) or branch.

A key that uses a ZMK behavior VIA cannot express (home-row mods, custom
hold-taps, `&sk`, ...) shows as the **ZMK** placeholder key. Leave it alone and
it keeps working; set it to something else and the old binding is replaced.
Codes VIA offers that ZMK has no equivalent for are ignored, so after saving
check the key still reads what you set.

There is no bootloader keycode: use `scripts/force_recovery.py`.

## Key tester

The tester shows which keys are down, so any program that can open the
keyboard's raw HID device could read what you type. It is therefore off until
you hold **Esc and Enter together for 3 seconds**; it then stays on for
10 minutes.

## Two entries in the device list

OpenRGB's raw HID interface (USB interface 1) must carry the same identification
as VIA's (usage page 0xFF60, usage 0x61; OpenRGB's detector requires it), so VIA
lists "Crush 80" twice. Both work: the OpenRGB interface answers VIA's 32-byte
packets exactly like the VIA interface, and answers only OpenRGB's 64-byte
packets as OpenRGB. (Refusing VIA on that interface does not help: the VIA app
then mixes up its device list and fails to load the other entry.) VIA decides by
packet length, so this assumes the browser sends VIA's packets at their real
32-byte size; Chrome on Linux does.

## How it fits together

| File | |
|---|---|
| `zmk/src/via/via.c` | USB interface, VIA commands, key tester, keymap access |
| `zmk/src/via/via_xlat.c` | VIA keycode <-> ZMK binding, ASCII to key |
| `zmk/src/via/via_macro.c` | macro buffer, flash storage, the `&vmacro` behavior |
| `scripts/gen_via_def.py` | builds `config/via.json` and `via_def.h` from `crush80.dts` |
| `patches/zephyr/0013-...` | third raw HID instance in Zephyr's USB HID class |

### Tests

```bash
zmk/tests/run_via_xlat_test.sh
```

round-trips every VIA keycode the firmware accepts through the translation.
After flashing, the protocol can be exercised against the real keyboard with
any HID client; the commands are the ones in
[qmk_firmware](https://github.com/qmk/qmk_firmware) (`quantum/via.c`).
