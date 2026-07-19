#!/usr/bin/env python3
"""Extract the factory preset table from a Lexicon PCM80 host-CPU firmware
ROM dump into a JSON archive.

Usage:
    python3 extract_presets.py <firmware.bin> -o <archive.json>

What this actually decodes
---------------------------
The PCM80's host/UI controller is an Intel 80186-class CPU (separate from
the Motorola 56000-series DSP that does the actual audio processing). Its
ROM stores factory presets as a chain of variable-length records. Record
*boundaries* were verified statistically across a full 200-record chain
(the PCM80's known factory preset count) - not guessed from one or two
samples.

    offset  size  field                    confidence
    0       16    header (see below)       low (mostly), one universal constant
    16      12    preset name              SOLID - see verification note
    28      9     "quick knob" label       SOLID - see verification note
    37      *     bitpacked Effect Control Data (see pcm80lib/)

Header (offset 0-15): offset 12-13 is a little-endian uint16 record length
(the distance to the next record - this is how the chain is walked).
Offset 15 is a universal constant, 0xF0, across all 200 records (Soft Row
Slot 0). Offset 14 is the Algorithm ID (0-9 for the 10 algorithms this
manual documents; other values are presumably expansion-card algorithms
not covered here). The other 12 bytes vary too much (56-100+ distinct
values across 200 records) to characterize; not decoded.

Name/label (offset 16-37): SOLID. Verified by chain-walking an entire ROM
and finding exactly 200 records, matching the PCM80's known factory
preset count, with every name decoding to a plausible real preset
("Concert Hall", "Vox Chamber", "Rich Plate", "6 Vox Chorus", ...).

Bitpacked Effect Control Data (offset 37 to end of record): this used to
be three undecoded statistically-distinct "zones" (a byte previously
misidentified as a terminator, a "knob id-list", and a "range/flags
block") before Lexicon's own MIDI Implementation Details manual became
available. That manual documents this whole region as one continuous
LSB-first bitpacked structure (Soft Row Assignments, Unpatchable
Parameter Information, ADJUST Knob Initial Value, per-algorithm
Patchable Parameter Information, then Patching Information) - see
pcm80lib/decoder.py for the decoder and its validation status. For
algorithms 0-9, the "decoded" key below is now that structured, named
data - this really is "the real value" the earlier zone-splitting was
only estimating the boundaries of. pcm80lib/decoder.py documents a
still-unresolved anomaly where ~19% of presets (always the shortest ROM
record for a given algorithm) don't contain enough bitpacked data for
the algorithm's full documented field table; those are flagged rather
than silently decoded wrong. The three legacy hex zones are still
included as a raw fallback for every preset (useful for algorithms
outside 0-9, and as a cross-check).

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

from pcm80lib.decoder import decode_preset, is_algorithm_decodable

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
        algorithm_id = data[pos + 14]
        bitpack = data[pos + TERMINATOR_OFFSET : pos + length]

        preset = {
            "index": index,
            "name": name,
            "macro_knob_label": label,
            "rom_offset": pos,
            "record_length": length,
            "algorithm_id": algorithm_id,
            "header_hex_undecoded": header.hex(),
            "knob_id_list_hex": knob_field.hex(),
            "range_flags_block_hex_undecoded": flags_block.hex(),
            "parameter_value_block_hex": param_block.hex(),
        }

        if is_algorithm_decodable(algorithm_id):
            decoded = decode_preset(name, label, algorithm_id, bitpack)
            preset["decoded"] = decoded
            if not decoded["reliable"]:
                preset["decode_warning"] = (
                    "This record is shorter than the fields documented for its algorithm "
                    "require - see pcm80lib/decoder.py's 'Known unresolved anomaly' note. "
                    "Fields near the end of 'decoded.patchable' and everything in "
                    "'decoded.patches' were read past the end of valid data and are not "
                    "trustworthy; raw hex zones above are the fallback for this preset."
                )
        else:
            preset["decode_warning"] = (
                f"Algorithm ID {algorithm_id} is not one of the 10 base algorithms the "
                "MIDI Implementation Details manual documents (0-9) - likely an expansion "
                "card algorithm. Only raw hex zones are available for this preset."
            )

        presets.append(preset)
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
            "For presets using algorithm IDs 0-9 (the 10 algorithms Lexicon's "
            "MIDI Implementation Details manual documents), 'decoded' holds "
            "the fully parsed, named parameter set - see pcm80lib/decoder.py "
            "for validation status per algorithm and a known unresolved "
            "anomaly affecting a minority of (flagged) presets. Presets using "
            "other algorithm IDs, and the trailing raw hex zones on every "
            "preset, are undecoded - see the module docstring in "
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
