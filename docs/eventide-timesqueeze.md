# Eventide H3000-style Timesqueeze

Stage 1 (functional): the fourteenth Eventide H3000 algorithm in this
archive, Algorithm 113 per the Instruction Manual's own numbering (right
after Stutter, Algorithm 112 - see `docs/eventide-h3000-notes.md` and
`docs/eventide-stutter.md`). Per `CLAUDE.md`'s Primitive → Component →
Block → Graph layering:

- **Primitive reused**: `PitchShifter` (unchanged) - no new primitives.
- **Block**: `dsp/include/dsp/algorithms/Timesqueeze.h` - two
  independent `PitchShifter`s (Left/Right) sharing one computed shift
  ratio derived from Time% and an independent Pitch trim.
- **Graph**: `dsp/include/dsp/graphs/TimesqueezeAlgorithm.h` - the Block
  plus independent Left/Right input trim, no Mix control.

## Why this algorithm, fourteenth, and why it's not a skip

Straight numeric order. Unlike every other H3000 algorithm's manual page
built so far, Timesqueeze's own page has **no Block Diagram** and is
framed almost entirely around a physical, variable-speed tape machine:
"this algorithm will automatically control the tape machine playback
speed and make the necessary pitch correction." The bulk of its
parameter set (`select`/`custom`/`reference` under "Tape Machine
Interfacing Parameters") exists purely to configure a control-voltage
output driving a real analog deck - hardware this software has no
equivalent of, on the Endless or in a VST host, so that half is
correctly out of scope.

But the manual's own #0 Time and #1 Pitch parameters are genuinely
audio-domain, and the H3000 computes and applies their pitch correction
*internally* regardless of whether a deck is attached: "Changing this
parameter will automatically set the tape machine speed **and the
correct amount of pitch shift**." That's exactly the H3000's own
internal `PitchShifter` doing real-time work, and it's fully
reproducible: Time% is the length change a tape machine would be told to
make, which implies a playback speed ratio (`speedRatio = 1 +
time/100`); a tape sped up would otherwise raise the pitch of whatever's
recorded on it, so the H3000 shifts by the *inverse* of that ratio to
keep the pitch the listener hears constant, then applies the
independent Pitch trim ratio (#1) on top. So rather than a blanket skip,
this Block builds the pitch-correction half faithfully and skips only
the tape-transport-control half, which has no meaningful software
equivalent.

## The math, and why it self-validates

`speedRatio = 1 + time/100`, `compensatingRatio = 1/speedRatio`,
`totalRatio = compensatingRatio * pitchTrim`, converted to semitones via
`12 * log2(totalRatio)`. Two sanity checks fell out of this for free:
Time=100% (a tape doubled in speed) yields `speedRatio=2.0`, so the
compensating shift is exactly `0.5` - down one octave, matching the
manual's own Pitch parameter description elsewhere on the same page ("A
ratio of .5 corresponds to an octave shift down"). And Time=-87.5% (the
parameter's own minimum) yields `speedRatio=0.125`, an inverse of `8.0` -
three octaves up, matching the H3000's own hardware-spec pitch range
("3 octaves up, 3 octaves down") noted in `docs/eventide-h3000-notes.md`.
Neither of these was designed in on purpose; both fell out of just
implementing the stated formula, which is a reasonable sign the formula
is right.

## No Mix control - a real absence, not an oversight

This algorithm's Levels section lists Left In/Right In/Left Out/Right
Out but, uniquely among the H3000 algorithms built in this archive so
far, no Mix parameter of any kind. Combined with the complete absence of
a Block Diagram, this reads as a correction utility meant to be always
fully applied (there's no sensible "50% pitch-corrected" state for a
tool whose whole purpose is keeping pitch anchored to a reference), so
this Block deliberately doesn't invent one.

## Known simplifications

- **No tape machine control-voltage output** (`select`/`custom`/
  `reference` under "Tape Machine Interfacing Parameters") - no
  equivalent hardware exists on this software's targets; see above.
- **No Deglitch parameters** (min delay/delayrng/min freq/silence-gap
  detection) - same simplification already made for Diatonic Shift and
  Patch Factory's own Deglitch sections; this archive's `PitchShifter`
  doesn't expose an equivalent tunable.

## Status

Verified via `Timesqueeze` Block smoke tests, checking actual frequency
ratios (via zero-crossing counting on a steady tone, the same technique
used for the Shift-family algorithms) rather than just "doesn't crash":
1. Time=0%, Pitch=1.0 (defaults): a 220Hz input measures ~220Hz at the
   output (no shift).
2. Time=100%: a 220Hz input measures ~120Hz at the output (down close to
   an octave, allowing for the `PitchShifter`'s own grain-crossfade
   estimation noise - expected ~110Hz).
3. Time=-50%: a 220Hz input measures ~460Hz (up close to an octave,
   expected ~440Hz).
4. Time=0% with Pitch trim=2.0 (independent of Time): also measures
   ~460Hz, confirming the trim stacks correctly on top of a
   Time-derived shift of zero.

`dsp_host_render timesqueeze` renders a burst end to end with finite
output. `patches/eventide/timesqueeze/` (2 real knobs - Left = Time,
Mid = Pitch trim; Right is unused since no third parameter exists;
footswitch press = bypass, hold = reset Pitch trim to 1.0) and the
`EventideTimesqueezePlugin` JUCE target both build clean, verified by
launching the actual Standalone build headlessly (Xvfb) and confirming
both the parameter list and the architecture diagram render correctly.
As with the other patches in this archive, the ARM cross-toolchain isn't
available in this sandbox, so the Patch adapter is verified via
`make -n` plus a host-compiler build under `-fno-exceptions -fno-rtti`,
not an actual `.endl` build.
