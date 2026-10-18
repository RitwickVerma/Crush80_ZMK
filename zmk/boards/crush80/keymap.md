# Keymap reference

The default keymap is [`config/crush80.keymap`](../../../config/crush80.keymap).
[`keymaps/example.keymap`](../../../keymaps/example.keymap) is an alternative with
home-row mods, hold-W navigation and a Sym layer (a different layer list, with
its own Caps and Fn behaviour); build it with
`CRUSH80_KEYMAP=keymaps/example.keymap bash build.sh`. Its layer list differs, so
raise `CONFIG_CRUSH80_KEYMAP_REV` (`zmk/Kconfig`) when switching either way, or live edits
saved for the other layout will land on the wrong layers.

## Idea

Base is a plain 80% layout: every key does what it says and Caps is Caps Lock.
There is not a single hold-tap or combo, so nothing is timed, and every layer can be
edited in VIA, because everything used is a plain layer key, a toggle or a key.
Extras sit behind the Fn key.

## Layers

The numbers are what VIA and ZMK Studio show. Live edits are saved by number.

| # | Name | How you get there |
|---|---|---|
| 0 | Base | always on |
| 1 | Mac | Fn+M (toggle) |
| 2 | Vim | Fn+V toggles it: vim's normal mode in any text field (see the table in the README) |
| 3 | Visual | `v` in Vim: motions select, `y d c p` act on the selection |
| 4-6 | OpD, OpY, OpC | `d` `y` `c` in Vim; each lasts one key (a sticky layer) |
| 7 | Fn | hold Fn |
| 8 | MacFn | hold right Win (numpad on the nav cluster) |
| 9 | Num | hold Fn and Space |
| 10 | Macro | hold right Win, then Space; P plays macro 0 |
| 11-13 | spare | empty; VIA lists them, ZMK Studio can fill them |

ZMK looks up a key in the highest active layer first, so the order is deliberate:
Fn above Mac (so Mac cannot hide Fn's keys), MacFn above Fn and Num above MacFn
(each overrides the one below), and spare layers last (ZMK numbers enabled layers
before reserved ones). Change the layer list and you must raise
`CONFIG_CRUSH80_KEYMAP_REV` (`zmk/Kconfig`), which clears saved edits for the old
layout on first boot.

## Fn

```
 Esc  recovery        F12 Studio unlock      PrtSc Mute  ScrLk Vol+  Pause Bri+
 1 2 3 6 7  Bluetooth profiles 1-5    4 USB    5 toggle USB / Bluetooth
 Bspc RGB on/off   - = RGB speed   [ ] hue   \ Enter next effect   Up Down brightness
 Ins Prev   Home Play   PgUp Next       End Vol-   PgDn Bri-      Del clear BT pairing
 B battery   M Mac mode   V Vim mode

        Y PgUp   U Home             O End
        H Left   J Down   K Up      L Right    ; Del      X Bspc
        N PgDn                      Space  hold for Num

        W paragraph up      A word left     S paragraph down     D word right
```

A D W S are Ctrl+Left / Right / Up / Down.
No OS has a sentence keystroke; paragraph is the closest, and it works in browsers,
Word and most text fields (some editors, such as VS Code, use Ctrl+Up / Down to
scroll instead).

F1-F11 are not bound on Fn, so Fn+F-key is the plain F-key. Modifiers pass
through: Shift+Fn+L selects, Shift+Fn+D selects a word, Alt+Fn+H goes back.

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

Only what differs on a Mac: the left Win and Alt swap, and so do right Alt and Super (the right
Win key is left alone: it is the key that holds MacFn, your numpad), giving
Ctrl Opt Cmd Space Cmd in Apple's order. PrtSc is Cmd+Shift+4 (screenshot a
region), Home and End are Cmd+Left and Cmd+Right (start and end of line, as on
Windows and Linux). Layer 8 (right Win) holds the numpad on the nav cluster; its
Space key holds layer 10 (Macro).

Layers stack: ZMK keeps a set of active layers and looks each key up from the highest one
down, with transparent keys falling through, so right Win, then Space, then P reads P from
layer 10. A ZMK *conditional layer* is the one thing to avoid for a layer you hold by hand:
it re-checks its rule on every layer change and switches the layer off whenever the rule
is false, which is what once made right Win dead.

## Changing it

Edit the file and reflash, or edit live in VIA or ZMK Studio (docs/via.md). A key
edited live is stored on the keyboard and then wins over the file until the layout
revision changes or you reset the keymap in ZMK Studio; reflashing alone does not
undo it.

## Your layout changes

The right Win key holds MacFn (layer 8), which in practice is a numpad on the nav
cluster (Ins Home PgUp = 4 5 6, Del End PgDn = 1 2 3, and so on); Space on it holds
the Macro layer. These came from live edits and are now in the file. The Mac layer
used to turn the right Win key into Right Alt, which broke that numpad whenever Mac mode
was on; the Mac layer now leaves the right Win key transparent.
