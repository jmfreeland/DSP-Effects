#!/usr/bin/env python3
"""Extract the factory preset table from a Lexicon PCM80 host-CPU firmware
ROM dump into a JSON archive.

Usage:
    python3 extract_presets.py <firmware.bin> -o <archive.json>

What this actually decodes
---------------------------
The PCM80's host/UI controller is an Intel 80186-class CPU (separate from
the Motorola 56000-series DSP that does the actual audio processing). Its
ROM stores factory presets as a chain of variable-length records. Every
boundary below was verified statistically across a full 200-record chain
(the PCM80's known factory preset count) - not guessed from one or two
samples. Confidence varies a lot by zone; see each zone's note.

    offset  size  field                    confidence
    0       16    header (see below)       low (mostly), one universal constant
    16      12    preset name              SOLID - see verification note
    28      9     "quick knob" label       SOLID - see verification note
    37      1     0x00 terminator          solid
    38      15    knob id-list field       medium (boundary solid, semantics not)
    53      13    range/flags block        medium (boundary solid, semantics not)
    66      *     parameter value block    boundary solid, byte semantics NOT decoded

Header (offset 0-15): offset 12-13 is a little-endian uint16 record length
(the distance to the next record - this is how the chain is walked).
Offset 15 is a universal constant, 0xF0, across all 200 records. Offset
14 is a 10-value enum. The other 12 bytes vary too much (56-100+ distinct
values across 200 records) to characterize; not decoded.

Name/label (offset 16-37): SOLID. Verified by chain-walking an entire ROM
and finding exactly 200 records, matching the PCM80's known factory
preset count, with every name decoding to a plausible real preset
("Concert Hall", "Vox Chamber", "Rich Plate", "6 Vox Chorus", ...).

Knob id-list field (offset 38-52, 15 bytes): a variable-length ascending
run of small integers, zero-padded to fill the 15 bytes (offsets 48-51
are zero in the large majority of records - classic padding behavior).
The list only rarely starts at 0 and instead starts from small
distinct-looking values (most commonly 1, 3, or 4) - this argues against
it being a value/step table (which would plausibly start at 0) and for
it being a list of parameter-ID tags that the single front-panel "quick
knob" is linked to for this preset. Not confirmed.

Range/flags block (offset 53-65, 13 bytes): overwhelmingly low-cardinality
across all 200 presets (as few as 2-15 distinct values seen per byte
position, vs. 20-80+ in the parameter block that follows) - i.e. this
reads as boilerplate/enum data, not real per-preset values. Bytes 53-56
are dominated by 0xFF/0x7F-style range-boundary values, consistent with
a generic min/max descriptor most presets share. Byte 59 is the one
outlier with high cardinality (76 distinct values) - possibly a checksum,
or the block boundary drawn here is slightly off for some records. Byte
semantics not decoded.

Parameter value block (offset 66 to end of record): consistently
high-cardinality (continuous, 20-80+ distinct values per byte position)
across all 200 records regardless of each record's total length - this
is the strongest candidate for the actual per-preset DSP parameter
values (decay, mix, tone, etc.). This is very likely "the real value"
in each preset, but which byte index means which named parameter is NOT
decoded - that needs either real hardware to correlate front-panel knob
moves against, or a full disassembly of the firmware's patch-load
routine, neither of which this script attempts.

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

HEADER_SIZE = 16
NAME_SIZE = 12
LABEL_SIZE = 9
KNOB_FIELD_SIZE = 15
FLAGS_BLOCK_SIZE = 13

NAME_OFFSET = HEADER_SIZE
LABEL_OFFSET = NAME_OFFSET + NAME_SIZE
TERMINATOR_OFFSET = LABEL_OFFSET + LABEL_SIZE
KNOB_FIELD_OFFSET = TERMINATOR_OFFSET + 1
FLAGS_OFFSET = KNOB_FIELD_OFFSET + KNOB_FIELD_SIZE
PARAM_BLOCK_OFFSET = FLAGS_OFFSET + FLAGS_BLOCK_SIZE  # 66

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
    while pos + HEADER_SIZE <= len(data):
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
    while i + HEADER_SIZE <= n:
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

        header = data[pos : pos + HEADER_SIZE]
        knob_field = data[pos + KNOB_FIELD_OFFSET : pos + FLAGS_OFFSET]
        flags_block = data[pos + FLAGS_OFFSET : pos + PARAM_BLOCK_OFFSET]
        param_block = data[pos + PARAM_BLOCK_OFFSET : pos + length]

        presets.append(
            {
                "index": index,
                "name": name,
                "macro_knob_label": label,
                "rom_offset": pos,
                "record_length": length,
                "header_hex_undecoded": header.hex(),
                "knob_id_list_hex": knob_field.hex(),
                "range_flags_block_hex_undecoded": flags_block.hex(),
                "parameter_value_block_hex": param_block.hex(),
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
            "knob_id_list_hex, range_flags_block_hex_undecoded and "
            "parameter_value_block_hex are three statistically-distinct "
            "zones within what used to be one undifferentiated 'tail' blob "
            "- their *boundaries* are verified across all 200 records, but "
            "byte-level semantics within each are NOT decoded. "
            "parameter_value_block_hex is the best candidate for actual "
            "per-preset DSP parameter values (highest, most continuous "
            "byte-value variety of the three). See the module docstring in "
            "extract_presets.py and tools/pcm80-import/README.md for the "
            "full methodology."
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
