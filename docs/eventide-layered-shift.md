# Eventide H3000-style Layered Shift

Stage 1 (functional): the second Eventide H3000 algorithm in this archive,
Algorithm 101 per the Instruction Manual's own numbering (right after
Diatonic Shift, Algorithm 100 - see `docs/eventide-h3000-notes.md` and
`docs/eventide-diatonic-shift.md`). Per `CLAUDE.md`'s Primitive →
Component → Block → Graph layering:

- **Primitives**: `dsp/include/dsp/DelayLine.h`, `dsp/include/dsp/PitchShifter.h`
  (both already built for Diatonic Shift - no new primitives needed here).
- **Component**: `dsp/include/dsp/PitchShiftVoice.h` (new) - a Delay
  feeding a PitchShifter, one input in, one shifted output out. Extracted
  from the pattern already proven inside `DiatonicShift.h`'s Left/Right
  Voice handling, but parameterized for a *direct* semitone/cents shift
  rather than a pitch-tracked/diatonic one, matching this algorithm's own
  "Coarse, Fine" cents parameter. Reusable across the H3000's other
  fixed-interval pitch-shift algorithms (Dual Shift, Stereo Shift,
  Multi-Shift).
- **Block**: `dsp/include/dsp/algorithms/LayeredShift.h` - two
  `PitchShiftVoice`s, both fed from the same single input, each with its
  own Delay/cents/Feedback, feedback summing back into that shared input.
- **Graph**: `dsp/include/dsp/graphs/LayeredShiftAlgorithm.h` - the Block
  plus input trim on the one channel it reads (no Right In level, no
  generic width control - see "Topology notes" below).

## Why this algorithm, second

Diatonic Shift (Algorithm 100) is the H3000's own factory default and
signature effect - the natural first pick. Algorithm 101, Layered Shift,
is the very next algorithm in the Instruction Manual's own numbering and
table of contents (`docs/references` excerpt, "The Algorithms" section,
p.44: 100 Diatonic Shift, 101 Layered Shift, 102 Dual Shift, ...) - so
it's the natural next pick for working through the full H3000 algorithm
set in order, per the archive's roadmap of covering every algorithm 100
through 123 (see `docs/eventide-h3000-notes.md`'s Open Item).

## Topology notes

Per the manual's own Description: "This algorithm uses **the left input**
to create two separate pitch shifted outputs." This is a genuinely
different input topology from Diatonic Shift, which sums Left+Right to
mono. Layered Shift's block diagram shows small trim icons ahead of a
summing node, and both a Left Input and Right Input arrow into it, but
the Description text is unambiguous about which channel is the actual
source - so this Block reads only the `left` argument to `processSample()`
and never touches `right` as an input (it's still written as an output).
This matches Algorithm 104 (Reverse Shift)'s wording too ("one-channel-in,
two-channels-out"), while Algorithm 102 (Dual Shift) and 103 (Stereo
Shift) are explicitly stereo - each H3000 pitch-shift algorithm's input
wiring is taken from its own Description text rather than assumed
uniform across the family.

Both Voices' Feedback returns into that same shared single-channel input
(matching the manual's own note: "both right and left channel feedback
are returned to the same input point... high settings on both feedback
levels could cause unstable output conditions" - the same caution
Diatonic Shift's manual page gives, hence the `<0.99` clamp already used
there, reused here).

## Known simplifications

- **No Sustain (freeze) control.** The manual's Sustain parameter loops
  one pitch period of the input endlessly, sampler-style. This needs
  pitch-period-synced buffer looping (implicitly a small pitch detector
  just to find the loop point), which doesn't exist in this Block. Not
  implemented; Feedback near 1 gives a cascading-but-not-frozen
  alternative in the meantime (matches Diatonic Shift's Patch adapter's
  "freeze via feedback" pattern).
- **No Low Note/High Note/Source pitch-shifter-optimization controls.**
  The manual's Expert Mode parameters tune the delay-line pitch shifter's
  internal delay length and deglitching behavior for the expected input
  register and mono/poly source type. This engine's `PitchShifter` is a
  fixed-grain-length design (see `docs/eventide-h3000-notes.md`) with no
  equivalent adaptive delay sizing, so these controls have nothing to
  attach to. Grain length is still exposed directly (`Grain`, matching
  the underlying primitive's own tunable) as the closest analog.
- **Right In is not summed or otherwise used** (see "Topology notes"
  above) - a direct primary-source finding, not a simplification, but
  worth calling out since it differs from Diatonic Shift's mono-sum.

## Status

Verified via:
1. `PitchShiftVoice` in isolation: a 220Hz input shifted +12 semitones
   estimates ~460Hz via zero-crossing counting (some grain-boundary
   estimation noise expected from this crude method).
2. The full `LayeredShiftAlgorithm` Graph: a 220Hz input with Left Voice
   at +400 cents and Right Voice at +700 cents produces outputs
   estimated at ~278Hz and ~334Hz respectively - a major 3rd (expected
   ~277Hz) and a perfect 5th (expected ~330Hz) above the input, matching
   the fixed-interval (non-diatonic) shift this algorithm's manual page
   documents.
3. `dsp_host_render layered_shift` renders a 220Hz tone burst end to end
   with finite output and a sensible RMS build/decay curve.

`patches/eventide/layered_shift/` (3-knob mapping: Left = Left Voice
cents -1200..+1200, with Right Voice trailing a fixed minor 3rd (+300
cents) above it; Mid = shared Feedback; Right = shared Mix - the JUCE
plugin exposes independent Left/Right Voice cents, Delay, Feedback, and
Mix) and the `EventideLayeredShiftPlugin` JUCE target both build clean,
verified by launching the actual Standalone build headlessly (Xvfb) and
confirming both the parameter list and the architecture diagram render
correctly. As with the other patches in this archive, the ARM
cross-toolchain isn't available in this sandbox, so the Patch adapter is
verified via `make -n` plus a host-compiler build under
`-fno-exceptions -fno-rtti`, not an actual `.endl` build.
