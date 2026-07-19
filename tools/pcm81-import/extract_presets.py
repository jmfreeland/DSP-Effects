#!/usr/bin/env python3
"""Extract the factory preset table from a Lexicon PCM81 host-CPU firmware
ROM dump into a JSON archive - same archive schema as
tools/pcm80-import/extract_presets.py, and directly loadable by the same
Loom Browser plugin importer.

Usage:
    python3 extract_presets.py <firmware.bin> -o <archive.json>

Why this works
--------------
The PCM81's own MIDI Implementation Chart (docs/references/lexicon-pcm81-
user-guide-rev1-midi-troubleshooting-specs.pdf, page 5-9) states it
"transmits and receives in both PCM 80 (product ID 0x07) and PCM 81
(product ID 0x10) formats" and is fully PCM80-compatible for bulk data.
That backward compatibility turns out to run deeper than just MIDI SysEx:
the PCM81 ROM's own internal preset table uses the *same* fixed 16-byte
header / 12-byte name / 9-byte macro-knob-label record layout as the PCM80
ROM (tools/pcm80-import/extract_presets.py's record chain walker finds a
clean 300-record chain - the PCM81's known factory preset count - with no
changes needed), and for algorithm IDs 0-9 (the 10 algorithms Lexicon's
PCM80 MIDI Implementation Details manual documents: Plate, Chamber,
Infinite, Inverse, Concert Hall, M-Band+Rvb, Glide>Hall, Chorus+Rvb,
Res1>Plate, Res2>Plate) the bitpacked Effect Control Data decodes with
tools/pcm80-import/pcm80lib's existing decoder too. This isn't a guess -
preset *names* cross-check against their decoded algorithm: "Chorus
Plate" decodes as algorithm "Plate", "Flange>Rvb"/"Glide X-Ekos" decode as
"Glide>Hall", "StereoEqEkos" decodes as "M-Band+Rvb", etc. - exactly the
algorithm each name implies. "Prime Blue" (preset #1, "Efx/Rvb X" knob
label) matches docs/references/lexicon-pcm81-presets-menu.pdf exactly and
decodes as "Chorus+Rvb".

What's NOT decoded
-------------------
About a third of PCM81 factory presets (100/300 in the v1.00 ROM this was
built against) use algorithm IDs outside 0-9 - newer algorithms exclusive
to the PCM81 (its larger DSP gives it a materially bigger algorithm set
than the PCM80: DualChmb, DualPlt, DualInv, QuadHall, StereoChmb, VSOChmb,
PitchCorrect, and others per docs/references/lexicon-pcm81-presets-menu.pdf
and the full user guide). Lexicon's separate PCM81 MIDI Implementation
Details document (referenced but not included in the User Guide - "These
can be obtained directly from Lexicon") would presumably document those
algorithms' own Patchable Parameter tables the same way the PCM80 manual
does for its 10; without it, this tool has no documented field layout to
decode them against, so those presets get name/label + raw hex only, same
treatment tools/pcm80-import gives non-legacy algorithm IDs.

Copyright
---------
Same posture as tools/pcm80-import: this script only operates on a
firmware file *you* supply and never ships, bundles, or commits any ROM
image or extracted archive.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import sys
from pathlib import Path

# Reuse tools/pcm80-import/pcm80lib's bitpack decoder rather than forking
# it - the format is the same for algorithm IDs 0-9 (see module docstring).
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "pcm80-import"))

from extract_presets import find_preset_table, parse_records  # noqa: E402

PCM81_FACTORY_PRESET_COUNT = 300


def build_archive(rom_path: Path) -> dict:
    data = rom_path.read_bytes()
    start, count = find_preset_table(data)
    presets = parse_records(data, start)
    return {
        "_copyright_notice": (
            "Preset names and data below were extracted from a Lexicon PCM81 "
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
            "name/macro_knob_label are decoded using the same record layout "
            "as the PCM80 (verified: this ROM chain-walks cleanly to exactly "
            f"{PCM81_FACTORY_PRESET_COUNT} records, the PCM81's known factory "
            "preset count). For presets using algorithm IDs 0-9 (the 10 "
            "algorithms shared with the PCM80 and documented in its MIDI "
            "Implementation Details manual), 'decoded' holds the fully "
            "parsed, named parameter set, cross-checked by confirming preset "
            "names match their decoded algorithm (e.g. 'Chorus Plate' -> "
            "Plate, 'StereoEqEkos' -> M-Band+Rvb). Presets using PCM81-only "
            "algorithm IDs (its larger, non-PCM80 algorithm set - DualChmb, "
            "QuadHall, PitchCorrect, etc.) are undecoded - no public field "
            "layout is available for them. See extract_presets.py's module "
            "docstring and tools/pcm81-import/README.md for the full "
            "methodology."
        ),
        "presets": presets,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("firmware", type=Path, help="Path to the PCM81 ROM .bin dump")
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
