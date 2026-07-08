# Eventide H3000-style Band Delay

Stage 1 (functional): the eighteenth Eventide H3000 algorithm in this
archive, Algorithm 117 per the Instruction Manual's own numbering (right
after Multi-Shift, Algorithm 116 - see `docs/eventide-h3000-notes.md`
and `docs/eventide-multi-shift.md`). Per `CLAUDE.md`'s Primitive →
Component → Block → Graph layering:

- **Primitives reused**: `DelayLine`, `StateVariableFilter` (already
  built for Patch Factory and reused again for Vocoder), and
  `rotateStereoWidth` - no new primitives.
- **Block**: `dsp/include/dsp/algorithms/BandDelay.h` - one shared
  multi-tap delay line with 8 independently-settable read taps, each
  feeding its own bandpass filter, output Level, and Pan, plus one
  recirculating feedback loop.
- **Graph**: `dsp/include/dsp/graphs/BandDelayAlgorithm.h` - the Block
  plus independent Left/Right input trim.

## Why this algorithm, eighteenth, and "a multi-tap delay line" taken literally

Straight numeric order. Per the manual: "This algorithm is a multi-tap
delay line, with each of its eight delay outputs connected to a
separate bandpass filter." The singular phrasing - "a... delay line,"
not "eight delay lines" - is taken at face value here: this Block uses
**one shared `DelayLine`** with 8 independently-settable read-tap
positions, rather than 8 separate delay lines each with their own write
head. This is both simpler (one buffer to manage) and more faithful to
the manual's own wording than the alternative reading. One recirculating
feedback loop (its own Feedback Delay + Feedback amount, "in combination
... can be used to set up a digital delay repeat loop" per the manual)
reads the final mixed stereo output back into that same shared line,
alongside the live stereo input.

## Skipping Note Mode and MIDI-driven tuning entirely

This algorithm's own Expert Parameters include a Note Mode (#57:
off/routed/ordered/circular) governing how a MIDI keyboard tunes the
eight filters' center frequencies in real time, plus a per-filter Note
parameter (Cx to G9) as one of three additive tuning sources (Global
Frequency + individual Frequency cents + individual Note, all in
semitone/cents space, converted to Hz - the manual gives a full worked
example: "C4 + 200 cents - 12 semitones ... = D3"). None of this
project's three consumers (the Polyend Endless Patch, the native host
harness, or the JUCE plugin) implement MIDI input for parameter control
at all - every existing `PluginProcessor::acceptsMidi()` in this archive
already returns `false` - so there is no keyboard to route notes from.
Rather than stub out a MIDI pathway nothing else in this codebase has,
this Block replaces the Note parameter with a direct, settable **base
Hz** per filter (`setFilterBaseHz()`) serving the identical purpose (each
filter's fundamental tuning) without note-name semantics, keeping Global
Frequency (semitones, applied to all 8 filters) and per-filter Frequency
(cents) exactly as documented on top of it. Note Mode itself has no
equivalent and isn't exposed.

## Global Q reuses the manual's own stated normal usage

Each filter has its own raw Q setting (0-999, expert-only) plus a Global
Q (0-100%) that "simultaneously scales the Q factor... of all the
filters." The manual's own Hint section states the intended normal
workflow directly: "Normally, these are all set to 999 and the global Q
factor is used to adjust the value of all the Q factors simultaneously" -
confirming a straightforward multiplicative relationship
(`effectiveQ = (rawQ/999) * (globalQ/100)`), which this Block implements
exactly, with both defaulting to their described "normal" values (raw
999, global 100%).

## Known simplifications

- **No MIDI-driven tuning / Note Mode** - see above.
- **Left In and Right In are summed** into the one shared delay line
  (matching "a multi-tap delay line," singular) rather than kept on
  independent per-channel lines - the manual's own Block Diagram draws
  Left In and Right In entering between different tap pairs, but gives
  no further detail distinguishing per-channel behavior beyond that
  drawing.
- **Delay/Feedback Delay capacities rounded to 1.5s** (the manual's own
  figures are 1496.0ms and 1485.0ms respectively) - the same rounding
  convention already used elsewhere in this archive for near-equal
  manual-stated maximums.

## Status

Verified via `BandDelay` Block smoke tests:
1. An impulse routed through a single isolated, wide-open (low-Q) tap
   produces a clear echo at the expected delay time (confirmed by
   comparing energy in a window around the expected tap time against a
   window far away), and Global Delay=50% halves that echo's timing
   relative to Global Delay=100% for the same individual Delay setting -
   confirming the master delay scale genuinely applies to individual
   taps.
2. Engaging Feedback produces measurably more energy in a later time
   window than the same impulse with Feedback off - confirming the
   recirculating loop is real, not decorative.

`dsp_host_render band_delay` renders an 8-tap impulse end to end with
finite output. `patches/eventide/band_delay/` (3-knob mapping: Left =
Global Delay, Mid = Feedback, Right = Mix; footswitch press = bypass,
hold = toggle Global Frequency between 0 and +12 semitones, an "octave
up" preset for all eight filters at once) and the
`EventideBandDelayPlugin` JUCE target both build clean, verified by
launching the actual Standalone build headlessly (Xvfb) and confirming
both the parameter list (all 7 global controls plus the full per-filter
expert set: Base Hz/Cents/Q/Delay/Level/Pan x 8) and the architecture
diagram render correctly. As with the other patches in this archive,
the ARM cross-toolchain isn't available in this sandbox, so the Patch
adapter is verified via `make -n` plus a host-compiler build under
`-fno-exceptions -fno-rtti`, not an actual `.endl` build.
