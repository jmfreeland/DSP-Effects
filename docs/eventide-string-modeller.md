# Eventide H3000-style String Modeller

Stage 1 (functional): the nineteenth Eventide H3000 algorithm in this
archive, Algorithm 118 per the Instruction Manual's own numbering (right
after Band Delay, Algorithm 117 - see `docs/eventide-h3000-notes.md` and
`docs/eventide-band-delay.md`). Per `CLAUDE.md`'s Primitive → Component →
Block → Graph layering:

- **New Component**: `dsp/include/dsp/StringVoice.h` - a single
  Karplus-Strong string: `DelayLine` + feedback path through an
  `OnePoleLowpass` (damping) + a small one-zero DC blocker (see "A real
  bug" below), continuously re-tuned by delay length. Composes only
  existing Primitives - no new Primitive was needed.
- **Block**: `dsp/include/dsp/algorithms/StringModeller.h` - six
  `StringVoice`s excited by a shared stimulation signal (a
  `StateVariableFilter`, reused a fourth time after Patch Factory/
  Vocoder/Band Delay, fed by `NoiseGenerator`; plus the live input
  directly), feeding a `LFO`-modulated-delay Chorus that widens the mono
  resonator sum to stereo.
- **Graph**: `dsp/include/dsp/graphs/StringModellerAlgorithm.h` - the
  Block plus input trim (Left-In only, per the manual's own Block
  Diagram).

## Why this algorithm, nineteenth

Straight numeric order. Per the manual: "This algorithm digitally
simulates a set of six strings. When processing audio input, these
strings act as passive resonators, yielding a sound similar to singing
into a piano while holding down the damper pedal. To generate some
amazingly realistic sounds, the 'strings' can be 'plucked' by playing
notes on a MIDI keyboard." The "Detail of Voice" block diagram is a
textbook Karplus-Strong topology: `In -> (+) -> Delay -> Filter ->
[feedback to the (+) junction] -> Out`, so this is the first algorithm in
the archive to need that specific physical-modeling shape, even though
every piece it's built from (`DelayLine`, `OnePoleLowpass`) already
existed.

## No MIDI means "Open" Gate Mode plus a manual pluck

No consumer in this project (the Polyend Endless Patch, the native host
harness, or the JUCE plugin) implements MIDI input - the same fact that
already shaped Band Delay's Note Mode skip. The manual's own Gate Mode
parameter (#5: Normal/Keyed/Open) picks between three ways a MIDI
note-on drives the strings; only **Open** - "will stimulate the strings
constantly, regardless of whether any keys are pressed... Decay and
Release parameters are ignored" - produces any sound at all without MIDI,
so this Block runs as if permanently in that mode: the shared noise/input
stimulation signal excites all six strings continuously, every sample.

A `trigger()` method stands in for the missing MIDI note-on: it fires a
single shaped noise burst (envelope duration set by Gate, #4) into all
six strings at once - a hands-on "pluck," wired to the Endless
footswitch's hold action, additive on top of the continuous stimulation
rather than a replacement for it.

Decay (#1) and Release (#2) jointly describe a string's behavior across a
MIDI note being held and then released; without note-on/off events
there's no "held" state distinct from "released," so this Block collapses
them into a single Decay control governing each string's feedback/
resonance gain at all times. Sustain (#3) and Gate Mode (#5) are dropped
for the same reason. Hold (#6) and Offset (#7) exist only to manage
MIDI-driven tuning and are dropped too. Note 1-6 (#29-34) survive as a
direct settable Hz per string (`setNoteHz()`) - the manual's own
non-MIDI tuning path: "The strings can be tuned either manually (by
setting the 'note' parameters) or with a MIDI keyboard." The velocity-
and key-range-scaling expert parameters (#20-28, #35-40) are dropped
outright, since they only ever modulate a MIDI event's effect.

Six strings, tuned literally: `setNoteHz()`'s defaults are standard
guitar open-string tuning (E2/A2/D3/G3/B3/E4) rather than an arbitrary
scale - a natural default for a model whose own manual calls it "a set of
six strings."

## The stimulation signal: three sources, matching the manual's own worked example

Per the manual's own "Interesting Ideas": "Instead of using noise to
stimulate the strings, use the external input signal instead... To set
up this algorithm as a sympathetic resonator, set 'high amt', 'band amt'
and 'low amt' to 0. Set 'in amt' to about 20 per cent and set the gate
mode to 'open'. The strings will now resonate with the input signal."
This Block implements exactly that split: a shared `StateVariableFilter`
(fed by `NoiseGenerator`, tuned by Freq/Qfac) produces simultaneous low/
band/high outputs, mixed by Low/Band/High Amt (#11-13) into a "filtered
noise" signal; the live input is mixed in **separately and unfiltered**,
weighted by In Amt (#14) - so zeroing the noise mix and setting In Amt
alone reproduces the manual's own sympathetic-resonator recipe.

## A real bug: DC survives a one-pole lowpass at any cutoff

Karplus-Strong's damping filter (`Bright`, #10) is a one-pole lowpass in
each string's feedback loop. A first implementation used
`OnePoleLowpass` alone and failed its own smoke test outright: tuning all
six strings to 220Hz, plucking them, and checking the ringing tail's
fundamental with `dsp::PitchDetector` (reused from Diatonic Shift)
returned a slowly-decaying near-DC signal, not 220Hz.

The cause: a one-pole lowpass has **unity gain at DC regardless of
cutoff frequency** - only frequencies above cutoff are attenuated. The
loop's overall per-round-trip gain at DC is therefore always exactly the
configured feedback (resonance) gain, while the tuned pitch gets
additionally attenuated by the damping filter every round trip. At a low
`Bright` setting (a dark, heavily-damped string) the tuned pitch decays
far faster than the feedback gain alone would suggest, while any residual
DC bias in the excitation signal - a short noise burst rarely averages to
exactly zero - decays at the full, much slower feedback-only rate. A
plucked string then audibly outlives its own pitch, decaying from a tone
into a DC-biased thump instead of ringing out cleanly. This is a known
failure mode of naive Karplus-Strong implementations, not specific to
this codebase - real implementations universally include a DC-blocking
stage in the loop for exactly this reason. **Fixed** by adding a small
one-zero DC blocker (`StringVoice::blockDc()`,
`y[n] = x[n] - x[n-1] + 0.995*y[n-1]`) after the damping filter, inside
the feedback loop, removing the DC mode without touching the rest of the
spectrum. Re-verified with the same test: `PitchDetector` now reports
219.2Hz and 436.4Hz for 220Hz/440Hz-tuned plucks (within 1%), confirming
the fix.

## Chorus: an original reconstruction from the "Detail Of Chorus" diagram

The manual's own "Detail Of Chorus" diagram shows `In -> Delay -> a +/-
combining stage -> two Out signals` - a mono-to-stereo modulated-delay
widener, described but not derived from a formula. This Block implements
it directly: an `LFO::nextSine()`-modulated single `DelayLine` (Speed and
Depth per #16-17), combined as `left = mono + chorusMix*delayed`,
`right = mono - chorusMix*delayed`, matching the diagram's "+/-" stage
literally. Depth (#17) is documented in the manual as reaching "~300ms"
at 100 - an unusually large modulation range for a "chorus" in the
conventional sense, closer to a slow pitch-wobble - implemented as-stated
rather than assumed to be a typo.

## Known simplifications

- **No MIDI-driven note-on/tuning, Note Mode, or velocity/key-range
  scaling** - see above; the entire relevant "Things to be Aware of"
  section (MIDI response sluggishness, modulation-patched parameter
  timing) is MIDI-specific and doesn't apply here either.
- **Decay is the sole per-string dynamics control** - Release and
  Sustain have no MIDI note-off event to key off of.
- **Gate Mode is always effectively "Open"** - the only mode that
  produces sound without MIDI.
- **Mono-in** - the manual's own Block Diagram draws only "Left Input"
  feeding the stimulation filter.

## Status

Verified via `StringModeller` Block smoke tests:
1. Continuous (Open-mode) noise stimulation with default settings
   produces finite, non-silent output.
2. Six strings tuned to 220Hz, plucked with no continuous stimulation:
   `dsp::PitchDetector` measures the ringing tail's fundamental at
   ~219.2Hz.
3. The same test with Pitch=+12 semitones measures ~436.4Hz - confirming
   the global Pitch offset applies correctly.
4. `trigger()` with all stimulation sources at 0 produces a real,
   decaying burst (early-window RMS well above late-window RMS) - the
   pluck is genuinely additive, not a no-op.
5. Chorus engaged (100% wet, moderate Speed/Depth) produces a measurable
   Left/Right difference, confirming the mono-to-stereo widening is real.

`dsp_host_render string_modeller` renders two seconds of silence
followed by a manual pluck of all six strings (standard guitar tuning),
with finite output and a printed RMS decay curve confirming the ringing
actually decays. `patches/eventide/string_modeller/` (3-knob mapping:
Left = Pitch, Mid = Decay, Right = Mix; footswitch press = bypass, hold =
manually pluck all six strings - the algorithm's core gesture, since no
MIDI keyboard exists to trigger them; `init()` also sets a modest default
In Amt so the guitar-pedal use case - the strings resonating
sympathetically with whatever's plugged in - works out of the box) and
the `EventideStringModellerPlugin` JUCE target both build clean. The JUCE
plugin exposes the full parameter set (Pitch/Decay/Gate/Freq/Qfac/
Bright/High-Band-Low-In Amt/Chorus/Speed/Depth/Mix, all six Note Hz
values) plus the pluck trigger as a self-resetting momentary checkbox,
verified by launching the actual Standalone build headlessly (Xvfb) and
confirming both the parameter list and the architecture diagram render
correctly. As with the other patches in this archive, the ARM
cross-toolchain isn't available in this sandbox, so the Patch adapter is
verified via `make -n` plus a host-compiler build under
`-fno-exceptions -fno-rtti`, not an actual `.endl` build.
