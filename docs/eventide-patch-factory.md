# Eventide H3000-style Patch Factory

Stage 1 (functional): the twelfth Eventide H3000 algorithm in this
archive, Algorithm 111 per the Instruction Manual's own numbering (right
after Dual Digiplex, Algorithm 110 - see `docs/eventide-h3000-notes.md`
and `docs/eventide-dual-digiplex.md`). Per `CLAUDE.md`'s Primitive →
Component → Block → Graph layering:

- **New Primitives**: `NoiseGenerator` (deterministic xorshift32 white
  noise, the "Noise Gen" block) and `StateVariableFilter` (a Chamberlin
  SVF producing lowpass/highpass/bandpass simultaneously from one
  cutoff+Q pair - the "tuneable filter" blocks, whose manual page states
  plainly "At a Q factor of 1, the filter will oscillate at the center
  frequency," a real self-oscillation mode this primitive supports and
  keeps numerically bounded).
- **Block**: `dsp/include/dsp/algorithms/PatchFactory.h` - the full
  modular patch-bay: the two filters, two `DelayLine`s, one
  `PitchShifter`, two scalers, two summing junctions, and a settable
  16-source x 13-destination patch matrix wiring them together.
- **Graph**: `dsp/include/dsp/graphs/PatchFactoryAlgorithm.h` - the
  Block plus input trim (Left-In only, matching the Block's own Patch
  Sources list, which has no "Right Input" entry).

## Why this algorithm, twelfth

Straight numeric order. Per the manual: "The Patch Factory algorithm
gives the user a bit of almost everything... Using these basic effect
elements, a flexible patching scheme and some 'glue'... clever users can
create sound effects limited only by their imaginations." This is a
different kind of algorithm from anything built so far in this archive -
not one fixed signal path with parameters, but a small modular synthesis
environment. It's the first H3000 algorithm whose actual point *is* the
routing, so this Block is built as a genuine patch matrix rather than a
fixed topology with a "patching flavor."

## The patch matrix: 16 sources, 13 destinations, one sample of latency per hop

The manual's own parameter list (#19-31, "Patch destinations") and the
"Patch Sources" table that follows it are the literal spec: every one of
13 destinations (both filter inputs, both delay inputs, both scaler
inputs, both summing junctions' two inputs each, the pitch shifter
input, and the two channel outputs) can be independently routed from any
of 16 sources (Left Input, Noise Gen, both scalers, both delays, both
summing junctions, all three taps of each filter - Lowpass/Bandpass/
Highpass, listed as three *separate* sources, confirming each filter
genuinely outputs all three simultaneously rather than switching between
modes - the pitch shifter, and a silent Null Input). `setPatch()` takes
any `Destination`/`Source` pair.

A real patch cord on real hardware could tie destinations and sources
into a zero-delay loop (e.g. patching Sum 1's "b" input from Sum 2, and
Sum 2's "b" input from Sum 1). Rather than detecting or rejecting such
patches, every cross-connection in this Block reads the *previous*
sample's value of its source - only the two true external inputs (Left
Input and Noise Gen) are read at zero latency. This makes the whole
matrix inherently acyclic for *any* patch a user configures, at the cost
of one sample (~21us at 48kHz) of latency per hop - inaudible, and a
straightforward, honestly-documented simplification rather than an
attempt to reverse-engineer how the real PEL firmware's sequential
single-MAC execution actually ordered these reads.

## Filter self-oscillation stays finite by construction

`StateVariableFilter::setQ(1.0f)` drives the Chamberlin SVF's damping
coefficient down to a clamped minimum (0.02, not exactly 0) rather than
letting it hit exactly zero - the manual's documented behavior at Q=1 is
genuine, audible self-oscillation, and this Block should reproduce that
character, but an unclamped Chamberlin SVF can diverge to non-finite
values over a long render. Verified directly: a burst into a Q=1,
1000Hz-cutoff filter patched straight to output stays finite and bounded
over a full 2-second render (see the Block's own smoke test).

## Known simplifications

- **The factory-default patch is this codebase's own choice**, not a
  literal transcription of one - the manual's block diagram is
  suggestive (Noise Gen through Scale 1, Left Input through Scale 2, a
  Delay/Filter/Output chain on each side) rather than a single "this is
  the one true default patch" statement, since the whole point of this
  algorithm is that every connection is user-selectable. The default
  chosen here (documented in the Block's own `prepare()`) exercises
  every element type at least once: Left Input → Scale 2 → Sum 2 →
  Delay 1 → Filter 1 (Lowpass tap) → Left Output, and Left Input → Pitch
  Shift → Filter 2 (Lowpass tap) → Right Output, with Sum 1/Delay 2
  computed but unused downstream by default, available for re-patching.
- **No Trigger/Sweep/Deglitch expert parameters** - Patch Factory's own
  manual page doesn't have these (that's Stutter, Algorithm 112,
  immediately next); Patch Factory's only "expert" surface is the patch
  matrix itself, which is fully implemented.
- **Scalers are plain gain stages**, not level meters/limiters - the
  manual describes Scale 1/2 as simple attenuators (-100%..100%, negative
  inverts phase), matching a plain multiply.

## Status

Verified via:
1. Two new-primitive smoke tests: `NoiseGenerator` produces bounded,
   roughly zero-mean, non-constant output; `StateVariableFilter`
   attenuates a 1kHz tone by more than 70% through a 200Hz-cutoff
   lowpass while passing more than 70% of it through a 6000Hz-cutoff
   lowpass (both at Q=0), and sustains audible resonant energy well
   after an input burst stops at Q=1 while staying finite throughout.
2. `PatchFactory` Block smoke tests: the factory-default patch produces
   finite, non-silent output on both channels from a burst; re-patching
   Left Output directly to Noise Gen produces a left channel with
   noise-like (high sample-to-sample variance) rather than smooth-decay
   character, confirming re-patching genuinely changes the signal path
   rather than the matrix being decorative; a Q≈1 self-oscillating
   filter patched straight to output stays finite over 2 seconds.
3. `dsp_host_render patch_factory` renders the factory-default patch end
   to end with finite output.

`patches/eventide/patch_factory/` (3-knob mapping: Left = Cutoff, shared
by both filters; Mid = Pitch Shift amount; Right = shared dry/wet Mix -
the patch matrix itself stays at its factory default, since the
hardware's 3 knobs can't drive a 13-destination matrix; footswitch hold
toggles a "self-oscillate" mode pushing both filters' Q toward 1) and the
`EventidePatchFactoryPlugin` JUCE target both build clean. The JUCE
plugin, unlike the hardware Patch, exposes the *entire* patch matrix as
13 dropdown `AudioParameterChoice` parameters (one per destination, each
listing all 16 sources) alongside every basic-element parameter - since
the patch matrix is this algorithm's whole reason for existing, and
`AudioParameterChoice` is already supported by the shared
`LoomPluginEditor` (the same mechanism Diatonic Shift's Scale/Voice and
Ultra-Tap's Spacing/Weights/Pans Shape parameters use). Verified by
launching the actual Standalone build headlessly (Xvfb) and confirming
both the parameter list (all 12 float/percent/cents/seconds parameters
plus all 13 patch-matrix dropdowns, each showing its correct default
source) and the architecture diagram (all 13 stages, correctly labeled
default-patch connections) render correctly. As with the other patches
in this archive, the ARM cross-toolchain isn't available in this
sandbox, so the Patch adapter is verified via `make -n` plus a
host-compiler build under `-fno-exceptions -fno-rtti`, not an actual
`.endl` build.
