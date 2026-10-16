# Keymap Reference

Two keymaps share the same layer set:

- **Board keymap** (`zmk/boards/crush80/crush80.keymap`, the default build): a
  standard 80% layout. Nav, ExtNav, Sym, MacNav and MacExtNav are defined but
  nothing activates them; wire a key to them in the keymap or in ZMK Studio.
- **Home-row-mods keymap** (`keymaps/example.keymap`, build with
  `CRUSH80_KEYMAP=keymaps/example.keymap bash build.sh`): adds the triggers
  below and media keys on the F-row.

---

## Layer Triggers (example.keymap)

| Key | Action |
|-----|--------|
| W (hold) | Nav layer (or MacNav in Mac mode) |
| CapsLock (tap) | CapsLock |
| CapsLock (hold) | Sym layer |
| Apostrophe (hold) | Sym layer |
| Left of spacebar | Shift |
| Right of spacebar | Tab |
| Fn (right bottom) | Fn layer (hold) |
| Fn + M | Toggle Mac mode |
| ScrLk | Toggle Native layer |

---

## Base Layer (example.keymap)

Full Home Row Mods (CAGS order):

```
Left hand:   A=Ctrl  S=Alt  D=Gui  F=Shift
Right hand:  J=Shift K=Gui  L=Alt  ;=Ctrl
```

F-row sends media keys by default. Hold Fn for F1-F12 + BT/RGB controls.

---

## Nav Layer (hold W)

Right-hand IJKL arrow cluster; left-hand editing shortcuts.

### Right hand (arrows + navigation)

```
┌───────┬───────┬───────┬───────┬───────┐
│       │ Bksp  │  ↑    │  Del  │       │
│   y   │   u   │   i   │   o   │   p   │
├───────┼───────┼───────┼───────┼───────┤
│       │  ←    │  ↓    │  →    │S-Enter│
│   h   │   j   │   k   │   l   │   ;   │
├───────┼───────┼───────┼───────┼───────┤
│       │       │SelLn  │SelWrd │       │
│   n   │   m   │   ,   │   .   │   /   │
└───────┴───────┴───────┴───────┴───────┘
```

### Left hand (editing + modifiers)

```
┌───────┬───────┬───────┬───────┬───────┬───────┐
│  Tab  │  Esc  │       │ Home  │  End  │ Shift │
│  tab  │   q   │   w   │   e   │   r   │   t   │
├───────┼───────┼───────┼───────┼───────┼───────┤
│       │ Shift │ Shift │       │       │CapsWd │
│ caps  │   a   │   s   │   d   │   f   │   g   │
├───────┼───────┼───────┼───────┼───────┼───────┤
│ Shift │ Undo  │  Cut  │ Copy  │ Paste │       │
│ lsft  │   z   │   x   │   c   │   v   │   b   │
└───────┴───────┴───────┴───────┴───────┴───────┘
```

### Key summary

| Key | Action |
|-----|--------|
| i | Up |
| j | Left |
| k | Down |
| l | Right |
| u | Backspace |
| o | Delete (forward) |
| , | Select Line |
| . | Select Word |
| ; | Shift+Enter |
| q | Escape |
| e | Home |
| r | End |
| t | Shift |
| a | Shift |
| s | Shift |
| g | Caps Word (auto-deactivates on space/punctuation) |
| z | Undo (Ctrl+Z) |
| x | Cut (Ctrl+X) |
| c | Copy (Ctrl+C) |
| v | Paste (Ctrl+V) |
| Space | Activate ExtNav sub-layer |

---

## ExtNav Layer (hold Space while in Nav)

Word-level movement and page navigation on the same positions.

| Key | Action |
|-----|--------|
| i | Page Up |
| j | Word Left (Ctrl+Left) |
| k | Page Down |
| l | Word Right (Ctrl+Right) |
| u | Word Backspace (Ctrl+Bspc) |
| o | Word Delete (Ctrl+Del) |
| , | Select Line |
| . | Select Word |
| ; | Enter (plain) |
| q | Escape |
| e | Home |
| r | End |
| t | Shift |
| a | Shift |
| s | Shift |
| z | Redo (Ctrl+Shift+Z) |
| x | Cut |
| c | Copy |
| v | Paste |

---

## Sym Layer (hold CapsLock or Apostrophe)

Numbers on the left hand, English punctuation and brackets on the right.

### Left hand: Numbers and Macros

```
┌───────┬───────┬───────┬───────┬───────┬───────┐
│ Space │   4   │   5   │   6   │  ->   │=> {}← │
│  tab  │   q   │   w   │   e   │   r   │   t   │
├───────┼───────┼───────┼───────┼───────┼───────┤
│       │   7   │   8   │   9   │   -   │   +   │
│ caps  │   a   │   s   │   d   │   f   │   g   │
├───────┼───────┼───────┼───────┼───────┼───────┤
│ Shift │ Undo  │   0   │   .   │   \   │   |   │
│ lsft  │   z   │   x   │   c   │   v   │   b   │
└───────┴───────┴───────┴───────┴───────┴───────┘
```

Number row: `1 2 3` remain as-is, followed by `$ % ^ & * ( )` shifted symbols.

### Right hand: Punctuation and Brackets

```
┌───────┬───────┬───────┬───────┬───────┐
│() =>  │   (   │   )   │   [   │   ]   │
│   y   │   u   │   i   │   o   │   p   │
├───────┼───────┼───────┼───────┼───────┤
│   =   │   ,   │   .   │   ?   │   '   │
│   h   │   j   │   k   │   l   │   ;   │
├───────┼───────┼───────┼───────┼───────┤
│   /   │       │       │       │       │
│   n   │   m   │       │       │       │
└───────┴───────┴───────┴───────┴───────┘
```

### Macros

| Key | Output | Use |
|-----|--------|-----|
| r | `->` | C-style pointer/return arrow |
| t | `=> {}` + cursor left | Lambda body (cursor inside braces) |
| y | `() =>` | Arrow function start |

### Special keys in Sym layer

- **Space** sends Shift (allows capitals without releasing the layer key)
- **;** sends Apostrophe (quick access without reaching)
- **'** sends Backspace (right pinky delete)

---

## Fn Layer (hold Fn)

Fn is the top layer, so its shortcuts work in every mode (Mac, Native).

| Key | Board keymap | example.keymap |
|-----|--------------|----------------|
| Esc | Bootloader (known issue: see docs/flashing.md) | same |
| F1-F12 | F1/F2 brightness, F3-F5 prev/play/next, F6-F8 mute/volume, F12 ZMK Studio unlock | actual F1-F12 |
| Pause | — | ZMK Studio unlock |
| ScrLk | Toggle Native | — (plain ScrLk toggles Native) |
| 1 / 2 / 3 | BT profile 1/2/3 | same |
| 4 | USB output | same |
| 5 | USB/BT toggle | same |
| Ins | Clear current BT profile | same |
| Backspace | RGB on/off | same |
| \ or Enter | Next RGB effect | same |
| Up / Down | RGB brightness | same |
| M | Toggle Mac mode | same |

---

## Native Layer

Full passthrough with no remapping, above the Mac layers, so it also bypasses
Mac mode. Board keymap: Fn+ScrLk on, plain ScrLk off. example.keymap: ScrLk
toggles it.

---

## Mac Mode (toggle Fn+M)

**Board keymap:** the Mac layer is transparent except the modifiers, which move
to macOS order: Win becomes Option, Alt becomes Cmd, right Alt becomes right
Cmd (so the row reads Ctrl Opt Cmd Space Cmd).

**example.keymap:** the Mac layer is transparent except W (hold), which
activates **MacNav** instead of Nav. Home-row mods and Sym stay as in Base.

### MacNav Layer (hold W in Mac mode)

Same layout as Nav but with macOS shortcuts:

| Key | Nav (Linux/Win) | MacNav (macOS) |
|-----|-----------------|----------------|
| e | Home | Cmd+Left (line start) |
| r | End | Cmd+Right (line end) |
| z | Ctrl+Z (Undo) | Cmd+Z (Undo) |
| x | Ctrl+X (Cut) | Cmd+X (Cut) |
| c | Ctrl+C (Copy) | Cmd+C (Copy) |
| v | Ctrl+V (Paste) | Cmd+V (Paste) |
| , | Select Line (Home, Shift+End) | Select Line (Cmd+Left, Shift+Cmd+Right) |
| . | Select Word (Ctrl+Left, Shift+Ctrl+Right) | Select Word (Opt+Left, Shift+Opt+Right) |
| Space | ExtNav | MacExtNav |

All other keys (arrows, Bspc, Del, Shift, CapsWord) are unchanged.

### MacExtNav Layer (hold Space in MacNav)

Same layout as ExtNav but with macOS shortcuts:

| Key | ExtNav (Linux/Win) | MacExtNav (macOS) |
|-----|---------------------|-------------------|
| j | Ctrl+Left (word left) | Opt+Left (word left) |
| l | Ctrl+Right (word right) | Opt+Right (word right) |
| u | Ctrl+Bspc (word backspace) | Opt+Bspc (word backspace) |
| o | Ctrl+Del (word delete) | Opt+Del (word delete) |
| e | Home | Cmd+Left (line start) |
| r | End | Cmd+Right (line end) |
| z | Ctrl+Shift+Z (Redo) | Cmd+Shift+Z (Redo) |
| x | Ctrl+X (Cut) | Cmd+X (Cut) |
| c | Ctrl+C (Copy) | Cmd+C (Copy) |
| v | Ctrl+V (Paste) | Cmd+V (Paste) |
| , | Select Line | Select Line (Cmd+Left, Shift+Cmd+Right) |
| . | Select Word | Select Word (Opt+Left, Shift+Opt+Right) |

---

## Home Row Mod Parameters

| Parameter | Value |
|-----------|-------|
| Flavor | Balanced |
| Tapping term | 280ms |
| Quick-tap | 175ms |
| Require prior idle | 150ms |
| Bilateral filtering | Yes (hold-trigger-key-positions) |

---

## Layer Index Summary

| Index | Name | Activation |
|-------|------|------------|
| 0 | Base | Default |
| 1 | Nav | Hold W (example.keymap) |
| 2 | ExtNav | Hold Space (in Nav) |
| 3 | Sym | Hold CapsLock or Apostrophe (example.keymap) |
| 4 | Mac | Toggle Fn+M |
| 5 | MacNav | Hold W (in Mac, example.keymap) |
| 6 | MacExtNav | Hold Space (in MacNav) |
| 7 | Native | Toggle (see above) |
| 8 | Fn | Hold Fn; the top real layer |
| 9-11 | (reserved) | Layers added in ZMK Studio; must stay last |
