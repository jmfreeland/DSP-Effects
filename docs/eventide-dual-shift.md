# Eventide H3000-style Dual Shift

Stage 1 (functional): the third Eventide H3000 algorithm in this archive,
Algorithm 102 per the Instruction Manual's own numbering (right after
Layered Shift, Algorithm 101 - see `docs/eventide-h3000-notes.md` and
`docs/eventide-layered-shift.md`). Per `CLAUDE.md`'s Primitive →
Component → Block → Graph layering:

- **Primitives/Component**: reuses `dsp/include/dsp/PitchShiftVoice.h`
  unchanged (built for Layered Shift) - no new primitives or Components.
- **Block**: `dsp/include/dsp/algorithms/DualShift.h` - two
  `PitchShiftVoice`s, each fully independent: its own input, own
  Delay/cents/Feedback/Mix, no cross-channel interaction at all.
- **Graph**: `dsp/include/dsp/graphs/DualShiftAlgorithm.h` - the Block
  plus independent Left/Right input trim.

## Why this algorithm, third

Straight numeric order per the Instruction Manual's own Table of
Contents (`docs/eventide-h3000-notes.md`'s Status section has the full
100-123 list) - Algorithm 102 is the next one after Layered Shift. It
also happened to be the cheapest possible next build: `PitchShiftVoice`
(the Component built for Layered Shift) needed zero changes, since Dual
Shift is *architecturally simpler* than Layered Shift - no shared input,
no shared feedback point, just two copies of the same per-channel unit
wired side by side.

## Topology notes

Per the manual: "Algorithm 102 gives you two completely separate pitch
shifters. One pitch shifter uses the left channel input and output while
the other uses the right channel." This is the most literal possible
reading of "stereo pitch shifter" - unlike Layered Shift (single input,
shared feedback point) or Stereo Shift (Algorithm 103, true stereo but
*shared* parameters), Dual Shift's two channels share nothing: not the
input, not the feedback path, not even necessarily the same shift amount
or delay time. Confirmed independence is the entire point of this
algorithm (per the manual's own "completely separate"), so it's tested
explicitly: feeding only the Left channel produces an unchanged Left
output regardless of what the Right channel is doing (see Status below).

Delay range here is 0-500ms, half of Layered Shift's 0-1000ms - taken
directly from the manual's own parameter table, not assumed to match the
other algorithm.

## Known simplifications

Same set as Layered Shift, for the same reasons (see
`docs/eventide-layered-shift.md`'s "Known simplifications" - this
algorithm shares the same Expert Mode parameter list: Sustain, Low
Note/High Note, Source):

- **No Sustain (freeze) control.**
- **No Low Note/High Note/Source pitch-shifter-optimization controls** -
  this engine's fixed-grain-length `PitchShifter` has no equivalent
  adaptive delay sizing to attach these to.

## Status

Verified via:
1. The full `DualShiftAlgorithm` Graph: independent 220Hz (Left) and
   330Hz (Right) inputs, with Left set to -1200 cents and Right to +1200
   cents, produce outputs estimated (zero-crossing counting) at ~106Hz
   and ~672Hz - an octave down from 220Hz (expected 110Hz) and an octave
   up from 330Hz (expected 660Hz), within the crude estimator's noise
   margin.
2. Channel independence, the algorithm's own defining property: silencing
   the Right input entirely leaves the Left output's estimated frequency
   unchanged (both runs measured ~106Hz).
3. `dsp_host_render dual_shift` renders both channels end to end with
   finite output and a sensible RMS build/decay curve.

`patches/eventide/dual_shift/` (3-knob mapping: Left = Left channel
cents, Mid = Right channel cents - independent, since the two channels
never interact on real hardware either; Right = shared Mix, Feedback
fixed off except via footswitch-hold freeze, since the hardware only has
3 knobs - the JUCE plugin exposes independent Left/Right Delay and
Feedback) and the `EventideDualShiftPlugin` JUCE target both build
clean, verified by launching the actual Standalone build headlessly
(Xvfb) and confirming both the parameter list and the architecture
diagram render correctly. As with the other patches in this archive, the
ARM cross-toolchain isn't available in this sandbox, so the Patch adapter
is verified via `make -n` plus a host-compiler build under
`-fno-exceptions -fno-rtti`, not an actual `.endl` build.
