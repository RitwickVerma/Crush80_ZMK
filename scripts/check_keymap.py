#!/usr/bin/env python3
"""Static checks for config/crush80.keymap (and any keymap given as an argument).

Catches the mistakes that otherwise surface as a build error or, worse, a silently wrong
keyboard: a layer with the wrong number of keys, a #define layer number that no longer
matches the layer's position, a reserved spare layer placed before a real one, a binding
that names a behavior nobody defined, and a layer macro or morph that refers to a layer
that does not exist.

Needs the ZMK sources only to list the built-in behaviors:
  ZMK_INCLUDE=path/to/zmk/app/include   (default: the build workspace)
"""

import os
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
KEYS = 88


def builtin_behaviors(zmk_include):
    dts = Path(zmk_include).parent / "dts" / "behaviors"
    names = set()
    for f in dts.glob("*.dtsi"):
        names.update(re.findall(r"^\s*(\w+)\s*:", f.read_text(), re.M))
    # Behaviors the board's own devicetree files add.
    for f in (ROOT / "zmk" / "dts").glob("*.dtsi"):
        names.update(re.findall(r"^\s*(\w+)\s*:", f.read_text(), re.M))
    names.update({"kp", "trans", "none"})
    return names


def main():
    path = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "config" / "crush80.keymap"
    zmk_include = os.environ.get(
        "ZMK_INCLUDE", str(Path.home() / "Projects/crush80-workspace/zmk-src/app/include"))
    text = path.read_text()
    errors = []

    defines = {m[0]: int(m[1]) for m in re.findall(r"^#define (\w+)\s+(\d+)\s*$", text, re.M)}
    km = text[text.index("keymap {"):]
    layers = re.findall(r"^\t\t(\w+) \{\n(.*?)^\t\t\};", km, re.M | re.S)
    names = []
    bindings = {}
    reserved_seen = None
    for node, body in layers:
        if 'status = "reserved"' in body:
            reserved_seen = reserved_seen or node
            continue
        if reserved_seen:
            errors.append(f"layer {node} comes after the reserved layer {reserved_seen}; "
                          "spare layers must be last")
        m = re.search(r'display-name = "([^"]*)"', body)
        b = re.search(r"bindings = <(.*?)>;", body, re.S)
        if not m or not b:
            errors.append(f"layer {node}: missing display-name or bindings")
            continue
        cells = ["&" + c.strip() for c in b.group(1).split("&")[1:]]
        if len(cells) != KEYS:
            errors.append(f"layer {node} ({m.group(1)}): {len(cells)} bindings, need {KEYS}")
        names.append(m.group(1))
        bindings[m.group(1)] = cells

    # Every layer #define is the position of the layer with that name.
    for define, number in defines.items():
        if define in ("ZMK_BEHAVIORS_KEEP_ALL",):
            continue
        if number >= len(names):
            errors.append(f"#define {define} {number}: only {len(names)} layers")
        elif names[number].upper() != define:
            errors.append(f"#define {define} {number} points at layer {names[number]!r}")

    known = builtin_behaviors(zmk_include)
    known.update(re.findall(r"^\t\t(\w+):\s*\w+\s*\{", text, re.M))
    for layer, cells in bindings.items():
        for pos, c in enumerate(cells):
            beh = c[1:].split()[0]
            if beh not in known:
                errors.append(f"layer {layer} key {pos}: unknown behavior &{beh}")

    # Layer-valued parameters (&mo FN, &tog VIM, &vsl OPD, ...) must be a layer #define.
    for m in re.finditer(r"&(?:mo|tog|to|sl|vsl)\s+([A-Z]\w*)", text):
        if m.group(1) not in defines:
            errors.append(f"{m.group(0)}: no #define {m.group(1)}")

    if errors:
        print(f"{path}: {len(errors)} problem(s)")
        for e in errors:
            print("  -", e)
        return 1
    print(f"{path.name}: {len(names)} layers x {KEYS} keys, "
          f"{len(known)} behaviors known, all references resolve")
    return 0


if __name__ == "__main__":
    sys.exit(main())
