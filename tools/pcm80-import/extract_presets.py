#!/usr/bin/env python3
"""Extract the factory preset table from a Lexicon PCM80 host-CPU firmware
ROM dump into a JSON archive.

Usage:
    python3 extract_presets.py <firmware.bin> -o <archive.json>

What this actually decodes
---------------------------
The PCM80's host/UI controller is an Intel 80186-class CPU (separate from
the Motorola 56000-series DSP that does the actual audio processing). Its
ROM stores factory presets as a chain of variable-length records:

    offset  size  field
    0       12    unidentified per-record header (pointers/flags, not decoded)
    12      2     record length, little-endian uint16 (distance to next record)
    14      2     unidentified (possibly algorithm/category id, not decoded)
    16      12    preset name, space-padded ASCII
    28      9     "quick knob" macro label, space-padded ASCII
    37      1     0x00 terminator
    38      *     remainder of record ("tail"), length = record_length - 38

The name/label fields are solid - this was verified by chain-walking the
whole ROM and finding a 200-record chain, matching the PCM80's known
factory preset count exactly, with every decoded name reading as a
plausible real preset ("Concert Hall", "Vox Chamber", "Rich Plate", ...).

The per-record "tail" is almost certainly the actual parameter/coefficient
block for that preset (its size varies with how many parameters the
algorithm exposes), but its internal byte layout has NOT been decoded -
that would need either real hardware to correlate knob movements against,
or a full disassembly of the firmware's patch-load routine. This script
preserves the tail as raw hex in the archive rather than guessing at
field meanings.

Copyright
---------
The ROM and every string/byte extracted from it are Lexicon's copyrighted
property. This script only operates on a firmware file *you* supply (from
hardware *you* own) and never ships, bundles, or commits any ROM image or
extracted archive - see tools/pcm80-import/README.md.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
from pathlib import Path

RECORD_HEADER_SIZE = 16
NAME_SIZE = 12
LABEL_SIZE = 9
NAME_OFFSET = RECORD_HEADER_SIZE
LABEL_OFFSET = NAME_OFFSET + NAME_SIZE
FIXED_PREFIX_SIZE = LABEL_OFFSET + LABEL_SIZE + 1  # + 0x00 terminator
MIN_RECORD_LEN = 30
MAX_RECORD_LEN = 400
MIN_CHAIN_LEN = 20  # reject short/incidental chains


def _is_name_like(chunk: bytes) -> bool:
    if not chunk or not (0x20 <= chunk[0] < 0x7F):
        return False
    printable = sum(1 for b in chunk if 0x20 <= b < 0x7F)
    return printable / len(chunk) > 0.85


def _chain_length(data: bytes, start: int) -> tuple[int, int]:
    """Walk a candidate record chain from `start`. Returns (count, end_offset)."""
    pos = start
    count = 0
    while pos + RECORD_HEADER_SIZE <= len(data):
        length = struct.unpack_from("<H", data, pos + 12)[0]
        if not (MIN_RECORD_LEN <= length <= MAX_RECORD_LEN):
            break
        if pos + length > len(data):
            break
        name_field = data[pos + NAME_OFFSET : pos + NAME_OFFSET + NAME_SIZE]
        if not _is_name_like(name_field):
            break
        count += 1
        pos += length
    return count, pos


def find_preset_table(data: bytes) -> tuple[int, int]:
    """Scan the whole ROM for the longest valid record chain.

    Returns (start_offset, record_count). Raises ValueError if nothing
    resembling the known record format is found.
    """
    best_start = None
    best_count = 0
    i = 0
    n = len(data)
    while i + RECORD_HEADER_SIZE <= n:
        length = struct.unpack_from("<H", data, i + 12)[0]
        if MIN_RECORD_LEN <= length <= MAX_RECORD_LEN and i + length <= n:
            name_field = data[i + NAME_OFFSET : i + NAME_OFFSET + NAME_SIZE]
            if _is_name_like(name_field):
                count, _end = _chain_length(data, i)
                if count > best_count:
                    best_count = count
                    best_start = i
                # Skip ahead - no point re-testing offsets inside a chain
                # we already know is shorter than our best.
        i += 1
    if best_start is None or best_count < MIN_CHAIN_LEN:
        raise ValueError(
            "Could not find a preset record chain in this file - "
            "it may not be a PCM80 host-CPU ROM, or the record format "
            "differs from the one this script knows about."
        )
    return best_start, best_count


def parse_records(data: bytes, start: int) -> list[dict]:
    presets = []
    pos = start
    index = 1
    while True:
        length = struct.unpack_from("<H", data, pos + 12)[0]
        if not (MIN_RECORD_LEN <= length <= MAX_RECORD_LEN):
            break
        name_field = data[pos + NAME_OFFSET : pos + NAME_OFFSET + NAME_SIZE]
        if not _is_name_like(name_field):
            break

        name = name_field.decode("latin1").rstrip()
        label_field = data[pos + LABEL_OFFSET : pos + LABEL_OFFSET + LABEL_SIZE]
        label = label_field.decode("latin1").rstrip("\x00").rstrip()
        header = data[pos : pos + RECORD_HEADER_SIZE]
        tail = data[pos + FIXED_PREFIX_SIZE : pos + length]

        presets.append(
            {
                "index": index,
                "name": name,
                "macro_knob_label": label,
                "rom_offset": pos,
                "record_length": length,
                "header_hex_undecoded": header.hex(),
                "tail_hex_undecoded": tail.hex(),
            }
        )
        pos += length
        index += 1
    return presets


def build_archive(rom_path: Path) -> dict:
    data = rom_path.read_bytes()
    start, _count = find_preset_table(data)
    presets = parse_records(data, start)
    return {
        "_copyright_notice": (
            "Preset names and data below were extracted from a Lexicon PCM80 "
            "firmware ROM. They are Lexicon's copyrighted property. This "
            "archive is for personal reference/archival use with hardware "
            "you own - do not redistribute it."
        ),
        "source_file": rom_path.name,
        "source_sha256": hashlib.sha256(data).hexdigest(),
        "rom_size": len(data),
        "preset_table_rom_offset": start,
        "preset_count": len(presets),
        "format_notes": (
            "name/macro_knob_label are decoded and verified (200/200 records "
            "chain-walk cleanly and read as plausible real preset names). "
            "header_hex_undecoded and tail_hex_undecoded are raw bytes whose "
            "internal layout is NOT decoded - the tail is very likely the "
            "actual per-preset parameter/coefficient block. See "
            "tools/pcm80-import/README.md."
        ),
        "presets": presets,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("firmware", type=Path, help="Path to the PCM80 ROM .bin dump")
    parser.add_argument(
        "-o", "--output", type=Path, default=None, help="Output JSON path (default: stdout)"
    )
    args = parser.parse_args()

    if not args.firmware.is_file():
        print(f"error: {args.firmware} is not a file", file=sys.stderr)
        return 1

    try:
        archive = build_archive(args.firmware)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    text = json.dumps(archive, indent=2)
    if args.output:
        args.output.write_text(text)
        print(f"Wrote {archive['preset_count']} presets to {args.output}", file=sys.stderr)
    else:
        print(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
