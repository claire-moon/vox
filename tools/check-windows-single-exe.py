#!/usr/bin/env python3
"""Check that the DIGS Windows artifact is a small, x64 single-file EXE."""

# SPDX-License-Identifier: GPL-3.0-or-later

from __future__ import annotations

import argparse
import os
import struct
import sys
from pathlib import Path


class PEError(ValueError):
    pass


def read_u16(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 2 > len(data):
        raise PEError("truncated 16-bit PE field")
    return struct.unpack_from("<H", data, offset)[0]


def read_u32(data: bytes, offset: int) -> int:
    if offset < 0 or offset + 4 > len(data):
        raise PEError("truncated 32-bit PE field")
    return struct.unpack_from("<I", data, offset)[0]


def rva_to_offset(data: bytes, rva: int, headers_size: int,
                  sections: list[tuple[int, int, int, int]]) -> int:
    if rva < headers_size and rva < len(data):
        return rva
    for virtual_address, virtual_size, raw_offset, raw_size in sections:
        span = max(virtual_size, raw_size)
        if virtual_address <= rva < virtual_address + span:
            offset = raw_offset + (rva - virtual_address)
            if offset < len(data):
                return offset
            break
    raise PEError(f"PE RVA 0x{rva:x} does not map to file data")


def read_c_string(data: bytes, offset: int) -> str:
    if offset < 0 or offset >= len(data):
        raise PEError("PE string points outside the file")
    end = data.find(b"\0", offset)
    if end < 0:
        raise PEError("unterminated PE string")
    try:
        return data[offset:end].decode("ascii")
    except UnicodeDecodeError as exc:
        raise PEError("non-ASCII PE import name") from exc


def parse_pe(data: bytes) -> tuple[int, int, list[str]]:
    if len(data) < 0x40 or data[:2] != b"MZ":
        raise PEError("missing DOS executable header")
    pe_offset = read_u32(data, 0x3C)
    if pe_offset + 24 > len(data) or data[pe_offset:pe_offset + 4] != b"PE\0\0":
        raise PEError("missing PE signature")

    machine = read_u16(data, pe_offset + 4)
    section_count = read_u16(data, pe_offset + 6)
    optional_size = read_u16(data, pe_offset + 20)
    optional = pe_offset + 24
    if optional + optional_size > len(data):
        raise PEError("truncated optional header")
    if read_u16(data, optional) != 0x20B:
        raise PEError("expected a 64-bit PE32+ executable")
    subsystem = read_u16(data, optional + 68)
    headers_size = read_u32(data, optional + 60)
    directory_count = read_u32(data, optional + 108)

    sections = []
    section_table = optional + optional_size
    for index in range(section_count):
        header = section_table + index * 40
        if header + 40 > len(data):
            raise PEError("truncated section table")
        sections.append((
            read_u32(data, header + 12),
            read_u32(data, header + 8),
            read_u32(data, header + 20),
            read_u32(data, header + 16),
        ))

    imports: list[str] = []
    if directory_count > 1:
        import_rva = read_u32(data, optional + 112 + 8)
        import_size = read_u32(data, optional + 112 + 12)
        if import_rva != 0 and import_size != 0:
            descriptor = rva_to_offset(data, import_rva, headers_size, sections)
            descriptor_limit = min(len(data), descriptor + import_size)
            terminated = False
            while descriptor + 20 <= descriptor_limit:
                fields = struct.unpack_from("<IIIII", data, descriptor)
                if fields == (0, 0, 0, 0, 0):
                    terminated = True
                    break
                name_offset = rva_to_offset(data, fields[3], headers_size, sections)
                imports.append(read_c_string(data, name_offset))
                descriptor += 20
            if not terminated:
                raise PEError("import table has no terminating descriptor")
    return machine, subsystem, imports


def is_os_dll(name: str) -> bool:
    lower = name.lower()
    if lower.startswith(("api-ms-win-", "ext-ms-win-")):
        return True
    system_root = Path(os.environ.get("SystemRoot", r"C:\Windows"))
    return (system_root / "System32" / name).is_file()


def synthetic_pe() -> bytes:
    """Return a tiny PE32+ image for exercising the import parser on Linux."""
    data = bytearray(0x240)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<HHIIIHH", data, 0x84,
                     0x8664, 1, 0, 0, 0, 240, 0)
    optional = 0x98
    struct.pack_into("<H", data, optional, 0x20B)
    struct.pack_into("<I", data, optional + 60, 0x200)
    struct.pack_into("<H", data, optional + 68, 3)
    struct.pack_into("<I", data, optional + 108, 16)
    struct.pack_into("<II", data, optional + 120, 0x1000, 40)
    section = optional + 240
    data[section:section + 8] = b".idata\0\0"
    struct.pack_into("<IIII", data, section + 8,
                     0x40, 0x1000, 0x40, 0x200)
    struct.pack_into("<IIIII", data, 0x200, 0, 0, 0, 0x1028, 0)
    data[0x228:0x236] = b"KERNEL32.dll\0"
    return bytes(data)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("executable", nargs="?", type=Path)
    parser.add_argument("--max-bytes", type=int, default=16 * 1024 * 1024)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        machine, subsystem, imports = parse_pe(synthetic_pe())
        if machine != 0x8664 or subsystem != 3 or imports != ["KERNEL32.dll"]:
            raise PEError("synthetic PE parser self-test failed")
        print("Windows single-EXE checker self-test passed")
        return 0
    if args.executable is None:
        parser.error("executable is required unless --self-test is used")
    if args.max_bytes <= 0:
        parser.error("--max-bytes must be positive")

    path = args.executable
    size = path.stat().st_size
    if size > args.max_bytes:
        raise PEError(
            f"executable is {size:,} bytes; limit is {args.max_bytes:,} bytes"
        )
    machine, subsystem, imports = parse_pe(path.read_bytes())
    if machine != 0x8664:
        raise PEError(f"expected AMD64 machine 0x8664, got 0x{machine:04x}")
    if subsystem not in (2, 3):
        raise PEError(f"unsupported Windows subsystem {subsystem}")
    external = sorted({name for name in imports if not is_os_dll(name)})
    if external:
        raise PEError(
            "non-Windows runtime DLL imports remain: " + ", ".join(external)
        )
    print(
        f"single-EXE check passed: x64, {size:,} bytes, "
        f"{len(imports)} Windows system DLL imports, no external DLLs"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, PEError) as exc:
        print(f"check-windows-single-exe: {exc}", file=sys.stderr)
        raise SystemExit(1)
