# PCM80 preset importer

A standalone script that reads a Lexicon PCM80 host-CPU firmware ROM dump
(the M27C2001/DIP32 EPROM that sits on the control board, *not* the
Motorola 56000-series DSP program) and extracts its factory preset table
into a JSON archive: preset name, the front-panel "quick knob" label, and
- for the 10 algorithms Lexicon's own MIDI Implementation Details manual
documents - the fully decoded, named parameter set for each preset (Mix,
Reverb Time, Diffusion, every patch, etc.), plus the raw per-preset
parameter bytes as a fallback.

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
docstring in `extract_presets.py` for the exact byte layout). Record
*boundaries* were verified statistically across the full 200-record
chain - not guessed from one or two samples.

- **Preset name, macro-knob label** - solid. Verified by chain-walking an
  entire ROM and finding exactly 200 records (the PCM80's known factory
  preset count), every one of which decodes to a name that reads as a
  real, plausible preset ("Concert Hall", "Vox Chamber", "Rich Plate",
  "6 Vox Chorus", ...).
- **Bitpacked Effect Control Data** (everything after the name/label, to
  the end of the record) - this used to be three undecoded
  statistically-distinct hex zones. Lexicon's own MIDI Implementation
  Details manual (a separate document from the PCM81 User Guide already
  in `docs/references/`, covering the PCM80's SysEx protocol) documents
  this whole region as one continuous LSB-first bitpacked structure, and
  `tools/pcm80-import/pcm80lib/` implements that documented format:
  Soft Row Assignments, Unpatchable Parameter Information, the ADJUST
  Knob's initial value, each algorithm's own Patchable Parameter list
  (named, range-decoded values - percent, dB, Hz, ms, tempo-synced
  Echo:Beat form, enums, pan position, etc.), and the Patching
  Information (which MIDI/internal sources are patched to which
  parameters, and by how much). For a preset using one of the 10
  algorithms this manual documents (Plate, Chamber, Infinite, Inverse,
  Concert Hall, M-Band+Rvb, Glide>Hall, Chorus+Rvb, Res1>Plate,
  Res2>Plate - `algorithm_id` 0-9), the archive's `decoded` key holds
  this structured output. Presets using other algorithm IDs (expansion
  cards this manual doesn't cover) only get the raw hex fallback.

  **Validation status**: the bit-reader, Soft Row Assignments,
  Unpatchable Parameter Information, and Patchable Parameter Information
  logic were checked end-to-end against the manual's own four worked
  examples (two on Chorus+Rvb, one each on Plate and Glide>Hall), and
  every field decoded within rounding of the manual's documented display
  values. The other 6 algorithm tables share the same validated building
  blocks and were transcribed with the same care but aren't directly
  checked against a known answer - the manual only worked through 3 of
  the 10 algorithms. See `pcm80lib/decoder.py`'s module docstring for
  the full validation notes, known imprecisions in a couple of the range
  decode lookup tables, and a still-unresolved anomaly: about 19% of
  presets in a real v1.10 ROM (always the shortest ROM record for a
  given algorithm) don't contain enough bitpacked data for that
  algorithm's full documented field table, even though both the fixed
  header and (independently, directly from the manual) the Plate
  algorithm's own field table check out exactly. Those presets are
  flagged with `decoded.reliable: false` and a `decode_warning` rather
  than silently decoded wrong - use the raw hex fallback for them.

The raw hex zones (`knob_id_list_hex`, `range_flags_block_hex_undecoded`,
`parameter_value_block_hex`) are still included for every preset as a
fallback/cross-check, even where `decoded` is present.

## Copyright

The ROM, and every preset name/byte extracted from it, are Lexicon's
copyrighted property. This directory ships only the *importer script* -
never a ROM image, never a pre-extracted archive. Any archive you
generate with this tool is for your own personal reference with hardware
you own; don't redistribute it. `.gitignore` at the repo root blocks
`tools/**/*.bin` and any `tools/pcm80-import/*.json`/`*.csv` output so
this stays true even if you run the script inside a checkout of this
repo.
