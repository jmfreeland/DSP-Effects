# Eventide H3000-style Stereo Shift

Stage 1 (functional): the fourth Eventide H3000 algorithm in this
archive, Algorithm 103 per the Instruction Manual's own numbering (right
after Dual Shift, Algorithm 102 - see `docs/eventide-h3000-notes.md` and
`docs/eventide-dual-shift.md`). Per `CLAUDE.md`'s Primitive → Component →
Block → Graph layering:

- **Primitives/Component**: reuses `dsp/include/dsp/PitchShiftVoice.h`
  unchanged - no new primitives or Components, same as Dual Shift.
- **Block**: `dsp/include/dsp/algorithms/StereoShift.h` - two
  `PitchShiftVoice`s, each with its own input/output/feedback (like Dual
  Shift), but driven by one shared Coarse/Fine/Delay/Feedback/Mix value
  instead of two independent sets.
- **Graph**: `dsp/include/dsp/graphs/StereoShiftAlgorithm.h` - the Block
  plus independent Left/Right input trim.

## Why this algorithm, fourth

Straight numeric order again (100 → 101 → 102 → 103). Architecturally,
Stereo Shift sits *between* Layered Shift (shared input, shared feedback
point) and Dual Shift (fully independent channels, independent
parameters): it has Dual Shift's independent per-channel signal paths
but Layered Shift's "one set of controls" simplicity. This made it the
cheapest of the three to build once Dual Shift existed - same Block
shape, fewer setters.

## Topology notes

Per the manual: "The Stereo Pitch Shift algorithm is for operation with
true stereo inputs. The unique deglitching takes both input channels
into account without mixing the two audio signals... Parameters of both
channels adjust together." The key distinction from Dual Shift is
exactly that last sentence - "adjust together" - which this Block
implements as one shared value applied to both `PitchShiftVoice`s'
setters, while each channel's Feedback still returns only into its own
input (matching the manual's block diagram, which draws two separate
feedback triangles, not a cross-channel or summed one).

## Known simplifications

Same set as Layered Shift/Dual Shift (Sustain, Low Note/High Note,
Source - see `docs/eventide-layered-shift.md`), plus one specific to
this algorithm:

- **No Deglitch Mode control.** The manual's Expert Mode adds a
  Stereo-Shift-specific parameter, "Deglitch Mode: 'Lock to chan. 1' or
  'chan. 1 and chan. 2'," tuning whether the two channels' pitch-shifter
  grain timing is locked together or independent. This engine's
  `PitchShifter` has no concept of cross-channel grain-timing
  coordination to attach this to (each `PitchShiftVoice` already runs
  its own independent grain phase, closer to "chan. 1 and chan. 2" than
  "Lock to chan. 1" by default, though the manual doesn't say which is
  more correct for typical use).

## Status

Verified via:
1. The full `StereoShiftAlgorithm` Graph: an identical 220Hz signal on
   both channels with a shared +700 cent shift produces both Left and
   Right outputs estimated (zero-crossing counting) at ~334Hz - a
   perfect 5th above 220Hz (expected ~330Hz), and the two channels'
   estimated frequencies match each other to within 2Hz, confirming the
   "one shared value drives both channels" behavior the manual describes.
2. `dsp_host_render stereo_shift` renders both channels end to end with
   finite output and a sensible RMS build/decay curve.

`patches/eventide/stereo_shift/` (3-knob mapping: Left = shared shift
-1200..+1200 cents, Mid = shared Feedback, Right = shared Mix - Delay
fixed at a reasonable default since the hardware only has 3 knobs, the
JUCE plugin exposes it directly) and the `EventideStereoShiftPlugin`
JUCE target both build clean, verified by launching the actual
Standalone build headlessly (Xvfb) and confirming both the parameter
list and the architecture diagram render correctly. As with the other
patches in this archive, the ARM cross-toolchain isn't available in this
sandbox, so the Patch adapter is verified via `make -n` plus a
host-compiler build under `-fno-exceptions -fno-rtti`, not an actual
`.endl` build.
