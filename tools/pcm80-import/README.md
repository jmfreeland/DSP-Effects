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
docstring in `extract_presets.py` for the exact byte layout, and the
methodology below). Every zone boundary was verified statistically
across the full 200-record chain - not guessed from one or two samples.
Confidence by zone:

- **Preset name, macro-knob label** - solid. Verified by chain-walking an
  entire ROM and finding exactly 200 records (the PCM80's known factory
  preset count), every one of which decodes to a name that reads as a
  real, plausible preset ("Concert Hall", "Vox Chamber", "Rich Plate",
  "6 Vox Chorus", ...).
- **Knob id-list field** (15 bytes right after the name/label) - boundary
  solid (verified: bytes 48-51 within it are zero in the large majority
  of records, i.e. classic zero-padding after a variable-length list).
  Contents read as a list of small parameter-ID-like tags (it rarely
  starts at 0, arguing against it being a value/step table) - semantics
  not confirmed.
- **Range/flags block** (13 bytes after that) - boundary solid (verified:
  this zone has dramatically lower byte-value cardinality across the 200
  records - as few as 2-15 distinct values per byte position - than the
  zone before or after it, i.e. it reads as boilerplate/enum data rather
  than real per-preset values). Byte semantics not decoded.
- **Parameter value block** (everything after that, to the end of the
  record) - boundary solid, and this is the strongest candidate for the
  actual per-preset DSP parameter values: it's consistently
  high-cardinality/continuous (20-80+ distinct byte values per position)
  across all 200 records regardless of each record's total length. This
  is very likely "the real value" in each preset. **Which byte index
  means which named parameter (Decay? Mix? Tone?) is NOT decoded** -
  that needs either real hardware to correlate front-panel knob moves
  against, or a full disassembly of the firmware's patch-load routine.
  Neither is attempted here.

The script preserves every zone as raw hex in the archive rather than
guessing at individual field meanings within them. If someone wants to
push this further (e.g. against real hardware, or a disassembly of the
80186 firmware), the raw hex is there to work from, already segmented
into the right regions - see `extract_presets.py`'s docstring for the
exact offsets and the statistical evidence behind each boundary.

## Copyright

The ROM, and every preset name/byte extracted from it, are Lexicon's
copyrighted property. This directory ships only the *importer script* -
never a ROM image, never a pre-extracted archive. Any archive you
generate with this tool is for your own personal reference with hardware
you own; don't redistribute it. `.gitignore` at the repo root blocks
`tools/**/*.bin` and any `tools/pcm80-import/*.json`/`*.csv` output so
this stays true even if you run the script inside a checkout of this
repo.
