# Lexicon PCM81-style Res1>Plate Algorithm

Stage 1 (functional): the fourth of the PCM81's five 6-Voice algorithms,
after Glide>Hall, Chorus+Rvb, and M-Band+Rvb (see
`docs/lexicon-pcm81-glide-hall.md`, `docs/lexicon-pcm81-chorus-rvb.md`,
`docs/lexicon-pcm81-mband-rvb.md`, and `docs/lexicon-pcm81-reference.md`).
Per `CLAUDE.md`'s Primitive → Component → Block → Graph layering:

- **New Primitives/Components**: none — reuses `StringVoice` (built for
  the Eventide H3000's String Modeller) unchanged as each of the six
  resonator voices, and `dsp::rt60ToGain()` (already used by Reverb
  Factory/Dense Room) for each voice's Duration control.
- **Block**: `dsp/include/dsp/algorithms/Res1Plate.h` — six
  `StringVoice` resonators (three excited by the left input, three by
  the right), each with independent Pitch/Level/Pan/Duration/HiCut,
  running **in series** with a fixed, owned `Plate` reverb instance.
- **Graph**: `dsp/include/dsp/graphs/Res1PlateAlgorithm.h` — the Block
  plus the Controls row's In Lvl/Pan, a Voice Diffusion stage, and the
  shared FX Width/Hi-Cut/Adjust/Mix chain.

## The Resonant Chord family: six resonators excited by the input itself

Per the manual: "The Resonant Chord effects use impulsive energy at the
inputs to excite six resonant voices (notes). The level, pitch, duration,
and high-frequency cutoff of the overtones for each voice are separately
controllable. Each voice can be panned independently. The voices resonate
to some degree with any input, but the most effective excitation contains
all frequencies, like percussion. Other instruments may give a quality of
tonal ambience in which certain notes rise ethereally from the
background. The output of the resonator is then fed into a stereo plate
reverb effect." This is architecturally distinct from every other 6-Voice
algorithm in this archive: there's no discrete "excitation" event or
delay-bank feedback loop shared across voices — each resonator is
continuously, independently driven by the raw input, exactly the shape
`StringVoice` (delay tuned to a pitch, feedback through a damping
lowpass) was already built for in `docs/eventide-string-modeller.md`,
just fed continuously rather than plucked. Reused here completely
unchanged, the first Karplus-Strong-style reuse across the Lexicon side
of this archive.

Per the shared 6-Voice convention (manual p.3-8, "Voices 1-3 ... panned
to the left. Voices 4-6 ... panned to the right"), Voices 1-3 are excited
by the left input, Voices 4-6 by the right.

## Chromatic pitch assignment without MIDI

Res1>Plate and its diatonic sibling Res2>Plate (`docs/lexicon-pcm81-res2-plate.md`)
differ only in how resonator pitch gets assigned: "The two algorithms
differ in the way pitches are assigned to the resonators. In Res1>Plate,
pitches are assigned to the six voices chromatically, in a round-robin.
If, for example, MIDI note numbers are used to assign pitch, the
resonators will constantly be re-tuned to the pitches of the last six
MIDI notes received." No consumer in this project implements MIDI input
(the same fact that has shaped every other MIDI-driven PCM81/H3000
feature — see e.g. `docs/eventide-band-delay.md`'s Note Mode skip), so
Res1>Plate substitutes six directly, independently settable `Pitch` (Hz)
controls for that round-robin note assignment — matching Band Delay's own
precedent of replacing a MIDI-note destination with a direct frequency,
rather than building an unusable round-robin state machine with nothing
to round-robin from.

## Duration as an RT60 target, not a raw feedback percentage

The manual lists Duration as a per-voice control distinct from a plain
feedback amount. `setVoiceDuration()` converts a seconds value into each
voice's `StringVoice::setFeedback()` gain via `rt60ToGain(delaySamples,
sampleRate, durationSeconds)`, so a voice's sustain time stays meaningful
and roughly pitch-independent — a low note (long delay, few loops per
second) and a high note (short delay, many loops per second) set to the
same Duration both audibly ring for about the same length of time, rather
than a fixed feedback coefficient making high notes ring far longer than
low ones per pass count.

## Known simplifications

- **Assign/Tuning/Active/Unison (the manual's Row 6 "Pitch" row) beyond
  plain pitch assignment aren't modeled.** The scanned excerpt's OCR of
  this row's exact cell layout wasn't reliable enough to trust (see
  `docs/lexicon-pcm81-reference.md`'s own note on this), and the clearly
  legible body text above fully specifies Level/Pitch/Duration/HiCut per
  voice — the four controls this Block implements — without describing
  Assign/Tuning/Active/Unison's exact behavior. Revisit if a clearer
  excerpt of that row turns up.
- **No per-voice input excitation mixing** (e.g. an "In Amt" style
  control) — every voice is excited at full strength by its assigned
  channel's raw input, matching the manual's plain "resonate to some
  degree with any input" rather than adding an unlisted extra knob.
- **FX/Rvb Width behavior** is the same `rotateStereoWidth()`
  reconstruction used throughout this archive's PCM81 side.

## Status

Verified via isolated Block smoke tests (`dsp::algorithms::Res1Plate`):
1. An impulse through a representative 6-voice chord (each voice a
   different pitch/pan/level) produces finite, bounded output for a full
   3-second render (no runaway feedback).
2. A single voice's ring frequency, measured by zero-crossing counting
   after letting the resonance settle onto its fundamental (a damped
   HiCut is needed for this measurement — a bright HiCut leaves early
   harmonics ringing comparably to the fundamental, which zero-crossing
   counting can't disentangle; this is a measurement-technique detail,
   not a Block bug), lands within 5% of its `setVoicePitch()` target
   (220Hz target, 212.5Hz measured).
3. Doubling Duration measurably slows the decay: RMS 1 second after
   excitation for a 3.0s-Duration voice is >5x that of a 0.3s-Duration
   voice at the same pitch.
4. Lower HiCut produces measurably darker (lower sample-to-sample
   difference sum) ringdown than a bright HiCut at the same pitch.
5. `FxMix=1` (fully reverbed) measures over 2x the RMS of `FxMix=0`
   (dry resonators only) two seconds after excitation — the resonators
   themselves have mostly decayed by then, so this confirms the series
   Plate reverb tail is genuinely audible.

`dsp_host_render res1_plate` renders a 6-second impulse response through
a spread two-octave chord (a root-third-fifth-octave-tenth-twelfth
voicing) with a monotonically-decaying RMS curve (-42.7dBFS down to below
-183dBFS). `patches/lexicon/res1_plate/` (that same chord wired by
default; 3-knob mapping: Left = all-voice Duration, Mid = FX Mix,
Right = Mix; footswitch press = bypass, hold = freeze) and the
`LexiconRes1PlatePlugin` JUCE target both build clean. The JUCE plugin
exposes every voice's Pitch/Level/Pan/Duration/HiCut individually,
verified by launching the actual Standalone build headlessly (Xvfb) and
confirming both the parameter list and the architecture diagram render
correctly. As with every other patch in this archive, the ARM
cross-toolchain isn't available in this sandbox, so the Patch adapter is
verified via `make -n` plus a host-compiler build under
`-fno-exceptions -fno-rtti`, not an actual `.endl` build.
