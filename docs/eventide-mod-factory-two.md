# Eventide H3000-style mod factory|two

Stage 1 (functional): the twenty-third and final Eventide H3000
algorithm in this archive, Algorithm 123 per the Instruction Manual's own
numbering (right after mod factory|one, Algorithm 122 - see
`docs/eventide-h3000-notes.md` and `docs/eventide-mod-factory-one.md`).
Per `CLAUDE.md`'s Primitive → Component → Block → Graph layering:

- **New Primitives**: none - reuses `MultiWaveLFO` and `EnvelopeDucker`
  (both built for mod factory|one) unchanged, plus `PitchShiftVoice`
  (Delay + `PitchShifter`, already used across the Shift-family
  algorithms) for its Detuner module.
- **Block**: `dsp/include/dsp/algorithms/ModFactoryTwo.h` - a smaller
  cousin to mod factory|one: 2 filtered delays, 2 detuning pitch
  shifters, one `MultiWaveLFO`, one `EnvelopeDucker`, 2 amplitude
  modulators, and 4 two-input mixers, wired by a settable
  28-destination x 22-source patch matrix.
- **Graph**: `dsp/include/dsp/graphs/ModFactoryTwoAlgorithm.h` - the
  Block plus independent Left/Right input trim.

## Why this algorithm, twenty-third, and the last one

Straight numeric order - and the final algorithm in this archive's
Eventide H3000 roadmap. Per the manual's own page: "This algorithm is a
cousin to algorithm #122, mod factory|one. This too is a 'modular'
effects processing algorithm... The main building blocks are a pair of
sweepable, filtered delays, a pair of detuning pitch shifters, one
low-frequency oscillator, one envelope detector, and two amplitude
modulators." Trading one LFO and one envelope detector (mod factory|one
has two of each) for two genuinely new modules - filtered delays and
detuners - shrinks the patch matrix from 28x26 down to 28x22, but the
underlying mechanism (the same one-sample-latency
`setPatch(Destination, Source)` technique) is identical.

## Filtered delays: mod factory|one's delays plus a highcut

Per the manual: "The filtered delay modules work just like those in mod
factory|one with the added feature of adjustable high frequency
rolloffs for each of the delays. This allows for warm, natural sounding
delays." Implemented by reusing `OnePoleLowpass` (already in the
archive) on each delay's read tap, with its own settable cutoff and
modulation input (#16/19: `dly1 ctmd`/`dly2 ctmd`), applied before the
result is fed back into the delay line's own feedback path - so the
recirculating signal darkens on each repeat, matching real analog delay
character.

## Detuners: exactly the shape `PitchShiftVoice` already is

Per the manual: "This algorithm contains two detuning modules. The most
common use of these modules is to slightly shift the pitch on the left
and right channels to create a very rich chorus effect... For a moderate
chorus effect the left and right channels are usually shifted plus and
minus ten cents." Its own parameter list - Detune (cents), Delay (ms,
BPM-syncable), Mod Amount (cents) - describes precisely the "Delay
feeding a PitchShifter" shape `PitchShiftVoice` already is (the repeated
unit behind Layered/Dual/Stereo/Multi-Shift - see
`docs/eventide-h3000-notes.md`), reused here completely unchanged rather
than hand-rolled again.

The manual's own Fadelength and Splice Length are two distinct
parameters - a crossfade portion and a segment length - but
`PitchShifter`'s single `setGrainSeconds()` parameter (whose triangular
crossfade window spans the *entire* grain) doesn't separate those two
concepts. Splice Length maps onto `setGrainSeconds()` directly; Fadelength
is a documented simplification, not modeled separately.

## Known simplifications

- **Fadelength isn't modeled separately from Splice Length** - see above.
- **No MIDI Damper Pedal BPM tap-in** - same reasoning as mod
  factory|one.
- **No HS322/HS395 expansion-board delay times** - same reasoning as mod
  factory|one; delays use the standard H3000's own maximum (rounded to
  650ms per this algorithm's own stated figure, vs. mod factory|one's
  700ms).
- **Mod Knob has no special smoothing behavior** - same reasoning as mod
  factory|one's own two Mod Knobs.

## Status

Verified via `ModFactoryTwo` Block smoke tests (the shared `MultiWaveLFO`
and `EnvelopeDucker` primitives were already verified in
`docs/eventide-mod-factory-one.md`):
1. The default chorus patch (Left Input through both Detuners at +/-10
   cents, mixed back together) produces finite output with real energy
   on *both* channels - this caught a genuine bug during testing: the
   Block's own default Mixer 3/4 gains were zeroed as expected, but
   Mixer 2's gains were accidentally zeroed too even though the default
   chorus patch routes through it, silently muting the entire right
   channel. Fixed by giving Mixer 1 and 2 both a full default A+B gain
   (matching the two mixers the default patch actually uses), leaving
   Mixer 3/4 at zero as originally intended.
2. A Detuner alone, patched directly to the output at +100 cents on a
   300Hz input, measures ~317Hz - a semitone up, within 5%.
3. Delay Highcut produces a measurably darker (lower sample-to-sample
   difference sum, i.e. less high-frequency content) impulse response at
   200Hz than at 20000Hz for the same delay tap.
4. Amplitude Modulator behavior matches the manual's own spec (shared
   module with mod factory|one, re-verified here as a regression check).

`dsp_host_render mod_factory_two` renders a 300Hz tone through the
default chorus patch with finite output and a printed RMS curve.
`patches/eventide/mod_factory_two/` (the manual's own suggested chorus
recipe wired by default; 3-knob mapping: Left = Detune amount
(symmetric), Mid = Splice Length, Right = Mix; footswitch press =
bypass, hold = toggle between a subtle +/-10 cent and a wide +/-40 cent
detune preset) and the `EventideModFactoryTwoPlugin` JUCE target both
build clean. The JUCE plugin exposes every module parameter plus all 28
patch destinations as source dropdowns, verified by launching the actual
Standalone build headlessly (Xvfb) and confirming both the parameter
list and the architecture diagram render correctly. As with the other
patches in this archive, the ARM cross-toolchain isn't available in this
sandbox, so the Patch adapter is verified via `make -n` plus a
host-compiler build under `-fno-exceptions -fno-rtti`, not an actual
`.endl` build.

This completes every algorithm in this archive's Eventide H3000
roadmap (100-123, with 121 resolved as not a distinct algorithm - see
`docs/eventide-studio-sampler.md`).
