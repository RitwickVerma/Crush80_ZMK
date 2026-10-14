#!/usr/bin/env python3
"""Prepare a ZMK firmware image for the Crush 80 stock OTA handler.

The stock Evision firmware uses a Telink boot header:
  - Offset 0x00: RISC-V jump (j start)
  - Offset 0x06: CRC type (2 bytes, MUST be 0x5D 0x02 — the boot ROM
                 refuses to boot the OTA bank if it does not match)
  - Offset 0x18: OTA image size (LE u32, includes the 4-byte CRC trailer)
  - Offset 0x20: TLNK magic (b"KNLT")
  - Last 4 bytes: CRC32 (poly 0xEDB88320, init 0xFFFFFFFF, NO final XOR
                 == zlib.crc32(...) ^ 0xFFFFFFFF)

The Zephyr build already emits the TLNK magic (start.S), but offset 0x06 and
0x18 are zeros and there is no CRC trailer. This script patches those and
appends the CRC. The body (everything before the CRC) must be 16-byte
aligned or the OTA handler rejects the END command with error 0x0B (SIZE_ERR).
"""
import argparse
import struct
import sys
import zlib

TLNK_MAGIC = b'\x4b\x4e\x4c\x54'  # "KNLT" at offset 0x20
CRC_TYPE_OFFSET = 0x06
CRC_TYPE_VALUE = b'\x5d\x02'
OTA_SIZE_OFFSET = 0x18
TLNK_OFFSET = 0x20


def telink_crc32(data: bytes) -> int:
    return (zlib.crc32(data) & 0xFFFFFFFF) ^ 0xFFFFFFFF


def prepare_ota(input_path: str, output_path: str) -> None:
    with open(input_path, 'rb') as f:
        data = bytearray(f.read())

    if len(data) < TLNK_OFFSET + 4:
        sys.exit("ERROR: file too small to contain a Telink boot header")

    if bytes(data[TLNK_OFFSET:TLNK_OFFSET + 4]) != TLNK_MAGIC:
        sys.exit(f"ERROR: TLNK magic not found at 0x20 "
                 f"(found {bytes(data[TLNK_OFFSET:TLNK_OFFSET + 4]).hex()})")
    print(f"  TLNK magic at 0x20: OK")

    if data[CRC_TYPE_OFFSET:CRC_TYPE_OFFSET + 2] != CRC_TYPE_VALUE:
        data[CRC_TYPE_OFFSET:CRC_TYPE_OFFSET + 2] = CRC_TYPE_VALUE
        print(f"  CRC type at 0x06: patched to {CRC_TYPE_VALUE.hex()}")
    else:
        print("  CRC type at 0x06: already correct")

    # Pad body to 16-byte alignment (OTA handler sends 16-byte segments).
    pad = (16 - (len(data) % 16)) % 16
    if pad:
        data.extend(b'\xff' * pad)
        print(f"  Padded body {pad} bytes to 16-byte align ({len(data)} bytes)")

    total_size = len(data) + 4
    struct.pack_into('<I', data, OTA_SIZE_OFFSET, total_size)
    print(f"  OTA size at 0x18: {total_size} (0x{total_size:X})")

    crc = telink_crc32(bytes(data))
    data.extend(struct.pack('<I', crc))
    print(f"  CRC32 appended: 0x{crc:08X}")

    with open(output_path, 'wb') as f:
        f.write(data)
    print(f"  Total size: {len(data)} bytes")
    print(f"  Output: {output_path}")


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('input', help='Input firmware binary (e.g. build-bridge/zephyr/zmk.bin)')
    ap.add_argument('-o', '--output', default=None, help='Output file (default: <input>_ota.bin)')
    args = ap.parse_args()

    out = args.output or (args.input.rsplit('.', 1)[0] + '_ota.bin')
    prepare_ota(args.input, out)


if __name__ == '__main__':
    main()
