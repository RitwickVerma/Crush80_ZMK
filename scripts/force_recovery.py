#!/usr/bin/env python3
"""Force a Crush 80 into MCUboot USB recovery over Bluetooth.

For when Fn+Esc does not reach the bootloader (for example an older image
without the binding). Over the paired Bluetooth link this erases ONE 4 KB
sector: the header of the running image (flash 0x10000, via the firmware's
flash_mgmt group). Nothing else is touched. On the next power-up MCUboot finds
no valid image and stays in USB recovery, where scripts/dfu.py flashes a new
one.

    python3 scripts/force_recovery.py            # keyboard must be paired and connected
    python3 scripts/force_recovery.py --backup slot0.signed.bin   # read the running image first

The keyboard does not type between this step and the next flash, so keep a
second keyboard at hand. --backup reads the running image (read only) so it can
be flashed back with dfu.py.

Linux only (BlueZ over D-Bus). Requires: pip install dbus-fast cbor2
"""

import argparse
import asyncio
import struct
import sys

try:
    import cbor2
    from dbus_fast import BusType, Message, MessageType, Variant
    from dbus_fast.aio import MessageBus
except ImportError:
    sys.exit("Requires: pip install dbus-fast cbor2")

SMP_UUID = "da2e7828-fbce-4e01-ae9e-261174997c48"   # standard mcumgr SMP characteristic
FLASH_MGMT = 64                                        # custom group, zmk/src/flash_mgmt.c
CMD_ERASE, CMD_READ = 0, 2
SLOT0 = 0x10000
IMAGE_MAGIC = 0x96F3B83D


async def call(bus, path, iface, member, sig="", body=()):
    reply = await bus.call(Message(destination="org.bluez", path=path, interface=iface,
                                   member=member, signature=sig, body=list(body),
                                   message_type=MessageType.METHOD_CALL))
    if reply.message_type == MessageType.ERROR:
        raise RuntimeError(f"{member}: {reply.body}")
    return reply.body


class Smp:
    """mcumgr SMP over BlueZ GATT (notify + write-without-response)."""

    def __init__(self, bus, char):
        self.bus, self.char = bus, char
        self.buf = bytearray()
        self.done = asyncio.Event()

    def _on_signal(self, msg):
        if (msg.message_type == MessageType.SIGNAL and msg.member == "PropertiesChanged"
                and msg.path == self.char and len(msg.body) > 1 and "Value" in msg.body[1]):
            self.buf += bytes(msg.body[1]["Value"].value)
            if len(self.buf) >= 8 and len(self.buf) >= 8 + struct.unpack(">H", self.buf[2:4])[0]:
                self.done.set()

    async def start(self):
        self.bus.add_message_handler(self._on_signal)
        await self.bus.call(Message(
            destination="org.freedesktop.DBus", path="/org/freedesktop/DBus",
            interface="org.freedesktop.DBus", member="AddMatch", signature="s",
            body=[f"type='signal',interface='org.freedesktop.DBus.Properties',"
                  f"member='PropertiesChanged',path='{self.char}'"],
            message_type=MessageType.METHOD_CALL))
        await call(self.bus, self.char, "org.bluez.GattCharacteristic1", "StartNotify")

    async def request(self, op, group, cmd, body, timeout=10):
        self.buf.clear()
        self.done.clear()
        payload = cbor2.dumps(body)
        pkt = struct.pack(">BBHHBB", op, 0, len(payload), group, 0, cmd) + payload
        for i in range(0, len(pkt), 20):                       # fits the default ATT MTU
            await call(self.bus, self.char, "org.bluez.GattCharacteristic1", "WriteValue",
                       "aya{sv}", [pkt[i:i + 20], {"type": Variant("s", "command")}])
        await asyncio.wait_for(self.done.wait(), timeout)
        resp = cbor2.loads(bytes(self.buf[8:8 + struct.unpack(">H", self.buf[2:4])[0]]))
        if resp.get("rc", 0):
            raise RuntimeError(f"device error {resp}")
        return resp


async def find_keyboard(bus, address):
    objs = (await call(bus, "/", "org.freedesktop.DBus.ObjectManager", "GetManagedObjects"))[0]
    found = []
    for path, ifaces in objs.items():
        dev = ifaces.get("org.bluez.Device1")
        if not dev or not dev.get("Connected", Variant("b", False)).value:
            continue
        if address and dev["Address"].value.upper() != address.upper():
            continue
        chars = [p for p, i in objs.items() if p.startswith(path + "/")
                 and i.get("org.bluez.GattCharacteristic1", {}).get("UUID", Variant("s", "")).value
                 == SMP_UUID]
        if chars:
            found.append((dev.get("Name", Variant("s", "?")).value, dev["Address"].value, chars[0]))
    if not found:
        sys.exit("No connected keyboard with an SMP service. Pair it, connect it, and "
                 "switch its output to Bluetooth (unplug USB) first.")
    if len(found) > 1:
        sys.exit("Several candidates; pick one with --address: "
                 + ", ".join(f"{n} {a}" for n, a, _ in found))
    return found[0]


async def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--address", help="Bluetooth address of the keyboard")
    ap.add_argument("--backup", metavar="FILE", help="read the running image to FILE first")
    ap.add_argument("--yes", action="store_true", help="do not ask for confirmation")
    args = ap.parse_args()

    bus = await MessageBus(bus_type=BusType.SYSTEM).connect()
    name, addr, char = await find_keyboard(bus, args.address)
    print(f"Keyboard: {name} ({addr})")
    smp = Smp(bus, char)
    await smp.start()

    head = (await smp.request(0, FLASH_MGMT, CMD_READ, {"off": SLOT0, "len": 32}))["data"]
    magic, _, hdr_size, _, img_size = struct.unpack_from("<IIHHI", head)
    if magic != IMAGE_MAGIC:
        sys.exit("No valid image header at 0x10000: already in recovery state? Nothing done.")

    if args.backup:
        want = hdr_size + img_size + 4            # body + TLV info header; extended below
        data = bytearray()
        while len(data) < want:
            n = min(256, want - len(data))
            data += (await smp.request(0, FLASH_MGMT, CMD_READ,
                                       {"off": SLOT0 + len(data), "len": n}))["data"]
            if len(data) == hdr_size + img_size + 4:
                want = hdr_size + img_size + struct.unpack_from("<H", data, hdr_size + img_size + 2)[0]
            print(f"\r  backup {100 * len(data) // want:3d} %", end="", flush=True)
        open(args.backup, "wb").write(data[:want])
        print(f"\n  saved {want} B to {args.backup}")

    if not args.yes and input("Erase the running image's header and drop to USB recovery? "
                              "[y/N] ").strip().lower() != "y":
        sys.exit("Aborted, nothing changed.")
    try:
        await smp.request(2, FLASH_MGMT, CMD_ERASE, {"off": SLOT0, "len": 4096}, timeout=5)
    except (asyncio.TimeoutError, RuntimeError) as e:
        # The CPU stalls while the sector erases; the reply is often lost.
        print(f"  (no reply to the erase: {type(e).__name__}; this is common)")
    print("Done. Unplug and replug USB: the keyboard comes up as MCUBOOT.\n"
          "Then flash:  python3 scripts/dfu.py dist/crush80-zmk-app.signed.bin")


if __name__ == "__main__":
    asyncio.run(main())
