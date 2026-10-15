#!/usr/bin/env python3
"""Validate and pad a GameCube/Wii DOL for 32-byte section reads.

Some ELF-to-DOL tools leave the final section's stored size unaligned and do
not include the extra bytes which loaders may read after rounding that size up
to 32 bytes. Dolphin's DOL reader rejects a file if those padded bytes would
extend beyond EOF. This helper preserves all header fields and appends zeros
only when required.
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path

HEADER_SIZE = 0x100
ALIGNMENT = 32


def align_up(value: int) -> int:
    return (value + ALIGNMENT - 1) & ~(ALIGNMENT - 1)


def required_file_size(data: bytes) -> int:
    if len(data) < HEADER_SIZE:
        raise ValueError(
            f"file is too short to contain a DOL header ({len(data)} < {HEADER_SIZE})"
        )

    # DOL header: 7 text offsets, 11 data offsets, then addresses, then sizes.
    text_offsets = struct.unpack_from(">7I", data, 0x00)
    data_offsets = struct.unpack_from(">11I", data, 0x1C)
    text_sizes = struct.unpack_from(">7I", data, 0x90)
    data_sizes = struct.unpack_from(">11I", data, 0xAC)

    active_sections: list[tuple[str, int, int, int]] = []
    required_end = HEADER_SIZE
    for name, offsets, sizes in (
        ("text", text_offsets, text_sizes),
        ("data", data_offsets, data_sizes),
    ):
        for index, (offset, size) in enumerate(zip(offsets, sizes)):
            if size == 0:
                continue
            if offset < HEADER_SIZE:
                raise ValueError(
                    f"{name} section {index} has invalid file offset 0x{offset:x}"
                )
            if offset % ALIGNMENT:
                raise ValueError(
                    f"{name} section {index} file offset 0x{offset:x} is not "
                    f"{ALIGNMENT}-byte aligned"
                )
            actual_end = offset + size
            padded_end = offset + align_up(size)
            if actual_end > len(data):
                raise ValueError(
                    f"{name} section {index} extends past EOF "
                    f"(0x{actual_end:x} > 0x{len(data):x})"
                )
            active_sections.append((f"{name} section {index}", offset, actual_end, padded_end))
            required_end = max(required_end, padded_end)

    # A padded read must not run into the next section's stored bytes.
    ordered = sorted(active_sections, key=lambda item: item[1])
    for current, following in zip(ordered, ordered[1:]):
        current_name, _, _, padded_end = current
        following_name, following_offset, _, _ = following
        if padded_end > following_offset:
            raise ValueError(
                f"{current_name}'s aligned range overlaps {following_name} "
                f"(0x{padded_end:x} > 0x{following_offset:x})"
            )

    if not active_sections:
        raise ValueError("DOL has no non-empty text or data sections")
    return required_end


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dol", type=Path, help="path to the DOL file")
    parser.add_argument(
        "--check",
        action="store_true",
        help="validate only; fail instead of adding missing trailing padding",
    )
    args = parser.parse_args()

    data = args.dol.read_bytes()
    try:
        required = required_file_size(data)
    except ValueError as exc:
        print(f"DOL validation failed: {exc}", file=sys.stderr)
        return 1

    if len(data) < required:
        missing = required - len(data)
        if args.check:
            print(
                f"DOL validation failed: missing {missing} trailing padding byte(s) "
                f"(file size 0x{len(data):x}, required 0x{required:x})",
                file=sys.stderr,
            )
            return 1
        args.dol.write_bytes(data + bytes(missing))
        print(f"Padded {args.dol}: appended {missing} zero byte(s)")
        data_len = required
    else:
        data_len = len(data)

    print(f"DOL validated: {args.dol} ({data_len} bytes; aligned section reads fit)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
