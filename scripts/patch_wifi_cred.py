#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Patch SSID/PSK into a WunderBar WiFi firmware image (.bin or .elf).

Credentials live in a 128-byte flash slot at 0x0007E000 (default):

  magic[8]=WBWIFIv1  ssid[33]  psk[65]  reserved[...]

Examples:
  ./scripts/patch_wifi_cred.py build-freertos-wifi-rtt/wunderbar_freertos_wifi.bin \\
      --ssid MyNetwork --psk 'secret-pass'
  ./scripts/patch_wifi_cred.py image.bin --show
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

MAGIC = b"WBWIFIv1"
SSID_MAX = 32
PSK_MAX = 64
SLOT_SIZE = 128
DEFAULT_ADDR = 0x0007E000
FLASH_BASE = 0x00000000


def encode_field(value: str, maxlen: int, name: str) -> bytes:
    raw = value.encode("utf-8")
    if len(raw) > maxlen:
        raise SystemExit(f"error: {name} is {len(raw)} bytes (max {maxlen})")
    return raw + b"\x00" * (maxlen + 1 - len(raw))


def pack_slot(ssid: str, psk: str) -> bytes:
    body = MAGIC + encode_field(ssid, SSID_MAX, "ssid") + encode_field(psk, PSK_MAX, "psk")
    if len(body) > SLOT_SIZE:
        raise SystemExit("internal error: slot overflow")
    return body + b"\x00" * (SLOT_SIZE - len(body))


def unpack_slot(data: bytes) -> tuple[str, str]:
    if len(data) < SLOT_SIZE or data[:8] != MAGIC:
        raise SystemExit("error: credential magic not found / invalid slot")
    ssid = data[8 : 8 + SSID_MAX + 1].split(b"\x00", 1)[0].decode("utf-8", "replace")
    psk = data[41 : 41 + PSK_MAX + 1].split(b"\x00", 1)[0].decode("utf-8", "replace")
    return ssid, psk


def find_all_magic(blob: bytes) -> list[int]:
    out: list[int] = []
    start = 0
    while True:
        off = blob.find(MAGIC, start)
        if off < 0:
            return out
        out.append(off)
        start = off + 1


def read_elf_symbol_file_offset(path: Path, symbol: str = "wb_wifi_cred") -> int | None:
    try:
        from elftools.elf.elffile import ELFFile  # type: ignore
    except ImportError:
        return None

    with path.open("rb") as f:
        elf = ELFFile(f)
        symtab = elf.get_section_by_name(".symtab")
        if symtab is None:
            return None
        for sym in symtab.iter_symbols():
            if sym.name != symbol:
                continue
            addr = int(sym["st_value"])
            for seg in elf.iter_segments():
                if seg["p_type"] != "PT_LOAD":
                    continue
                vaddr = int(seg["p_vaddr"])
                filesz = int(seg["p_filesz"])
                if vaddr <= addr < vaddr + filesz:
                    return int(seg["p_offset"]) + (addr - vaddr)
    return None


def load_image(path: Path) -> tuple[bytearray, str]:
    data = bytearray(path.read_bytes())
    kind = "elf" if path.suffix.lower() == ".elf" else "bin"
    return data, kind


def resolve_offset(data: bytearray, path: Path, kind: str, address: int | None) -> int:
    # Explicit address wins.
    if address is not None:
        if kind == "bin":
            off = address - FLASH_BASE
            if off < 0 or off + SLOT_SIZE > len(data):
                raise SystemExit(
                    f"error: address 0x{address:08X} out of range for .bin "
                    f"(file size {len(data)})"
                )
            if data[off : off + 8] != MAGIC:
                raise SystemExit(
                    f"error: no WBWIFIv1 magic at flash 0x{address:08X} "
                    f"(file offset 0x{off:X})"
                )
            return off
        off = read_elf_symbol_file_offset(path)
        if off is not None:
            return off
        raise SystemExit(
            "error: ELF + --address needs pyelftools (pip install pyelftools), "
            "or omit --address to use symbol/magic"
        )

    # ELF: prefer symbol file offset.
    if kind == "elf":
        off = read_elf_symbol_file_offset(path)
        if off is not None:
            return off

    # .bin default: fixed flash slot.
    default_off = DEFAULT_ADDR - FLASH_BASE
    if default_off + SLOT_SIZE <= len(data) and data[default_off : default_off + 8] == MAGIC:
        return default_off

    hits = find_all_magic(data)
    if not hits:
        raise SystemExit("error: magic WBWIFIv1 not found in image")
    if len(hits) > 1:
        print(
            f"warning: {len(hits)} magic markers at {[hex(h) for h in hits]}; "
            f"using 0x{hits[-1]:X} (prefer rebuilding so only the cred slot has magic)",
            file=sys.stderr,
        )
        return hits[-1]
    return hits[0]


def main() -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument("image", type=Path, help="Firmware .bin or .elf")
    ap.add_argument("--ssid", help="STA SSID (max 32 bytes)")
    ap.add_argument("--psk", default=None, help="STA PSK (max 64 bytes); empty for open")
    ap.add_argument(
        "--address",
        type=lambda s: int(s, 0),
        default=None,
        help=f"Absolute flash address (default: 0x{DEFAULT_ADDR:X} for .bin)",
    )
    ap.add_argument("--show", action="store_true", help="Print current credentials and exit")
    ap.add_argument(
        "-o",
        "--output",
        type=Path,
        help="Write patched image here (default: in-place)",
    )
    args = ap.parse_args()

    if not args.image.is_file():
        raise SystemExit(f"error: file not found: {args.image}")

    data, kind = load_image(args.image)
    off = resolve_offset(data, args.image, kind, args.address)

    if args.show or (args.ssid is None and args.psk is None):
        ssid, psk = unpack_slot(bytes(data[off : off + SLOT_SIZE]))
        print(f"offset  0x{off:X}  (flash 0x{FLASH_BASE + off:08X})")
        print(f"ssid    {ssid!r}")
        print(f"psk     {psk!r}")
        return 0

    if args.ssid is None:
        raise SystemExit("error: --ssid is required unless using --show")
    psk = "" if args.psk is None else args.psk

    slot = pack_slot(args.ssid, psk)
    data[off : off + SLOT_SIZE] = slot
    out = args.output or args.image
    out.write_bytes(data)
    print(f"patched {out} at offset 0x{off:X} (flash 0x{FLASH_BASE + off:08X})")
    print(f"  ssid={args.ssid!r}")
    print(f"  psk={psk!r}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
