# PCM80/81 "Echo:Beat" tempo-sync field decoding

Several Patchable Parameters that are normally a plain time value (e.g.
Chorus+Rvb's `DelayTime Voice1-6`, `Rvb Time Pre Delay`/`RefDly L/R`/
`EkoDly L/R`) can instead be driven by the front-panel Tempo Mode: when a
1-bit "Tempo Flag" preceding the field is set, the field's raw value no
longer means "N milliseconds" - it displays as `X: Y Echo:Beat` instead
(e.g. `"19: 20 Echo:Beat"`).

## Why this mattered

`tools/pcm80-import`'s decoder correctly recognized this format and
displayed it (`decode_tempo_value()`), but never converted it to a real
millisecond value - the module docstring flagged this explicitly as
"the manual doesn't fully spell out the echoes/beats arithmetic - out of
scope for now." Every `EngineAdapter::importPcm80Preset()` skips any
field without a `numeric` value, so a tempo-synced field silently fell
back to whatever short default the target parameter already had.

This had a real, audible consequence: Chorus+Rvb's own factory "Prime
Blue" preset has two of its six chorus voices (Voice 2, Voice 4) tempo-
synced. On real hardware those voices produce a long, clearly audible
delay repeat; imported into this project's engine, they silently fell
back to short (25-35ms) chorus-range defaults, discarding an entire
audible element of the preset. Diagnosed 2026-07-19 by comparing a real
PCM81 hardware recording of Prime Blue against this codebase's own
render of the same preset.

## The format

Confirmed against the actual Lexicon **PCM80 MIDI Implementation
Details** manual (a separate document from the PCM81 User Guide already
in `docs/references/` - not itself committed here, see Copyright below),
specifically its "Parameter" SysEx message description (live MIDI
automation of a single parameter):

> Byte 8: Tempo Mode Flag - "Defines the value as an absolute 16-bit
> value (0) or as a ratio split into bytes (1)."
> Bytes 9-10: "Least-significant/next nibble of absolute value or
> **numerator** byte."
> Bytes 11-12: "Next/most-significant nibble of absolute value or
> **denominator** byte."

And later, in the Patch Assignment SysEx message's own tempo-point
fields: `"Tempo value Numerator (1-24)"` / `"Tempo value Denominator
(1-24)"` - each a separate small integer, range 1-24.

In the ROM's own bitpacked Effect Control Data, a tempo-active field's
raw value (a single N-bit word, N = the field's normal absolute-mode
width) packs both numbers into one value: the numerator in the upper 6
bits, the denominator in the lower 5 bits - exactly the split this
project's `decode_tempo_value()` already used (`raw >> 5`, `raw & 0x1F`),
which is how it already reproduced the manual's own display strings
verbatim (see Validation below). The manual's own "Echo:Beat" label
names these two numbers Echo (numerator) and Beat (denominator).

## The time formula

The manual never states the numerator:denominator -> milliseconds
formula in prose. It was reconstructed from the manual's own two full
worked examples for Chorus+Rvb (Prime Blue, RandomImages), each of which
prints both the raw Echo:Beat display *and* that same preset's own
patched Tempo Rate (a per-preset value, stored in the fixed Unpatchable
Parameter Information header every algorithm shares - not a global/DAW
tempo):

```
time_ms = (beat / echo) * (60000 / preset_bpm)
```

Where `preset_bpm` is that preset's own decoded `Tempo Rate` (Range
Decode 1: raw value + 40 BPM) and `60000 / preset_bpm` is the duration
of one "beat" as defined by that preset's `BeatValue` (the manual: "if
the rate is 120 BPM, and you select eighth-note here, the tempo will be
120 eighth-notes per minute" - i.e. BPM is already scaled to whatever
BeatValue is set to, no separate note-length multiplier needed).

### Validation against the manual's own numbers

| Preset | Field | Echo:Beat | Preset BPM | Formula result | Plausible? |
|---|---|---|---|---|---|
| Prime Blue | Pre Delay | 12:1 | 81 | 61.7ms | yes - typical pre-delay |
| Prime Blue | RefDly L | 12:1 | 81 | 61.7ms | yes |
| Prime Blue | RefDly R | 8:1 | 81 | 92.6ms | yes |
| Prime Blue | DelayTime Voice2 | 19:20 | 81 | 779.7ms | yes - audible echo tap |
| RandomImages | EkoDly L | 1:4 | 120 | 2000ms | plausible (long pre-echo) |
| RandomImages | EkoDly R | 1:2 | 120 | 1000ms | plausible |
| RandomImages | DelayTime Voice3 | 1:2 | 120 | 1000ms | plausible |
| RandomImages | DelayTime Voice6 | 1:4 | 120 | 2000ms | plausible |

The two `EkoDly`/`Voice3`/`Voice6` results exceed those parameters' own
*absolute-mode* maximum range (e.g. `DelayTime Voice` tops out at 1365ms
in absolute mode) - this project's importer already clamps every
imported value to the target engine parameter's own valid range, so an
overshoot degrades gracefully (clamped, not wrong-unit-garbage) rather
than being a correctness risk. Whether tempo mode genuinely allows
exceeding the absolute-mode range on real hardware, or the formula needs
a further refinement for that specific case, is not yet confirmed.

**Not yet handled**: "Cycl:Beat" fields (tempo-synced LFO Rate, not a
delay/time parameter) need a *frequency* formula, not a time one - out
of scope here, see `decode_tempo_value()`'s doc comment.

## Copyright

Same posture as everything else under `tools/pcm80-import/`: this
document describes a reverse-engineered *format*, derived from and
checked against a manual excerpt, but does not reproduce that manual's
copyrighted text, tables, or images.
