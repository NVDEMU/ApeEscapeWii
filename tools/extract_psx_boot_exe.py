#!/usr/bin/env python3
"""Extract the boot executable named by SYSTEM.CNF from a PS1 ISO9660 image."""
from __future__ import annotations

import argparse
import io
import re
import sys
from pathlib import Path

try:
    import pycdlib
except ImportError:
    print("Missing dependency: install pycdlib with pip.", file=sys.stderr)
    raise SystemExit(2)


def read_iso_file(iso: "pycdlib.PyCdlib", candidates: list[str]) -> bytes:
    last_error: Exception | None = None
    for iso_path in candidates:
        sink = io.BytesIO()
        try:
            iso.get_file_from_iso_fp(sink, iso_path=iso_path)
            return sink.getvalue()
        except Exception as exc:  # pycdlib exposes several path-not-found forms
            last_error = exc
    raise RuntimeError(f"Could not read any of {candidates}: {last_error}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("iso", type=Path, help="2048-byte-sector ISO made from the disc's data track")
    parser.add_argument("output", type=Path, help="output path for the extracted PS-X EXE")
    args = parser.parse_args()

    iso = pycdlib.PyCdlib()
    try:
        iso.open(str(args.iso))
        cnf = read_iso_file(iso, ["/SYSTEM.CNF;1", "/SYSTEM.CNF"])
        text = cnf.decode("ascii", errors="replace")
        match = re.search(
            r"^\\s*BOOT(?:\\d)?\\s*=\\s*cdrom\\d*:[\\\\/]*([^;\\s\\r\\n]+)(?:;(\\d+))?",
            text,
            re.IGNORECASE | re.MULTILINE,
        )
        if not match:
            raise RuntimeError("SYSTEM.CNF has no recognizable BOOT=cdrom entry")

        boot_name = match.group(1).replace("\\\\", "/").replace("\\", "/")
        version = match.group(2) or "1"
        # Treat the path as ISO9660, preserving any directory portion.
        iso_path = "/" + boot_name.lstrip("/").upper()
        if not iso_path.endswith(";" + version):
            iso_path += ";" + version

        exe = read_iso_file(iso, [iso_path, iso_path.rsplit(";", 1)[0]])
        if not exe.startswith(b"PS-X EXE"):
            raise RuntimeError(
                f"Boot file {iso_path} does not have the PS-X EXE signature; "
                "check the extracted data track and cue layout"
            )

        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes(exe)
        print(f"Extracted {iso_path} -> {args.output} ({len(exe):,} bytes)")
        return 0
    except Exception as exc:
        print(f"PS1 executable extraction failed: {exc}", file=sys.stderr)
        return 1
    finally:
        iso.close()


if __name__ == "__main__":
    raise SystemExit(main())
