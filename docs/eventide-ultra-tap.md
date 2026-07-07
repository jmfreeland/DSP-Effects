# Eventide H3000-style Ultra-Tap

Stage 1 (functional): the ninth Eventide H3000 algorithm in this
archive, Algorithm 108 per the Instruction Manual's own numbering (right
after Reverb Factory, Algorithm 107 - see `docs/eventide-h3000-notes.md`
and `docs/eventide-reverb-factory.md`). Per `CLAUDE.md`'s Primitive →
Component → Block → Graph layering:

- **Primitives reused**: `DiffuserChain<4>` (already built for the
  Lexicon PCM81 reverb cores' own Diffusion stage) drives this
  algorithm's 4-stage Allpass diffusor unchanged in structure - only its
  per-stage delay lengths differ, which required a small primitive
  extension (see "A real bug caught mid-build" below).
- **Block**: `dsp/include/dsp/algorithms/UltraTap.h` - the diffusor
  feeding a new 12-tap *cumulative* delay line (each tap's own Delay
  parameter is the time since the *previous* tap, not from the input),
  with independent Level/Pan per tap and one selectable tap feeding
  back.
- **Graph**: `dsp/include/dsp/graphs/UltraTapAlgorithm.h` - the Block
  plus input trim.

## Why this algorithm, ninth

Straight numeric order again, and the direct sequel to the "six delay
lines" family (105-107): Ultra-Tap moves to a different shape (a
diffusor feeding *twelve* discrete taps rather than a small feedback
network) but reuses `DiffuserChain<4>` directly, since diffusion-before-
a-delay-structure is exactly the role that Component already plays for
every PCM81 reverb core in this archive.

## A real bug caught mid-build: `Allpass`'s delay was never actually settable

The manual states each of the diffusor's four Allpass stages has "its
own Delay parameter." While wiring this up, `setAllpassDelayMs()` was
initially written to store a value that was never read anywhere -
`Allpass::process()` always reads at a *fixed* lag (`buffer.size() - 1`,
i.e. whatever capacity `setBuffer()` was given), with no way to change it
afterward. This was silently dead code, not a crash or a wrong number -
the kind of bug a smoke test only catches if it specifically checks that
a setter has an effect. Caught by writing `setAllpassDelayMs()`'s
intended behavior into the design and then verifying against the actual
`Allpass` implementation, not by output failing to compile.

Fixed by extending `Allpass` (and `DiffuserChain<N>`) with an *opt-in*
`setDelaySamples()`/`setStageDelaySamples()`: callers that never call it
keep the original fixed-length behavior exactly (verified by re-running
all 5 PCM81 `dsp_host_render` scenarios after the change - all still
pass), while Ultra-Tap's diffusor now genuinely uses its own per-stage
Delay parameter via `readLinear()`, the same fractional-interpolated-read
approach `Comb`/`PitchShifter` already use elsewhere in `dsp/`.

## The Quickset shape generators

The manual's Spacing/Weights (six shapes each: constant, linear
increasing/decreasing, exponential increasing/decreasing, random) and
Pans (nine configurations) are implemented as one-shot generators -
`applySpacingShape()`/`applyWeightsShape()`/`applyPansShape()` -  that
overwrite all 12 Tedium values at once, matching the manual's own
warning: "Using the Quickset parameters will change the related settings
in the Tedium mode... These parameters are used to 'preset' all of the
Tedium values." Not a persistent transform layered on top of Tedium -
calling one of these and then hand-editing an individual tap afterward
leaves that tap's new value in place, exactly like the real hardware.

## Known simplifications

- **No enforced 1450ms combined cap across all 16 delays** (4 allpass +
  12 taps). Each parameter is independently clamped to its own
  documented range (allpass stages to 800ms, the 12-tap line to a
  1450ms cumulative total) rather than additionally enforcing the
  manual's cross-parameter "OOPS" limit, since the working buffer here
  has ample headroom regardless (unlike the real hardware's shared 64K-
  word delay memory - see `docs/eventide-h3000-notes.md`).
- **No Glide** on Length/Diffusion/Width changes.
- **Random shapes use this Block's own xorshift PRNG**, not a claim
  about the real hardware's randomization algorithm.

## Status

Verified via:
1. The full `UltraTap` Block: an impulse produces finite output with
   energy appearing well after t=0 (confirming the 12-tap cumulative
   delay line actually spreads the signal over time, not just a
   same-sample pass-through), and switching to the "all taps hard left"
   Pan shape produces left-channel energy roughly 5x greater than right
   (confirming Pan/Width are actually wired through).
2. All 5 existing Lexicon PCM81 `dsp_host_render` scenarios (concert_hall,
   plate, chamber, infinite, inverse) re-verified unchanged after the
   `Allpass`/`DiffuserChain` extension above, confirming the opt-in
   design didn't disturb already-shipped behavior.
3. `dsp_host_render ultra_tap` renders an impulse response end to end
   with finite output and a sensible RMS curve.

`patches/eventide/ultra_tap/` (3-knob mapping: Left = Length, Mid =
Diffusion, Right = Mix - Width/Feedback stay at their built-in defaults
since the hardware only has 3 knobs, the JUCE plugin exposes the full
36-parameter Tedium set plus the Quickset shape dropdowns) and the
`EventideUltraTapPlugin` JUCE target both build clean, verified by
launching the actual Standalone build headlessly (Xvfb) and confirming
both the parameter list (including the three shape choice dropdowns) and
the architecture diagram render correctly. As with the other patches in
this archive, the ARM cross-toolchain isn't available in this sandbox,
so the Patch adapter is verified via `make -n` plus a host-compiler
build under `-fno-exceptions -fno-rtti`, not an actual `.endl` build.
