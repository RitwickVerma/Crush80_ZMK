# OpenRGB

The firmware speaks the [QMK OpenRGB protocol](https://github.com/Kasper24/QMK-OpenRGB)
(revision 0x0C) on a raw HID interface, so [OpenRGB](https://openrgb.org) 1.0
controls all 154 LEDs individually, alongside the rest of your desktop lighting.

## Setup

OpenRGB only probes QMK keyboards you list, by USB ID. Once:

1. Install the udev rule (see [flashing.md](flashing.md#one-time-setup)), so
   OpenRGB can open the keyboard without root.
2. Add the keyboard to OpenRGB's QMK device list (in its settings), with name
   `Crush 80`, USB VID `1d50`, USB PID `615e`. Or add it to
   `~/.config/OpenRGB/OpenRGB.json` while OpenRGB is closed:

   ```json
   "QMKOpenRGBDevices": {
       "devices": [ { "name": "Crush 80", "usb_vid": "1d50", "usb_pid": "615e" } ]
   }
   ```
3. Restart OpenRGB or **Rescan Devices**. "Crush 80" appears with 154 LEDs.

From the command line:

```bash
openrgb --list-devices                         # find its index
openrgb -d "Crush 80" -m Direct -c 00FF00      # all keys green
```

## Modes

| OpenRGB mode | Firmware effect |
|---|---|
| Direct | Per-LED colour from OpenRGB (Caps Lock and battery indicators still draw on top) |
| Static | Solid colour |
| Cycle All / Cycle Left Right | Rainbow / wave |
| Rainbow Beacon | Comet |
| Raindrops / Jellybean Raindrops | Rain / twinkle |
| Typing Heatmap | Heatmap |
| Solid Reactive | Reactive pulse |
| Rainbow Reactive Splash | Ripple |

Pressing one of the keyboard's own RGB controls (Fn+Backspace, Fn+\, Fn+↑/↓)
leaves Direct mode and hands the LEDs back to the built-in effects.

## Why it needs USB interface 1

OpenRGB's QMK detector only looks at USB interface 1, where QMK keyboards put
their raw HID interface. The firmware therefore puts the keyboard on interface
0 and OpenRGB on interface 1; the only interface after them is the VIA raw HID
(2, see [via.md](via.md)). That is also why ZMK Studio runs over Bluetooth
rather than a USB serial port: an extra USB interface in front would push the
raw HID off interface 1 and OpenRGB would no longer find it.
