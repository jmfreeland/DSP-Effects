# PCM81 preset importer

A standalone script that reads a Lexicon PCM81 firmware ROM dump and
extracts its factory preset table into a JSON archive - the exact same
schema `tools/pcm80-import` produces, and loadable by the same "Import
PCM80 Preset..." button in the Loom Browser plugin (see below).

## Usage

```
python3 tools/pcm81-import/extract_presets.py /path/to/your-dump.bin -o archive.json
```

You need to supply your own ROM dump, read out of hardware you own. This
repo does not include, fetch, or ship any Lexicon ROM image.

## What's actually decoded

The PCM81 turns out to be far more PCM80-compatible than its "MIDI
Implementation Chart can transmit/receive PCM80-format SysEx" note alone
suggests: its own internal factory preset table uses the *identical*
fixed-layout record header, preset name, and macro-knob label fields as
the PCM80 ROM, and for algorithm IDs 0-9 - the 10 algorithms shared with
the PCM80 (Plate, Chamber, Infinite, Inverse, Concert Hall, M-Band+Rvb,
Glide>Hall, Chorus+Rvb, Res1>Plate, Res2>Plate) - the bitpacked Effect
Control Data decodes with `tools/pcm80-import/pcm80lib`'s existing,
manual-verified decoder, completely unmodified. See
`extract_presets.py`'s module docstring for the cross-checks that confirm
this (preset names matching their decoded algorithm, "Prime Blue"
matching `docs/references/lexicon-pcm81-presets-menu.pdf` exactly).

In a real v1.00 factory ROM, 200 of the 300 factory presets use one of
those 10 shared algorithm IDs and get the full decoded/named parameter
treatment `tools/pcm80-import` documents (numeric/unit pairs, Soft Row
Assignments, Patching Information, the works - see that tool's own
README for the field-level detail, all of which applies unchanged here).

The other ~100 presets use algorithm IDs exclusive to the PCM81 - its
larger DSP supports a substantially bigger algorithm set than the PCM80
(DualChmb, DualPlt, DualInv, QuadHall, StereoChmb, VSOChmb, PitchCorrect,
and more - see `docs/references/lexicon-pcm81-presets-menu.pdf` and the
full User Guide). Lexicon's PCM81 MIDI Implementation Details document
(distinct from the User Guide, and not something Lexicon publishes
online - "these can be obtained directly from Lexicon") would presumably
document those algorithms' own Patchable Parameter tables the same way
the PCM80 manual documents its 10, but without it there's no public field
layout to decode against. Those presets get name/macro-knob-label plus
the raw hex fallback only, same treatment `tools/pcm80-import` gives any
non-legacy algorithm ID.

## Loading presets into the Loom Browser plugin

An archive produced by this script loads through the exact same "Import
PCM80 Preset..." button and `plugin/source/browser/pcm80/` code path as a
PCM80 archive - the archive schema is identical, and every
`importPcm80Preset()` already implemented for the shared 10 algorithms
(`PlateAdapter.h`, `ChamberAdapter.h`, `InfiniteAdapter.h`,
`InverseAdapter.h`, `ConcertHallAdapter.h`, `MBandRvbAdapter.h`,
`GlideHallAdapter.h`, `ChorusRvbAdapter.h`, `Res1PlateAdapter.h`,
`Res2PlateAdapter.h`) works against a PCM81-sourced archive without any
changes. Presets using PCM81-exclusive algorithms have no `decoded` key
and currently can't be imported into their corresponding engine
(`DualChmbAdapter.h`, `QuadHallAdapter.h`, `PitchCorrectAdapter.h`, etc.)
- only name/label are known for those.

## Copyright

The ROM, and every preset name/byte extracted from it, are Lexicon's
copyrighted property. This directory ships only the *importer script* -
never a ROM image, never a pre-extracted archive. `.gitignore` at the
repo root blocks `tools/**/*.bin` and any
`tools/pcm81-import/*.json`/`*.csv` output so this stays true even if you
run the script inside a checkout of this repo.
