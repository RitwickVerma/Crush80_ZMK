# Keymap reference

The default keymap is [`config/crush80.keymap`](../../../config/crush80.keymap).
[`keymaps/example.keymap`](../../../keymaps/example.keymap) is an alternative with
home-row mods, hold-W navigation and a Sym layer (a different layer list, with
its own Caps and Fn behaviour); build it with
`CRUSH80_KEYMAP=keymaps/example.keymap bash build.sh`.

## Idea

Base is a plain 80% layout: every key does what it says and Caps is Caps Lock.
Nothing waits on a timer (there is not a single hold-tap), and every layer can be
edited in VIA, because everything used is a plain layer key, a toggle or a key.
Extras sit behind the Fn key.

## Layers

The numbers are what VIA and ZMK Studio show. Live edits are saved by number.

| # | Name | How you get there |
|---|---|---|
| 0 | Base | always on |
| 1 | Mac | Fn+M (toggle) |
| 2 | Fn | hold Fn |
| 3 | MacFn | automatic: Mac and Fn both on |
| 4 | Num | hold Fn and Space |
| 5-7 | spare | empty; VIA lists them, ZMK Studio can fill them |

ZMK looks up a key in the highest active layer first, so the order is deliberate:
Fn above Mac (so Mac cannot hide Fn's keys), MacFn above Fn and Num above MacFn
(each overrides the one below), and spare layers last (ZMK numbers enabled layers
before reserved ones). Change the layer list and you must raise
`CONFIG_CRUSH80_KEYMAP_REV` (`zmk/Kconfig`), which clears saved edits for the old
layout on first boot.

## Fn

```
 Esc  bootloader      F12 Studio unlock      PrtSc Mute  ScrLk Vol+  Pause Bri+
 1 2 3 6 7  Bluetooth profiles 1-5    4 USB    5 toggle USB / Bluetooth
 Bspc RGB on/off   - = RGB speed   [ ] hue   \ Enter next effect   Up Down brightness
 Ins Prev   Home Play   PgUp Next       End Vol-   PgDn Bri-      Del clear BT pairing
 B battery   M Mac mode

        Y PgUp   U Home   I Up      O End
        H Bspc   J Left   K Down    L Right    ; Del
        N PgDn                      Space  hold for Num
```

F1-F11 are not bound on Fn, so Fn+F-key is the plain F-key. Modifiers pass
through: Shift+Fn+L selects, Ctrl+Fn+J jumps a word, Alt+Fn+J goes back.

The keys light up while you hold Fn. Every bound key is white; the active Bluetooth
profile is blue (orange while it waits for a host), the USB key is green when USB is
the output, and the Mac key is orange when Mac mode is on. The lights read the live
keymap, so they follow your edits.

## Num (hold Fn and Space)

```
 Q ~    W !   E @   R #   T $            U 7  I 8  O 9   P -   [ *  ] /
 A (    S )   D {   F }   G |            J 4  K 5  L 6   ; +   ' =
 Z [    X ]   C <   V >   B \       N 0  M 1  , 2  . 3   / .         Enter = Enter
```

## Mac (Fn+M) and MacFn

Only what differs on a Mac: Win and Alt swap, and so do right Alt and Super, giving
Ctrl Opt Cmd Space Cmd Opt in Apple's order. PrtSc is Cmd+Shift+4 (screenshot a
region), Home and End are Cmd+Left and Cmd+Right (start and end of line, as on
Windows and Linux). MacFn does the same for Fn+U and Fn+O. MacFn is a ZMK
conditional layer: it switches on by itself when Mac and Fn are both active.

## Both Shift keys: Caps Word

A combo (`caps_word_combo` in the keymap), not a layer: pressing both Shift keys
within 40 ms switches Caps Word on, which capitalises until the next space or
punctuation. It cannot be edited in VIA.

## Changing it

Edit the file and reflash, or edit live in VIA or ZMK Studio (docs/via.md). A key
edited live is stored on the keyboard and then wins over the file until the layout
revision changes or you reset the keymap in ZMK Studio; reflashing alone does not
undo it.
