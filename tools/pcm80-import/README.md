# PCM80 preset importer

A standalone script that reads a Lexicon PCM80 host-CPU firmware ROM dump
(the M27C2001/DIP32 EPROM that sits on the control board, *not* the
Motorola 56000-series DSP program) and extracts its factory preset table
into a JSON archive: preset name, the front-panel "quick knob" label, and
the raw per-preset parameter bytes.

This is unrelated to the rest of the DSP-Effects archive - it isn't part
of the `dsp/`/`patches/`/`plugin/` build, has no CMake wiring, and doesn't
feed into any of the Lexicon/Eventide-inspired engines here. It exists
purely as a firmware-archaeology/archival utility for people who own a
PCM80 and want a readable catalog of their unit's presets.

## Usage

```
python3 tools/pcm80-import/extract_presets.py /path/to/your-dump.bin -o archive.json
```

You need to supply your own ROM dump, read out of hardware you own. This
repo does not include, fetch, or ship any Lexicon ROM image.

## What's actually decoded

The preset table is a chain of variable-length records (see the
docstring in `extract_presets.py` for the exact byte layout). Confidence
by field:

- **Preset name, macro-knob label** - solid. Verified by chain-walking an
  entire ROM and finding exactly 200 records (the PCM80's known factory
  preset count), every one of which decodes to a name that reads as a
  real, plausible preset ("Concert Hall", "Vox Chamber", "Rich Plate",
  "6 Vox Chorus", ...).
- **Per-preset parameter tail** (60-200 bytes depending on the preset) -
  **not decoded**. This is almost certainly the actual knob-value/
  algorithm-coefficient block, but pinning down what each byte means
  would need either real hardware to correlate front-panel knob moves
  against, or a full disassembly of the firmware's patch-load routine.
  The script preserves these bytes as raw hex in the archive rather than
  guessing at field meanings.
- **The 16-byte per-record header** (mostly the offsets 0-11, since
  offset 12-13 is the known length field) - also not decoded.

If someone wants to push this further (e.g. against real hardware, or a
disassembly of the 80186 firmware), the raw hex is there to work from -
see `extract_presets.py`'s docstring for the exact offsets.

## Copyright

The ROM, and every preset name/byte extracted from it, are Lexicon's
copyrighted property. This directory ships only the *importer script* -
never a ROM image, never a pre-extracted archive. Any archive you
generate with this tool is for your own personal reference with hardware
you own; don't redistribute it. `.gitignore` at the repo root blocks
`tools/**/*.bin` and any `tools/pcm80-import/*.json`/`*.csv` output so
this stays true even if you run the script inside a checkout of this
repo.
