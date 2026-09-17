#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Patch SSID/PSK into a WunderBar WiFi firmware image (.bin and/or .elf).

Credentials live in a 128-byte flash slot at 0x0007E000:

  magic[8]=WBWIFIv1  ssid[33]  psk[65]  reserved[...]

IMPORTANT: J-Link / MCUXpresso / Ozone usually flash the .elf, not the .bin.
Patch BOTH, or flash the patched .bin explicitly at address 0x00000000.

Examples:
  ./scripts/patch_wifi_cred.py build-freertos-wifi-rtt/wunderbar_freertos_wifi.bin \\
      --ssid MyNetwork --psk 'secret-pass'
  # also updates sibling .elf when present
  ./scripts/patch_wifi_cred.py image.bin --show
"""

from __future__ import annotations

import argparse
import struct
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


def elf_section_file_offset(data: bytes, section_name: str = ".wb_wifi_cred") -> int | None:
    """Return file offset of a section in a 32-bit little-endian ELF."""
    if len(data) < 52 or data[:4] != b"\x7fELF":
        return None
    ei_class, ei_data = data[4], data[5]
    if ei_class != 1 or ei_data != 1:  # ELF32 LSB only
        return None

    (
        _e_type,
        _e_machine,
        _e_version,
        _e_entry,
        _e_phoff,
        e_shoff,
        _e_flags,
        _e_ehsize,
        _e_phentsize,
        _e_phnum,
        e_shentsize,
        e_shnum,
        e_shstrndx,
    ) = struct.unpack_from("<HHIIIIIHHHHHH", data, 16)

    if e_shentsize < 40 or e_shnum == 0 or e_shstrndx >= e_shnum:
        return None

    def shdr(i: int) -> tuple:
        off = e_shoff + i * e_shentsize
        return struct.unpack_from("<IIIIIIIIII", data, off)

    # sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size, ...
    str_sh = shdr(e_shstrndx)
    str_off, str_size = str_sh[4], str_sh[5]
    strtab = data[str_off : str_off + str_size]

    for i in range(e_shnum):
        sh_name, _t, _f, sh_addr, sh_offset, sh_size, *_rest = shdr(i)
        if sh_name >= len(strtab):
            continue
        end = strtab.find(b"\x00", sh_name)
        name = strtab[sh_name:end if end >= 0 else None].decode("ascii", "replace")
        if name == section_name and sh_size >= SLOT_SIZE:
            if data[sh_offset : sh_offset + 8] != MAGIC and sh_addr != DEFAULT_ADDR:
                # Still accept by name; caller verifies magic.
                pass
            return int(sh_offset)
    return None


def elf_symbol_file_offset(data: bytes, symbol: str = "wb_wifi_cred") -> int | None:
    """Fallback: locate symbol via .symtab (ELF32 LE)."""
    if len(data) < 52 or data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
        return None

    (
        _e_type,
        _e_machine,
        _e_version,
        _e_entry,
        e_phoff,
        e_shoff,
        _e_flags,
        _e_ehsize,
        e_phentsize,
        e_phnum,
        e_shentsize,
        e_shnum,
        e_shstrndx,
    ) = struct.unpack_from("<HHIIIIIHHHHHH", data, 16)

    def shdr(i: int):
        return struct.unpack_from("<IIIIIIIIII", data, e_shoff + i * e_shentsize)

    str_sh = shdr(e_shstrndx)
    strtab = data[str_sh[4] : str_sh[4] + str_sh[5]]

    symtab_off = symtab_size = ent = strtab_sym_off = None
    for i in range(e_shnum):
        sh_name, sh_type, _f, _a, sh_offset, sh_size, sh_link, *_r = shdr(i)
        end = strtab.find(b"\x00", sh_name)
        name = strtab[sh_name:end if end >= 0 else None].decode("ascii", "replace")
        if name == ".symtab" and sh_type == 2:
            symtab_off, symtab_size, ent = sh_offset, sh_size, 16
            # sh_link -> .strtab
            link = shdr(sh_link)
            strtab_sym_off = link[4]
        elif name == ".dynsym":
            continue

    if symtab_off is None or ent is None or strtab_sym_off is None:
        return None

    sym_str = data  # full file; offsets absolute
    # Load sym strtab section
    # find .strtab linked — use sh_link size from symtab's linked section
    for i in range(e_shnum):
        sh_name, sh_type, _f, _a, sh_offset, sh_size, *_r = shdr(i)
        end = strtab.find(b"\x00", sh_name)
        name = strtab[sh_name:end if end >= 0 else None].decode("ascii", "replace")
        if name == ".strtab":
            sym_names = data[sh_offset : sh_offset + sh_size]
            break
    else:
        return None

    addr = None
    for off in range(symtab_off, symtab_off + symtab_size, ent):
        st_name, st_value, st_size, st_info, st_other, st_shndx = struct.unpack_from(
            "<IIIBBH", data, off
        )
        nend = sym_names.find(b"\x00", st_name)
        n = sym_names[st_name:nend if nend >= 0 else None].decode("ascii", "replace")
        if n == symbol:
            addr = st_value
            break
    if addr is None:
        return None

    # Map VMA -> file via PT_LOAD
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        p_type, p_offset, p_vaddr, _p_paddr, p_filesz, _p_memsz, _p_flags, _p_align = (
            struct.unpack_from("<IIIIIIII", data, off)
        )
        if p_type != 1:  # PT_LOAD
            continue
        if p_vaddr <= addr < p_vaddr + p_filesz:
            return p_offset + (addr - p_vaddr)
    return None


def resolve_offset(data: bytearray, kind: str, address: int | None) -> int:
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
        # ELF + address: prefer section / symbol
        off = elf_section_file_offset(data) or elf_symbol_file_offset(data)
        if off is not None:
            return off
        raise SystemExit("error: could not map address to ELF file offset")

    if kind == "elf":
        off = elf_section_file_offset(data)
        if off is not None and data[off : off + 8] == MAGIC:
            return off
        off = elf_symbol_file_offset(data)
        if off is not None and data[off : off + 8] == MAGIC:
            return off

    default_off = DEFAULT_ADDR - FLASH_BASE
    if default_off + SLOT_SIZE <= len(data) and data[default_off : default_off + 8] == MAGIC:
        return default_off

    hits = find_all_magic(data)
    if not hits:
        raise SystemExit("error: magic WBWIFIv1 not found in image")
    if len(hits) > 1:
        # Prefer DEFAULT_ADDR if present
        if default_off in hits:
            return default_off
        print(
            f"warning: {len(hits)} magic markers at {[hex(h) for h in hits]}; "
            f"using 0x{hits[-1]:X}",
            file=sys.stderr,
        )
        return hits[-1]
    return hits[0]


def elf_section_vma(data: bytes, section_name: str = ".wb_wifi_cred") -> int | None:
    """Return VMA of a named ELF32 LE section."""
    if len(data) < 52 or data[:4] != b"\x7fELF" or data[4] != 1 or data[5] != 1:
        return None
    (
        _e_type,
        _e_machine,
        _e_version,
        _e_entry,
        _e_phoff,
        e_shoff,
        _e_flags,
        _e_ehsize,
        _e_phentsize,
        _e_phnum,
        e_shentsize,
        e_shnum,
        e_shstrndx,
    ) = struct.unpack_from("<HHIIIIIHHHHHH", data, 16)
    if e_shentsize < 40 or e_shnum == 0 or e_shstrndx >= e_shnum:
        return None

    def shdr(i: int) -> tuple:
        return struct.unpack_from("<IIIIIIIIII", data, e_shoff + i * e_shentsize)

    str_sh = shdr(e_shstrndx)
    strtab = data[str_sh[4] : str_sh[4] + str_sh[5]]
    for i in range(e_shnum):
        sh_name, _t, _f, sh_addr, _sh_offset, sh_size, *_rest = shdr(i)
        end = strtab.find(b"\x00", sh_name)
        name = strtab[sh_name:end if end >= 0 else None].decode("ascii", "replace")
        if name == section_name and sh_size >= SLOT_SIZE:
            return int(sh_addr)
    return None


def patch_one(path: Path, ssid: str | None, psk: str | None, show: bool, address: int | None) -> None:
    data = bytearray(path.read_bytes())
    kind = "elf" if path.suffix.lower() == ".elf" else "bin"
    off = resolve_offset(data, kind, address)
    flash_addr = DEFAULT_ADDR
    if kind == "elf":
        vma = elf_section_vma(bytes(data))
        if vma is not None:
            flash_addr = vma
    elif kind == "bin":
        flash_addr = FLASH_BASE + off

    if show or ssid is None:
        s, p = unpack_slot(bytes(data[off : off + SLOT_SIZE]))
        print(f"{path}:")
        print(f"  offset  0x{off:X}  (flash 0x{flash_addr:08X})")
        print(f"  ssid    {s!r}")
        print(f"  psk     {p!r}")
        if kind == "elf" and flash_addr != DEFAULT_ADDR:
            print(
                f"  note: section VMA is 0x{flash_addr:08X}, not 0x{DEFAULT_ADDR:08X} — "
                f"rebuild with WIFI_CRED linker region, or flash this .elf after patching",
                file=sys.stderr,
            )
        return

    assert psk is not None
    slot = pack_slot(ssid, psk)
    data[off : off + SLOT_SIZE] = slot
    path.write_bytes(data)
    print(f"patched {path} at file offset 0x{off:X} (flash 0x{flash_addr:08X})")
    print(f"  ssid={ssid!r}")
    print(f"  psk={psk!r}")
    if kind == "bin" and len(data) <= DEFAULT_ADDR:
        print(
            "warning: this .bin is smaller than 0x7E000 — Zephyr images must be "
            "patched as .elf (J-Link flashes the .elf section).",
            file=sys.stderr,
        )


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
        help=f"Absolute flash address for .bin (default: 0x{DEFAULT_ADDR:X})",
    )
    ap.add_argument("--show", action="store_true", help="Print current credentials and exit")
    ap.add_argument(
        "-o",
        "--output",
        type=Path,
        help="Write patched image here (default: in-place). Disables sibling auto-patch.",
    )
    ap.add_argument(
        "--no-sibling",
        action="store_true",
        help="Do not also patch sibling .elf/.bin next to the input",
    )
    args = ap.parse_args()

    if not args.image.is_file():
        raise SystemExit(f"error: file not found: {args.image}")

    show = args.show or (args.ssid is None and args.psk is None)
    if not show and args.ssid is None:
        raise SystemExit("error: --ssid is required unless using --show")
    psk = "" if args.psk is None else args.psk

    targets = [args.image]
    if args.output:
        data = args.image.read_bytes()
        args.output.write_bytes(data)
        targets = [args.output]
    elif not args.no_sibling:
        # Also touch companion image: J-Link / Ozone usually flash the .elf.
        if args.image.suffix.lower() == ".bin":
            sib = args.image.with_suffix(".elf")
            if sib.is_file():
                targets.append(sib)
        elif args.image.suffix.lower() == ".elf":
            sib = args.image.with_suffix(".bin")
            if sib.is_file():
                targets.append(sib)

    for t in targets:
        patch_one(t, None if show else args.ssid, None if show else psk, show, args.address)

    if not show and len(targets) == 1 and args.image.suffix.lower() == ".bin":
        print(
            "note: flash this .bin at 0x00000000, OR also patch the .elf "
            "(J-Link usually loads the .elf).",
            file=sys.stderr,
        )
    if show and len(targets) > 1:
        print(
            "note: J-Link typically programs the .elf — both images must match.",
            file=sys.stderr,
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
