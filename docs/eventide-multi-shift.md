# Eventide H3000-style Multi-Shift

Stage 1 (functional): the seventeenth Eventide H3000 algorithm in this
archive, Algorithm 116 per the Instruction Manual's own numbering (right
after Vocoder, Algorithm 115 - see `docs/eventide-h3000-notes.md` and
`docs/eventide-vocoder.md`). Per `CLAUDE.md`'s Primitive → Component →
Block → Graph layering:

- **Primitives/Components reused**: `DelayLine`, `PitchShifter`,
  `ReverseBuffer`, and `rotateStereoWidth` - all already built for
  earlier Shift algorithms and the PCM81 side. No new primitives.
- **Block**: `dsp/include/dsp/algorithms/MultiShift.h` - two independent
  pitch-shift channels (each a Delay→Shift chain, or a Reverse-splice→
  Shift chain in Reverse mode, exactly like Reverse Shift/Algorithm 104)
  plus two independent dry delay taps, four sources total with a
  patchable feedback structure.
- **Graph**: `dsp/include/dsp/graphs/MultiShiftAlgorithm.h` - the Block
  plus independent Left/Right input trim.

## Why this algorithm, seventeenth

Straight numeric order. Per the manual: "similar to the dual shift
program, allowing discrete stereo pitch shifting. In addition to the
pitch shifters, a delay tap has been added to each pitch shift channel,
giving a total of four outputs. Each of the four outputs can be panned
anywhere in the stereo field... a 'patchable' feedback structure has
been set up, allowing each pitch shifter to use any two of the four
outputs as feedback." This is a genuine escalation over Dual Shift
(Algorithm 102, already built): same two-independent-channel shape, but
each channel gains its own dry delay tap and a real feedback-routing
matrix - narrower than Patch Factory's general matrix (only two
feedback slots per channel, only four possible sources, and only the
pitch shifters' own inputs are patchable destinations) but built with
the exact same one-sample-latency technique for the same reason: making
any user-chosen routing (including a channel feeding back into itself)
well-defined without needing to detect cycles.

## Reverse mode reuses Reverse Shift's own mechanism directly

Per the manual: "When this parameter is set to 'reverse', the left
pitch shifter is set to reverse pitch shift mode (exactly like program
104)." This Block takes that literally: each channel holds both a
`DelayLine` (forward path) and a `ReverseBuffer` (reverse path, its
`ReverseBuffer::setLengthSeconds()` driven by the manual's own Splice
parameter), and Direction simply selects which one feeds that channel's
shared `PitchShifter` each sample - not two separate signal chains.

## Two documented interpretation calls

- **Xfade (Slow/Fast)** maps directly onto `PitchShifter::setGrainSeconds()`:
  Fast uses a short grain ("exactly like our old pitch shifters"), Slow
  uses a longer grain ("intended for small pitch shift amounts... allows
  virtually glitchless micro-pitch shifting") - a faithful mapping since
  grain length is exactly what trades transient response for smoothness
  in this archive's `PitchShifter`.
- **Feedback (#7)** is described as "a 'global' scaling control for the
  amount of feedback... controls all four feedback levels at once," but
  its printed range in the manual is "0 to 10.0 milliseconds" - a unit
  that doesn't match a scaling control at all (every other "global
  scale" parameter in this archive's H3000 algorithms, e.g. Swept Combs'
  Master controls, is a percentage). This reads as a copy-paste artifact
  from a template page rather than the intended value, so this Block
  implements it as the *described* behavior - a 0-100% master scale
  multiplying each path's own Feedback 1/2 amount - rather than the
  literally printed millisecond unit.

## Known simplifications

- **No Deglitch on/off** (#27/#31) - the manual describes this as
  toggling between adaptive spectral-analysis tracking and "a fixed
  splice interval"; this archive's `PitchShifter` always uses its own
  fixed-grain crossfade mechanism regardless, so there's no adaptive
  mode to disable. Same simplification already made for the Deglitch
  parameters on Diatonic Shift, Patch Factory, and Vocoder.
- **Image is a width-rotation, not a literal "L<->R to R<->L" sweep** -
  reuses `rotateStereoWidth()` (already an original reconstruction on
  the PCM81 side) rather than a new primitive, since it's the only
  width-manipulation mechanism in this archive and the manual gives no
  further mechanism detail beyond "determines the width of the output
  stereo field."

## Status

Verified via `MultiShift` Block smoke tests, checking actual measured
frequency ratios (zero-crossing counting) rather than just "doesn't
crash":
1. Left channel at +1200 cents and Right channel at 0 cents, given a
   220Hz input, measure ~420Hz and ~220Hz respectively (up an octave on
   Left, unchanged on Right) - confirming the two channels shift fully
   independently.
2. An impulse with Left Feedback 1 routed back into L Pitch's own input
   sustains measurably more tail energy than the same impulse with
   feedback disabled - confirming the patchable feedback structure is
   real, not decorative.
3. Reverse mode runs a ramp input through without producing non-finite
   output.

`dsp_host_render multi_shift` renders a burst end to end with finite
output on both channels. `patches/eventide/multi_shift/` (3-knob
mapping: Left = L Coarse/Fine, Mid = R Coarse/Fine, Right = Mix;
footswitch press = bypass, hold = toggle both channels' Direction
together) and the `EventideMultiShiftPlugin` JUCE target both build
clean, verified by launching the actual Standalone build headlessly
(Xvfb) and confirming both the parameter list (including all four
feedback-source dropdowns and Direction/Xfade checkboxes) and the
architecture diagram render correctly. As with the other patches in
this archive, the ARM cross-toolchain isn't available in this sandbox,
so the Patch adapter is verified via `make -n` plus a host-compiler
build under `-fno-exceptions -fno-rtti`, not an actual `.endl` build.
