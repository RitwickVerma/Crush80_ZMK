#!/usr/bin/env python3
"""Stage-2 flasher for the Crush 80 ZMK bridge.

Writes a combined image (MCUboot + ZMK app) to the staging area and issues the
bridge's custom flash_mgmt COMMIT (group 64) command, which erases bank 0,
copies the staged image to 0x0 and resets into MCUboot -> ZMK.

Usage:
    python3 flash_combined.py combined.bin [--port /dev/ttyACM0] [--stg 0x80000]
"""
import argparse
import base64
import glob
import struct
import sys
import time

import cbor2
import serial

FLASH_MGMT_GROUP = 64
CMD_ERASE = 0
CMD_WRITE = 1
CMD_COMMIT = 3

SECTOR_SIZE = 4096
WRITE_CHUNK = 256
TIMEOUT = 5.0


def crc16_ccitt(data):
    crc = 0x0000
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


class SMP:
    def __init__(self, port):
        self.seq = 0
        self.ser = serial.Serial(port, 115200, timeout=0.5)
        time.sleep(0.5)
        self.ser.reset_input_buffer()

    def _frame(self, op, cmd_id, payload):
        self.seq = (self.seq + 1) % 256
        cb = cbor2.dumps(payload)
        msg = bytearray(8 + len(cb))
        msg[0] = op  # 2 = write, 0 = read
        msg[1] = 0
        struct.pack_into(">H", msg, 2, len(cb))
        struct.pack_into(">H", msg, 4, FLASH_MGMT_GROUP)
        msg[6] = self.seq
        msg[7] = cmd_id
        msg[8:] = cb
        pkt = struct.pack(">H", len(msg) + 2) + bytes(msg) + struct.pack(">H", crc16_ccitt(bytes(msg)))
        b64 = base64.b64encode(pkt)
        out = b""
        for i in range(0, len(b64), 124):
            chunk = b64[i:i + 124]
            out += (b"\x06\x09" if i == 0 else b"\x04\x14") + chunk + b"\n"
        return out

    def _recv(self, timeout):
        deadline = time.time() + timeout
        buf = b""
        while time.time() < deadline:
            data = self.ser.read(4096)
            if data:
                buf += data
                while b"\x06\x09" in buf:
                    i = buf.index(b"\x06\x09")
                    nl = buf.find(b"\n", i)
                    if nl == -1:
                        break
                    frame = buf[i + 2:nl]
                    buf = buf[nl + 1:]
                    try:
                        raw = base64.b64decode(frame)
                        if len(raw) >= 12:
                            pkt_len = struct.unpack(">H", raw[0:2])[0]
                            smp = raw[2:2 + pkt_len - 2]
                            if len(smp) >= 8:
                                return cbor2.loads(smp[8:])
                    except Exception:
                        continue
        return None

    def cmd(self, op, cmd_id, payload, timeout=10):
        self.ser.reset_input_buffer()
        self.ser.write(self._frame(op, cmd_id, payload))
        return self._recv(timeout)

    def close(self):
        self.ser.close()


def detect_port():
    for dev in glob.glob('/dev/ttyACM*'):
        return dev
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('image')
    ap.add_argument('--port', default=None)
    ap.add_argument('--stg', default='0x80000', help='staging flash offset')
    args = ap.parse_args()

    stg = int(args.stg, 16)
    with open(args.image, 'rb') as f:
        image = f.read()

    port = args.port or detect_port()
    if not port:
        sys.exit('ERROR: no /dev/ttyACM* found')

    print(f'Image: {len(image)} bytes -> staging 0x{stg:X}')
    print(f'Port:  {port}')

    smp = SMP(port)

    # 1. Erase staging sectors
    nsectors = (len(image) + SECTOR_SIZE - 1) // SECTOR_SIZE
    print(f'Erasing {nsectors} sectors at 0x{stg:X}...')
    for i in range(nsectors):
        off = stg + i * SECTOR_SIZE
        r = smp.cmd(2, CMD_ERASE, {"off": off, "len": SECTOR_SIZE}, timeout=15)
        if r is None or r.get('rc', 1) != 0:
            sys.exit(f'erase failed at 0x{off:X}: {r}')
        if i % 8 == 7:
            print(f'  erased {i + 1}/{nsectors}', flush=True)
    print('Erase complete.')

    # 2. Write image in 256-byte chunks
    nchunks = (len(image) + WRITE_CHUNK - 1) // WRITE_CHUNK
    print(f'Writing {nchunks} chunks...')
    for i in range(nchunks):
        chunk = image[i * WRITE_CHUNK:(i + 1) * WRITE_CHUNK]
        r = smp.cmd(2, CMD_WRITE, {"off": stg + i * WRITE_CHUNK, "data": chunk}, timeout=10)
        if r is None or r.get('rc', 1) != 0:
            sys.exit(f'write failed at chunk {i}: {r}')
        if i % 128 == 127:
            print(f'  {i + 1}/{nchunks} chunks', flush=True)
    print('Write complete.')

    # 3. Commit (erases bank 0, copies staging -> 0x0, resets)
    print(f'Committing (stg=0x{stg:X}, len={len(image)})...')
    r = smp.cmd(2, CMD_COMMIT, {"stg": stg, "len": len(image)}, timeout=10)
    print(f'Commit response: {r}', flush=True)
    print('Keyboard should reboot into ZMK shortly.')
    smp.close()


if __name__ == '__main__':
    main()
