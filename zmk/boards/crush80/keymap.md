# Keymap reference

The default keymap is [`config/crush80.keymap`](../../../config/crush80.keymap).
[`keymaps/example.keymap`](../../../keymaps/example.keymap) is an alternative with
home-row mods, hold-W navigation and a Sym layer; build it with
`CRUSH80_KEYMAP=keymaps/example.keymap bash build.sh`.

## Idea

Base is a plain 80% layout. Everything else sits behind a held key, and tapping
that key still does what it always did, so nothing about normal typing or shortcuts
changes.

## Layers

The numbers are what VIA and ZMK Studio show. They are fixed, because edits made
there are stored by layer number.

| # | Name | How you get there |
|---|---|---|
| 0 | Base | default |
| 1 | Nav | hold Caps |
| 2, 3 | spare | empty, yours to use |
| 4 | Mac | Fn+M (toggle) |
| 5 | MacNav | hold Caps while Mac is on |
| 6 | NumSym | hold Space while Nav (or MacNav) is on |
| 7 | Native | Fn+N (toggle) |
| 8 | Fn | hold Fn |
| 9-11 | reserved | layers made in ZMK Studio |

Layer 8 must stay the highest real layer, and the reserved ones must stay last.
ZMK picks a key from the highest active layer, so anything above Fn would swallow
Fn's shortcuts, and a real layer placed after a reserved one is never looked up.

## Nav (hold Caps)

```
 Q        W Bspc   E Del    R ^Bspc  T ^Del    Y PgUp  U ^←     I ↑   O ^→
 A ^A     S ^S     D line   F word   G CapsWd  H Home  J ←      K ↓   L →    ; End
 Z ^Z     X ^X     C ^C     V ^V     B ^Y              N PgDn   M back  , fwd
 Space  →  NumSym
```

`^` is Ctrl. Back and forward are Alt+← and Alt+→. D and F are macros that select
the line (Home, Shift+End) or the word (Ctrl+←, Ctrl+Shift+→). Left Shift and the
modifiers are left as they are, so Shift+Caps+L selects text.

The Caps key uses its own hold-tap (`lt_nav`, "balanced"): it becomes a hold as
soon as another key is pressed and released while Caps is down, so Caps+I works at
once, and tapping Caps then typing still just types. VIA shows that key as the
"ZMK" placeholder, because VIA cannot express a custom hold-tap; leave it alone.

## MacNav (hold Caps in Mac mode)

Nav with Cmd instead of Ctrl (undo, cut, copy, paste, select all, save, redo =
Cmd+Shift+Z), Option for word jumps and word delete, Cmd+← / → for Home / End and
Cmd+[ / ] for back and forward.

## Mac (Fn+M)

Only the modifiers move: Win becomes Option, Alt becomes Cmd, and the right-hand
pair the same way, giving Ctrl Opt Cmd Space Cmd Opt. Caps opens MacNav instead of
Nav.

## NumSym (hold Caps, then Space)

```
 1 ->   2 =>{}   3 ()=>                      (number row: code snippets)
 Q ~    W !   E @   R #   T $            U 7  I 8  O 9   P -   [ *  ] /
 A (    S )   D {   F }   G |            J 4  K 5  L 6   ; +   ' =
 Z [    X ]   C <   V >   B \       N 0  M 1  , 2  . 3   / .
```

## Fn (hold Fn)

| Key | Action |
|---|---|
| Esc | bootloader (known issue: see docs/flashing.md) |
| F12 | unlock ZMK Studio |
| PrtSc / ScrLk / Pause | mute / volume up / brightness up |
| Ins / Home / PgUp | previous / play / next |
| End / PgDn | volume down / brightness down |
| 1 2 3 / 6 7 | Bluetooth profiles 1-3 / 4-5 |
| 4 / 5 | USB output / toggle USB and Bluetooth |
| Del | clear the current Bluetooth profile |
| Backspace | RGB on / off |
| \ or Enter | next RGB effect |
| [ ] | RGB hue down / up |
| ; ' | RGB speed down / up |
| ↑ ↓ | RGB brightness up / down |
| B | show battery level |
| M | Mac mode on / off |
| N | Native on / off |

F1-F12 are not bound on Fn, so Fn+F-key is the plain F-key.

## Native (Fn+N)

Every key plain, above the Mac layer, so it also undoes the Mac modifier swap.
Caps is plain Caps Lock here. Fn still works, so Fn+N turns it off.

## Changing it

Edit the file and reflash, or edit live in VIA or ZMK Studio (docs/via.md). The
hold-taps, macros and Caps Word keys show as the "ZMK" placeholder in VIA.

A key edited live is stored on the keyboard and then wins over the file: if you
change `config/crush80.keymap` later, any key you also edited live keeps your edit
until you set it again (or reset the stored keymap in ZMK Studio). Reflashing alone
does not undo live edits.
