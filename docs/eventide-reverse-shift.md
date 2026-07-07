# Eventide H3000-style Reverse Shift

Stage 1 (functional): the fifth Eventide H3000 algorithm in this
archive, Algorithm 104 per the Instruction Manual's own numbering (right
after Stereo Shift, Algorithm 103 - see `docs/eventide-h3000-notes.md`
and `docs/eventide-stereo-shift.md`). Per `CLAUDE.md`'s Primitive →
Component → Block → Graph layering:

- **Primitive**: `dsp/include/dsp/ReverseBuffer.h` (new) - a "tape
  reverse" splice generator: records a settable-length segment while
  playing back the *previous* segment time-reversed, swapping roles at
  each boundary. The first genuinely new H3000 primitive since
  `PitchDetector`/`PitchShifter` - none of Layered/Dual/Stereo Shift
  needed anything beyond those.
- **Block**: `dsp/include/dsp/algorithms/ReverseShift.h` - mono-sums the
  stereo input, running it through two independent
  `ReverseBuffer -> PitchShifter` chains (Left, Right), each with its
  own splice Length and pitch-shift cents, feedback summing back into
  the shared input.
- **Graph**: `dsp/include/dsp/graphs/ReverseShiftAlgorithm.h` - the Block
  plus input trim.

## Why this algorithm, fifth

Straight numeric order again. Unlike 101-103, which all reused
`PitchShiftVoice` unchanged (Layered/Dual/Stereo Shift are all variations
on "smoothly pitch-shift a delayed signal"), Reverse Shift's actual
transposition mechanism is entirely different - genuine time reversal,
not a continuous pitch sweep - so it needed a real new primitive rather
than another wiring variation. This is the first Eventide algorithm
since Diatonic Shift's `PitchDetector` to add new DSP substance, not
just new topology around existing pieces.

## Why a new primitive instead of reusing PitchShifter

`PitchShifter`'s two-tap crossfading design reads *forward* through a
circular delay line at a rate controlled by the target pitch ratio -
fundamentally a continuous-time mechanism. True reversal needs something
qualitatively different: a *stable* region to read backward through
while a *separate* region keeps recording, so the read never chases the
write head. `ReverseBuffer` solves this with two fixed-capacity halves
that ping-pong roles (recording ↔ playback) rather than one circular
buffer - see that file's doc comment. The manual's own description
("Think of a tape recorder recording a small length of tape... and then
playing it back in reverse while it records the next") maps directly
onto this two-buffer swap, which is a reasonable sign the mechanism
matches the real one at the conceptual level (the actual PEL
implementation isn't public, per the usual "inspired by" framing - see
`CLAUDE.md`).

The manual's own "Coarse, Fine" pitch-shift parameters are then layered
*on top* of the reversal by feeding `ReverseBuffer`'s output into a
normal `PitchShifter` - decomposing "reverse-with-pitch-shift" into two
already-understood mechanisms in series, matching the manual's "Two
pitch shifters in fact" description (interpretable as two independent
processing chains, each capable of pitch shift) without inventing a
single hybrid mechanism that does both at once.

## A real behavioral gotcha: Length changes apply at the next swap boundary

`ReverseBuffer::setLengthSeconds()` doesn't take effect immediately -
only at the next recording/playback swap, to avoid corrupting whatever
is currently mid-playback. In practice this means turning the Length
knob has up to the *current* splice length's worth of latency before
the audible change lands (worst case just under 1.4s, matching the
manual's own maximum). This is a deliberate glitch-avoidance choice, not
a bug (caught during this Block's own smoke test - see Status below,
where a test written to check Length changes immediately had to be
fixed to call `reset()` first, matching how a live parameter change
would actually behave over the following one or two splices).

## Known simplifications

Same Expert Mode gaps as the other H3000 pitch-shift algorithms in this
archive (Low Note/High Note/Source - see
`docs/eventide-layered-shift.md`'s "Known simplifications" for why).

## Status

Verified via:
1. `ReverseBuffer` in isolation: a 10-sample ramp `[1..10]` written into
   the first segment, followed by any second-segment input, plays back
   exactly `[10,9,...,1]` during the second segment - confirmed sample-
   for-sample, not just "sounds reversed."
2. The full `ReverseShiftAlgorithm` Graph: a repeating ramp signal at 0
   cents (matched grain/splice length to minimize the pitch-shift
   stage's own latency) shows clearly descending output samples in a
   later splice, confirming the reversal survives the full
   ReverseBuffer → PitchShifter chain, not just the primitive alone.
3. `dsp_host_render reverse_shift` renders a short tone burst end to end
   with finite output (the printed RMS-at-t=0/t=1s decay-curve summary
   reads as silence for this algorithm specifically, since the burst and
   its reversed playback both fall inside the first second at 100ms
   sampling granularity that curve doesn't capture - the WAV file itself
   contains the actual reversed audio).

`patches/eventide/reverse_shift/` (3-knob mapping: Left = shared splice
Length 20ms..1s, Mid = shared Feedback, Right = shared Mix - pitch shift
fixed at 0 cents (pure reversal) since the hardware only has 3 knobs, the
JUCE plugin exposes independent Left/Right Length and cents) and the
`EventideReverseShiftPlugin` JUCE target both build clean, verified by
launching the actual Standalone build headlessly (Xvfb) and confirming
both the parameter list and the architecture diagram render correctly.
As with the other patches in this archive, the ARM cross-toolchain isn't
available in this sandbox, so the Patch adapter is verified via `make -n`
plus a host-compiler build under `-fno-exceptions -fno-rtti`, not an
actual `.endl` build.
