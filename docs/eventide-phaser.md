# Eventide H3000-style Phaser

Stage 1 (functional): the twentieth Eventide H3000 algorithm in this
archive, Algorithm 119 per the Instruction Manual's own numbering (right
after String Modeller, Algorithm 118 - see `docs/eventide-h3000-notes.md`
and `docs/eventide-string-modeller.md`). Per `CLAUDE.md`'s Primitive →
Component → Block → Graph layering:

- **New Primitive**: `dsp/include/dsp/AllpassFilter.h` - a first-order
  allpass *filter* (unity gain everywhere, a continuously-settable
  corner frequency where phase crosses -90 degrees). Not the same thing
  as `dsp::Allpass` (the fixed-delay Schroeder allpass used for reverb
  diffusion elsewhere in this archive) despite the shared name - this is
  the first algorithm to need the other kind of "allpass."
- **Block**: `dsp/include/dsp/algorithms/Phaser.h` - twelve
  `AllpassFilter` stages in series, all swept to the same corner
  frequency by one of three sources (LFO, envelope follower, or ADSR),
  mixed back with the dry signal asymmetrically per channel.
- **Graph**: `dsp/include/dsp/graphs/PhaserAlgorithm.h` - the Block plus
  independent Left/Right input trim (Right matters even though it isn't
  phase-shifted, since it can serve as an envelope-follower sidechain).

## Why this algorithm, twentieth

Straight numeric order. Per the manual: "a mono-in, stereo-out phase
shifter, similar in theory to a guitarist's foot-pedal phaser. The dry
signal is mixed with the phase-shifted signal (created by a series of
all-pass filters) to produce a series of notches, whose frequencies can
be swept by altering the filter characteristics." This is the first
algorithm in the archive needing a genuinely different "allpass" than the
one already in `dsp/` - see the new-Primitive note above.

## The output is asymmetric between channels - on purpose

The manual's own Block Diagram shows `(1-mix)` feeding only the left
output's summing junction, with `mix` feeding *both* outputs from the
same twelve-stage chain output. This Block implements that literally:
`left = dry*(1-mix) + wet*mix`, `right = wet*mix` - no dry term on the
right at all. That's the mechanism behind the classic phaser's stereo
motion from a mono source: the notches (dry+wet destructive interference)
only exist on the left; the right is the raw phase-shifted signal alone,
so the two channels diverge as the sweep moves.

## Feedback wraps the whole twelve-stage chain

The manual states plainly what feedback *does* - "With 100% feedback, no
more dry signal is admitted into the phase-shifter loop, and the loop
will resonate" - but its Block Diagram doesn't unambiguously pin down
*where* along the twelve stages the feedback tap sits. This Block takes
the simplest, most defensible reading consistent with that description: a
single feedback path around the entire chain (`chainInput = dry +
feedback*previousChainOutput`, using the same one-sample-latency
technique this archive already uses everywhere a feedback path would
otherwise be a cyclic dependency), rather than guessing at an
intermediate tap point the manual doesn't actually specify.

## All three sweep modes work without MIDI

Sweep Mode (#5) picks between an LFO, an envelope follower, or an ADSR.
The ADSR's own description initially reads MIDI-dependent - "If a MIDI
trigger is received in the attack phase, it will just continue to
attack" - but the same paragraphs describe a second, audio-driven path
that needs no MIDI at all: the ADSR auto-enters its attack segment
whenever the envelope follower rises above Attack Threshold (#12), and
auto-enters release when it falls below Release Threshold (#13) during
sustain. Since no consumer in this project implements MIDI input, this
Block relies on that audio-driven path for real ADSR cycling, and adds a
`trigger()` method as the manual substitute for the MIDI-only ADSR
Trigger (#14) - the same "manual gesture stands in for MIDI" pattern
already used for Stutter and String Modeller's triggers.

## Envelope Channel: a real (non-phase-shifted) sidechain

Envelope Channel (#15) picks whether the envelope follower tracks the
signal actually being phase-shifted (the left input) or "a different
signal on the other channel" - on real H3000 hardware, a genuinely
separate stereo input path. This project's mono-in convention doesn't
preclude that: the Right input is still a real audio input even though
it's never part of the phase-shifted signal path, so `setEnvelopeChannel
(true)` makes the envelope follower track Right as a pure, unshifted
sidechain - letting one signal's dynamics drive the phaser sweep on a
completely different signal, exactly as described.

## Known simplifications

- **No MIDI-driven ADSR Trigger** - `trigger()` is the manual substitute.
- **Feedback taps the whole 12-stage chain**, not an intermediate stage -
  see above; the manual's own diagram doesn't specify one unambiguously.
- **Sweep Bottom/Top are log-mapped 0-100% -> 20Hz-15000Hz** - the manual
  states these are percentages representing frequency extremes but
  doesn't give the underlying Hz range or curve.

## Status

Verified via `AllpassFilter` primitive and `Phaser` Block smoke tests:
1. `AllpassFilter` is a genuine allpass: steady-state RMS stays within
   1.5% of the theoretical sine RMS across six widely-spaced test
   frequencies, for a fixed cutoff - confirming unity gain everywhere.
2. Summing dry with one `AllpassFilter` stage produces real destructive
   interference (a notch) near the cutoff frequency (RMS ~0.50, matching
   the theoretical -90-degree-phase-shift case) versus far from it
   (RMS ~0.70, near-unaffected) - the actual mechanism a phaser depends on.
3. `Phaser` Block: finite output across a full second of a swept tone.
4. At Mix=0%, Right output is silent while Left still carries the full
   dry signal - confirming the manual's asymmetric no-dry-on-right
   Block Diagram is implemented as described.
5. Sweeping the LFO across a fixed tone produces real RMS variation
   across the render (max/min ratio > 1.3x) - the notches are actually
   moving, not static.
6. `trigger()` (with the envelope-follower auto-trigger threshold pinned
   out of reach) measurably changes the ADSR-mode output versus the same
   render without a trigger - confirming the manual gesture is genuinely
   additive, not a no-op.

`dsp_host_render phaser` renders one second of an 800Hz tone through the
LFO-swept phaser with finite output and a printed RMS curve.
`patches/eventide/phaser/` (3-knob mapping: Left = Mix, Mid = Sweep Rate,
Right = Feedback; footswitch press = bypass, hold = toggle the sweep
source between LFO and Envelope Follower) and the `EventidePhaserPlugin`
JUCE target both build clean, verified by launching the actual Standalone
build headlessly (Xvfb) and confirming both the parameter list (Mix/
Feedback/Sweep Rate/Envelope Decay Rate/ADSR Rate Scaler/Sweep Mode/
Sweep Bottom/Sweep Top plus the full ADSR expert set, Envelope Channel,
Envelope Decay Shape, and the ADSR Trigger checkbox) and the architecture
diagram render correctly. As with the other patches in this archive, the
ARM cross-toolchain isn't available in this sandbox, so the Patch adapter
is verified via `make -n` plus a host-compiler build under
`-fno-exceptions -fno-rtti`, not an actual `.endl` build.
