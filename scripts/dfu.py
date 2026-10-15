#!/usr/bin/env python3
"""Flash a signed ZMK image to the Crush 80 through MCUboot serial recovery.

Put the keyboard in bootloader mode first (Fn+Esc): it re-enumerates as
"ZEPHYR MCUBOOT" (USB 2fe3:0100). The image is written straight into the boot
slot, its hash is checked against what MCUboot reports, then the keyboard is
reset into the new firmware.

Pure Python 3 standard library: no mcumgr, pyserial or cbor packages needed.

    python3 scripts/dfu.py dist/crush80-zmk-app.signed.bin
    python3 scripts/dfu.py IMAGE --port /dev/ttyACM0      # skip auto-detection
    python3 scripts/dfu.py --list                          # show slots, change nothing
"""

import argparse
import base64
import glob
import hashlib
import os
import select
import struct
import sys
import termios
import time

CHUNK = 256            # image bytes per request (MCUboot receive buffer is 1 KiB)
LINE_MAX = 124         # SMP serial lines are limited to 127 bytes incl. framing
TIMEOUT_S = 5.0
WAIT_FOR_DEVICE_S = 60


# --- minimal CBOR (only what SMP image upload needs) -------------------------

def _cbor_head(major, n):
    if n < 24:
        return bytes([major << 5 | n])
    for ai, fmt in ((24, ">B"), (25, ">H"), (26, ">I"), (27, ">Q")):
        if n < 1 << (8 * struct.calcsize(fmt)):
            return bytes([major << 5 | ai]) + struct.pack(fmt, n)
    raise ValueError(n)


def cbor_encode(obj):
    if isinstance(obj, bool):
        return b"\xf5" if obj else b"\xf4"
    if isinstance(obj, int):
        return _cbor_head(0, obj) if obj >= 0 else _cbor_head(1, -1 - obj)
    if isinstance(obj, bytes):
        return _cbor_head(2, len(obj)) + obj
    if isinstance(obj, str):
        b = obj.encode()
        return _cbor_head(3, len(b)) + b
    if isinstance(obj, list):
        return _cbor_head(4, len(obj)) + b"".join(cbor_encode(v) for v in obj)
    if isinstance(obj, dict):
        return _cbor_head(5, len(obj)) + b"".join(cbor_encode(k) + cbor_encode(v)
                                                  for k, v in obj.items())
    raise TypeError(type(obj))


def cbor_decode(buf, i=0):
    ib = buf[i]
    major, ai = ib >> 5, ib & 31
    i += 1
    if ai < 24:
        n = ai
    elif ai in (24, 25, 26, 27):
        size = 1 << (ai - 24)
        n = int.from_bytes(buf[i:i + size], "big")
        i += size
    elif ai == 31:
        n = None                                   # indefinite length
    else:
        raise ValueError(f"unsupported CBOR item 0x{ib:02x}")
    if major == 0:
        return n, i
    if major == 1:
        return -1 - n, i
    if major in (2, 3):
        raw = buf[i:i + n]
        return (bytes(raw) if major == 2 else raw.decode()), i + n
    if major in (4, 5):
        out = [] if major == 4 else {}
        count = 0
        while (n is None and buf[i] != 0xFF) or (n is not None and count < n):
            if major == 4:
                v, i = cbor_decode(buf, i)
                out.append(v)
            else:
                k, i = cbor_decode(buf, i)
                v, i = cbor_decode(buf, i)
                out[k] = v
            count += 1
        return out, (i + 1 if n is None else i)
    if major == 7:
        return {20: False, 21: True, 22: None}.get(ai), i
    raise ValueError(f"unsupported CBOR major type {major}")


# --- SMP over serial ----------------------------------------------------------

def crc16_xmodem(data):
    crc = 0
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


class Smp:
    def __init__(self, port):
        self.fd = os.open(port, os.O_RDWR | os.O_NOCTTY)
        attrs = termios.tcgetattr(self.fd)
        attrs[0] = attrs[1] = attrs[3] = 0                      # raw: no echo / translation
        attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL
        attrs[4] = attrs[5] = termios.B115200
        termios.tcsetattr(self.fd, termios.TCSANOW, attrs)
        termios.tcflush(self.fd, termios.TCIOFLUSH)
        self.seq = 0
        self.rx = b""

    def close(self):
        os.close(self.fd)

    def _send(self, pkt):
        raw = base64.b64encode(struct.pack(">H", len(pkt) + 2) + pkt
                               + struct.pack(">H", crc16_xmodem(pkt)))
        first = True
        while raw:
            room = (LINE_MAX - 3) // 4 * 4     # MCUboot decodes each line separately
            part, raw = raw[:room], raw[room:]
            os.write(self.fd, (b"\x06\x09" if first else b"\x04\x14") + part + b"\n")
            first = False

    def _recv(self, timeout):
        end = time.monotonic() + timeout
        b64 = b""
        while True:
            while b"\n" not in self.rx:
                left = end - time.monotonic()
                if left <= 0 or not select.select([self.fd], [], [], left)[0]:
                    raise TimeoutError("no response from the keyboard")
                self.rx += os.read(self.fd, 4096)
            line, self.rx = self.rx.split(b"\n", 1)
            if line[:2] == b"\x06\x09":
                b64 = line[2:]
            elif line[:2] == b"\x04\x14":
                b64 += line[2:]
            else:
                continue                                         # console noise
            try:
                raw = base64.b64decode(b64)
            except ValueError:
                continue                                         # frame not complete yet
            total = struct.unpack(">H", raw[:2])[0]
            if len(raw) - 2 < total:
                continue
            pkt = raw[2:2 + total - 2]
            if crc16_xmodem(pkt) != struct.unpack(">H", raw[total:total + 2])[0]:
                raise IOError("CRC mismatch in response")
            return pkt

    def request(self, op, group, cmd, body, timeout=TIMEOUT_S):
        payload = cbor_encode(body)
        self.seq = (self.seq + 1) & 0xFF
        self._send(struct.pack(">BBHHBB", op, 0, len(payload), group, self.seq, cmd) + payload)
        pkt = self._recv(timeout)
        resp, _ = cbor_decode(pkt, 8)
        rc = resp.get("rc", 0) if isinstance(resp, dict) else 0
        if rc:
            raise IOError(f"device returned error rc={rc}: {resp}")
        return resp


def image_hash(data):
    """SHA-256 MCUboot reports for a signed image (header + body, no TLVs)."""
    hdr_size = struct.unpack_from("<H", data, 8)[0]
    img_size = struct.unpack_from("<I", data, 12)[0]
    return hashlib.sha256(data[:hdr_size + img_size]).digest()


def find_port():
    deadline = time.monotonic() + WAIT_FOR_DEVICE_S
    told = False
    while time.monotonic() < deadline:
        hits = sorted(glob.glob("/dev/serial/by-id/*MCUBOOT*"))
        if hits:
            return os.path.realpath(hits[0])
        if not told:
            print("Waiting for the bootloader (press Fn+Esc, or plug in the keyboard)...")
            told = True
        time.sleep(0.5)
    sys.exit("No MCUboot device found. Is the keyboard in bootloader mode (Fn+Esc)?")


def list_images(smp):
    images = smp.request(0, 1, 0, {}).get("images", [])
    if not images:
        print("  (no valid image)")
    for im in images:
        print(f"  slot {im.get('slot')}: version {im.get('version')}  "
              f"hash {im.get('hash', b'').hex()[:16]}")
    return images


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("image", nargs="?", help="signed image (*.signed.bin)")
    ap.add_argument("--port", help="serial port (default: find the MCUboot device)")
    ap.add_argument("--list", action="store_true", help="only list images")
    ap.add_argument("--no-reset", action="store_true", help="stay in the bootloader")
    args = ap.parse_args()
    if not args.list and not args.image:
        ap.error("an image is required (or --list)")

    port = args.port or find_port()
    print(f"Bootloader on {port}")
    smp = Smp(port)
    try:
        if args.list:
            list_images(smp)
            return
        data = open(args.image, "rb").read()
        if struct.unpack_from("<I", data, 0)[0] != 0x96F3B83D:
            sys.exit(f"{args.image} is not a signed MCUboot image (use *.signed.bin)")
        want = image_hash(data)
        t0 = time.monotonic()
        off = 0
        while off < len(data):
            body = {"off": off, "data": data[off:off + CHUNK]}
            if off == 0:
                body["len"] = len(data)
            resp = smp.request(2, 1, 1, body)
            new = resp.get("off", off + CHUNK)
            if new <= off:
                sys.exit(f"upload stalled at offset {off}")
            off = new
            print(f"\r  {100 * off // len(data):3d} %  {off}/{len(data)} B", end="", flush=True)
        print(f"\n  uploaded in {time.monotonic() - t0:.1f} s")
        images = list_images(smp)
        if any(im.get("hash") == want for im in images):
            print("Verified: image hash matches.")
        elif images:
            sys.exit("Verification FAILED: the bootloader reports a different image. "
                     "The keyboard stays in the bootloader; try again.")
        else:
            # This MCUboot build does not always list a just-written slot. Safe to
            # continue: MCUboot validates the image at boot and, if it is bad, stays
            # in recovery mode (BOOT_SERIAL_NO_APPLICATION) instead of running it.
            print("Bootloader did not list the image; it is validated on boot instead.")
        if not args.no_reset:
            try:
                smp.request(2, 0, 5, {}, timeout=2)
            except (TimeoutError, OSError):
                pass                                   # it may reset before answering
            print("Reset. The keyboard boots the new firmware in a few seconds.")
    finally:
        smp.close()


if __name__ == "__main__":
    main()
