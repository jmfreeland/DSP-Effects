# Lexicon PCM81-style Res2>Plate Algorithm

Stage 1 (functional): the fifth and last of the PCM81's five 6-Voice
algorithms, after Glide>Hall, Chorus+Rvb, M-Band+Rvb, and Res1>Plate (see
`docs/lexicon-pcm81-glide-hall.md`, `docs/lexicon-pcm81-chorus-rvb.md`,
`docs/lexicon-pcm81-mband-rvb.md`, `docs/lexicon-pcm81-res1-plate.md`, and
`docs/lexicon-pcm81-reference.md`). Per `CLAUDE.md`'s Primitive →
Component → Block → Graph layering:

- **New Primitives/Components**: none — reuses `StringVoice` (Res1>Plate's
  own resonator, itself built for the Eventide H3000's String Modeller)
  unchanged, plus `PitchDetector` and `DiatonicScale.h`'s
  `HarmonicInterval`/`nearestScaleDegreeIndex`/`diatonicSemitones`
  (already built for the Eventide H3000's Diatonic Shift) for the
  pitch-tracking harmonization math.
- **Block**: `dsp/include/dsp/algorithms/Res2Plate.h` — Res1>Plate's exact
  topology (six `StringVoice` resonators, three excited by the left
  input, three by the right, in series with a fixed `Plate` reverb), but
  each voice is continuously retuned to a `HarmonicInterval` relative to
  a live-tracked input note rather than a fixed Hz.
- **Graph**: `dsp/include/dsp/graphs/Res2PlateAlgorithm.h` — the Block
  plus the Controls row's In Lvl/Pan, a Voice Diffusion stage, and the
  shared FX Width/Hi-Cut/Adjust/Mix chain.

## Res1 and Res2's only real difference: how pitch gets assigned

Per the manual: "The two algorithms differ in the way pitches are
assigned to the resonators. In Res1>Plate, pitches are assigned to the
six voices chromatically, in a round-robin... In Res2>Plate, pitches are
assigned to the six resonators diatonically — harmonized with the key,
scale, and root of your choice. If MIDI note numbers are used to assign
pitch, the resonators will constantly be re-tuned to harmonize with the
incoming notes." Everything else — the resonator excitation mechanism,
the Level/Duration/HiCut controls, the series Plate reverb, the
Voices-1-3-left/4-6-right convention — is identical to Res1>Plate (see
`docs/lexicon-pcm81-res1-plate.md`), so this Block is deliberately not a
from-scratch rewrite: it duplicates Res1Plate's voice/reverb/pan
bookkeeping (a DualDigiplex-style "two hand-rolled copies, not a
composition" call — the pitch-computation logic differs enough between
the two Blocks that sharing a base class would mostly just be indirection)
and replaces `setVoicePitch(Hz)` with `setVoiceInterval(HarmonicInterval)`.

## Diatonic harmonization without MIDI: reusing Diatonic Shift's own math

No consumer in this project implements MIDI input, so instead of MIDI
note numbers driving re-harmonization, this Block reuses the *exact*
mechanism the Eventide H3000's Diatonic Shift already built for the
identically-shaped problem ("the H3000 tracks your pitch and plays the
correct notes" — see `dsp/algorithms/DiatonicShift.h` and
`docs/eventide-diatonic-shift.md`): a `PitchDetector` tracks the mono sum
of the input, `nearestScaleDegreeIndex()` finds which scale degree the
tracked note is nearest to, and each voice's chosen `HarmonicInterval` is
applied in *scale-degree* space via `diatonicSemitones()` — a "third up"
from the 6th scale degree lands on the tonic (2 diatonic steps, not a
fixed 4 semitones), same as real diatonic harmony. `voiceShiftSemitones()`
in `Res2Plate.h` is a direct copy of `DiatonicShift::voiceShiftSemitones()`
(including its pedal-tone octave handling for `HarmonicInterval`'s
Low/High Tonic/Dominant Pedal choices), generalized from two voices to
six. `PitchDetector::frequencyHz()` already holds its last value through
silent/unvoiced stretches, and `updateVoicePitches()` additionally skips
retuning entirely while `hasPitch()` is false, so the resonators hold
their last harmony rather than collapsing when the input goes quiet.

## Known simplifications

- **Assign/Tuning/Active/Unison beyond plain pitch assignment aren't
  modeled** — same reasoning as Res1>Plate's own equivalent gap.
- **Monophonic tracking only** — like Diatonic Shift, the detector
  assumes a single tracked note at a time; no polyphonic pitch
  detection.
- **No fully-custom Scale 1/Scale 2 per-chromatic-note tables** — same
  gap as Diatonic Shift, for the same reason (arbitrary-cents harmony,
  not diatonic-degree-based, doesn't fit the `HarmonicInterval` model).
- **FX/Rvb Width behavior** is the same `rotateStereoWidth()`
  reconstruction used throughout this archive's PCM81 side.

## Status

Verified via isolated Block smoke tests (`dsp::algorithms::Res2Plate`):
1. A sustained 220Hz tone through a representative 6-voice patch produces
   finite, bounded output for a full second after the tone (no runaway
   feedback).
2. With Key=C/Scale=Major and Voice 0 set to `kThirdUp`, tracking a
   220Hz (A3) input and then measuring the resonator's own ringdown
   frequency (zero-crossing counting, same damped-HiCut technique
   Res1>Plate's test uses) lands within 15% of the hand-computed
   diatonic target: A is the 6th scale degree of C major, and a "third
   up" from the 6th degree is 2 diatonic steps (A→B→C), landing on C4
   (261.63Hz) rather than a fixed-interval 277.18Hz (a chromatic minor
   3rd) — confirming the scale-degree math, not just interval-in-cents
   math, is what's actually running.
3. Tracked frequency holds its last value through a following silent
   stretch rather than collapsing to 0.

`dsp_host_render res2_plate` renders a 220Hz A3 tone burst through the
default chord voicing (Third/Fifth/Octave/Second/Sixth/Seventh up from
the tracked note) with a correctly-tracked 220.2Hz reading and a
monotonically-decaying RMS curve after the tone ends. That render
disables Voice Diffusion for a clean demonstration: its short allpass
delay lengths (97/149 samples) are close enough to a 220Hz tone's own
period (~218 samples @ 48kHz) to measurably disturb the pitch tracker's
autocorrelation lag search when both are active together — this is a
demo-signal quirk of feeding a pure sine through short-delay diffusion
immediately ahead of an autocorrelation detector, not a Res2Plate bug (the
Block's own diatonic-harmony math is verified directly against a clean,
undiffused sine in the isolated smoke test above). `patches/lexicon/res2_plate/`
(that same chord voicing wired by default; 3-knob mapping: Left = Key
(chromatic root), Mid = FX Mix, Right = Mix; footswitch press = bypass,
hold = freeze) and the `LexiconRes2PlatePlugin` JUCE target both build
clean. The JUCE plugin exposes every voice's HarmonicInterval (as a
labeled choice menu, reusing `DiatonicShiftPluginProcessor`'s own
interval-name table)/Level/Pan/Duration/HiCut plus Key/Scale/Tune/pitch-
tracking range individually, verified by launching the actual Standalone
build headlessly (Xvfb) and confirming both the parameter list and the
architecture diagram render correctly. As with every other patch in this
archive, the ARM cross-toolchain isn't available in this sandbox, so the
Patch adapter is verified via `make -n` plus a host-compiler build under
`-fno-exceptions -fno-rtti`, not an actual `.endl` build.

This completes every algorithm in this archive's PCM81 6-Voice roadmap
(Glide>Hall, Chorus+Rvb, M-Band+Rvb, Res1>Plate, Res2>Plate) — the
7 Pitch-class algorithms (Quad>Hall, Dual-Chmb, Dual-Plt, Dual-Inv,
Stereo-Chmb, VSO-Chmb, Pitch Correct) remain the only unbuilt PCM81
algorithms, held pending primary-source manual pages for that class (see
`docs/lexicon-pcm81-reference.md`'s own gap note).
