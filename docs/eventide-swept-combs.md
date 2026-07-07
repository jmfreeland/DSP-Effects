# Eventide H3000-style Swept Combs

Stage 1 (functional): the sixth Eventide H3000 algorithm in this
archive, Algorithm 105 per the Instruction Manual's own numbering (right
after Reverse Shift, Algorithm 104 - see `docs/eventide-h3000-notes.md`
and `docs/eventide-reverse-shift.md`), and the first of a new family
(105-107 share one "six swept delay lines" shape - see "Why this
algorithm, sixth" below). Per `CLAUDE.md`'s Primitive → Component →
Block → Graph layering:

- **Primitive addition**: `LFO::nextRandomWalk()` (new method on the
  existing `dsp/include/dsp/LFO.h`) - a smoothly-wandering random
  modulation source, picking a new random target once per cycle and
  linearly sliding toward it, rather than a sine/triangle sweep. Directly
  grounded in the manual's own wording: "Is it a sine, ramp or triangle
  sweep? Actually, it's not any of those... The algorithm uses random
  numbers to achieve a more complex and thicker texture."
- **Component**: `dsp/include/dsp/SweptCombVoice.h` (new) - a `Comb`
  (settable-length feedback delay) whose length wanders continuously
  around a base value via `nextRandomWalk()`, plus level and pan - the
  "one digital delay unit" the manual describes ×6.
- **Block**: `dsp/include/dsp/algorithms/SweptCombs.h` - six independent
  `SweptCombVoice`s, each with its own base Delay/Rate/Depth/Feedback/
  Pan/Level (the manual's "Tedium" per-line parameters), summed into a
  stereo mix. Five Master ("Quickset") controls proportionally scale all
  six lines' values without altering the underlying per-line settings.
- **Graph**: `dsp/include/dsp/graphs/SweptCombsAlgorithm.h` - the Block
  plus input trim.

## Why this algorithm, sixth

Straight numeric order again. Algorithms 105 (Swept Combs), 106 (Swept
Reverb), and 107 (Reverb Factory) all share the "six delay lines, each
with its own feedback and sweep generator, patched to a stereo mixer or
reverb network" block diagram per the manual - Swept Combs is the
simplest of the three (delays feed the mixer directly, no shared reverb
network afterward), making it the natural first build of that shared
shape, with `SweptCombVoice` positioned to be reused by 106 and evaluated
for reuse by 107.

## Design choices not fully specified by the manual

The manual documents every parameter's *existence* and *range* in detail
but not the exact mapping from a Master control's 0-100% to a physical
unit, nor factory default values for this algorithm specifically (unlike
Diatonic Shift, where preset catalog entries gave concrete numbers to
check against). Three original (if reasonable) choices made here,
flagged rather than presented as verified facts:

- **Sweep rate range**: 0.05Hz-5Hz (slow wobble to audible flutter),
  mapped linearly from the 0-100 Rate parameter.
- **Sweep depth range**: up to ±30ms of delay-length excursion at 100%
  Depth, scaled by the Depth fraction.
- **Default per-line values**: an original, deliberately-non-identical
  spread across delays (41-211ms), rates (20-95), and pans (hard left to
  hard right) so the algorithm sounds like six *different* delay lines
  immediately, rather than requiring Tedium editing first - the manual
  doesn't specify factory defaults for the bare algorithm (only for named
  presets built from it, none of which were in the factory preset catalog
  already read for this archive - see `docs/eventide-h3000-notes.md`).

## Known simplifications

- **No Glide.** The real hardware's Glide Speed/Enable smooths *master*
  Delay changes to avoid clicks; not implemented here (matches this
  archive's existing pattern of documenting rather than always
  implementing secondary Expert-mode smoothing controls).
- **Repeat is an approximation.** The manual's Repeat "instantly captures
  the audio signal... and keeps replaying it. No new sound is allowed
  in." This engine's `setRepeat(true)` mutes new input into the six
  lines so existing recirculating energy keeps looping at whatever
  Feedback is set, rather than a true infinite-capture/hold state -
  matches the spirit but decays over time unless Feedback is pushed
  close to 1.

## Status

Verified via:
1. `LFO::nextRandomWalk()` in isolation: stays within [-1, 1] and its
   range across 10 retargets spans more than half that range, confirming
   genuine wandering rather than a flat or narrow-band output.
2. `SweptCombVoice` in isolation: an impulse produces finite, nonzero,
   delayed output.
3. The full `SweptCombsAlgorithm` Graph: an impulse produces finite
   output throughout a 2-second render with nonzero total energy
   (confirming the six lines are actually summing into the mix), and
   setting Master Delay to 0% (collapsing all lines toward zero delay)
   still produces finite output.
4. `dsp_host_render swept_combs` renders an impulse response end to end
   with a sensible RMS decay curve.

`patches/eventide/swept_combs/` (3-knob mapping: Left = Master Delay,
Mid = Master Feedback, Right = Mix - Rate/Depth/Width stay at their
built-in defaults since the hardware only has 3 knobs, the JUCE plugin
exposes all five Masters plus the full 36-parameter per-line Tedium set)
and the `EventideSweptCombsPlugin` JUCE target both build clean, verified
by launching the actual Standalone build headlessly (Xvfb) and
confirming both the parameter list (including the Stereo Input/Repeat
checkboxes) and the architecture diagram render correctly. As with the
other patches in this archive, the ARM cross-toolchain isn't available
in this sandbox, so the Patch adapter is verified via `make -n` plus a
host-compiler build under `-fno-exceptions -fno-rtti`, not an actual
`.endl` build.
