# Eventide H3000-style Dual Digiplex

Stage 1 (functional): the eleventh Eventide H3000 algorithm in this
archive, Algorithm 110 per the Instruction Manual's own numbering (right
after Long Digiplex, Algorithm 109 - see `docs/eventide-h3000-notes.md`
and `docs/eventide-long-digiplex.md`). Per `CLAUDE.md`'s Primitive →
Component → Block → Graph layering:

- **Primitives reused**: `DelayLine` and `GlideParameter` (the same pair
  Long Digiplex uses) - no new primitives.
- **Block**: `dsp/include/dsp/algorithms/DualDigiplex.h` - two fully
  independent delay lines, each with its own feedback, Glide-smoothed
  delay-time change, and a shared Repeat/Stereo-Input mode.
- **Graph**: `dsp/include/dsp/graphs/DualDigiplexAlgorithm.h` - the
  Block plus independent Left/Right input trim.

## Why this algorithm, eleventh

Straight numeric order. Per the manual, Dual Digiplex is "two completely
independent delay lines" - the direct two-channel sequel to Long
Digiplex's single line, in the same way Dual Shift (102) was the
independent-channel sequel to Layered Shift (101).

## A deliberate non-reuse: hand-rolled two channels rather than wrapping `LongDigiplex`

`LongDigiplex::processSample()` hard-codes "Left-In-only, duplicate the
one delayed signal to both outputs" - that's not a parameter setting,
it's the whole point of the Block (matching the manual's "the output is
sent to both right and left channels" language for Algorithm 109). Dual
Digiplex needs each channel to own its input, its own feedback state, and
its own delay/glide target, so trying to compose two `LongDigiplex`
instances would mean fighting that hard-coded behavior rather than reusing
it. Instead `DualDigiplex` hand-rolls the same Delay+Feedback+Glide shape
twice, independently - the same reasoning already used for `DualShift.h`
not trying to awkwardly reuse `LayeredShift.h`.

A `stereoInput_` flag controls whether the right line reads the right
input (true, the default) or the left input (false) - covering both the
"two independent stereo delays" reading and a "two taps off one mono
source" reading without adding a second Block.

## Verified true channel independence

As with Dual Shift and Stereo Shift, "independent channels" is checked
numerically, not just assumed from the code shape: rendering with both
channels active and comparing the left channel's output against a second
render with the right channel's input silenced entirely produces
bit-identical left-channel energy (difference on the order of 1e-9,
floating-point noise floor) - confirming the right line's state has zero
influence on the left line's output.

## Known simplifications

- **No enforced 1.4s Repeat capture semantics beyond mute-on-repeat** -
  same simplification already documented for the Swept-family algorithms'
  own Repeat controls and reused as-is for Long Digiplex.
- **Repeat and Stereo Input are shared, not per-channel** - the manual
  describes Dual Digiplex as symmetric ("two completely independent delay
  lines"), so the two mode flags apply uniformly to both rather than
  doubling the control surface.

## Status

Verified via:
1. The full `DualDigiplexAlgorithm` Graph: a burst played into Left
   Delay=0.2s / Right Delay=0.5s (Glide disabled for a clean instant
   onset) produces a left-channel echo at 0.2s and a right-channel echo
   at 0.5s, each independently timed and leveled per its own Feedback.
2. Channel independence, per above: silencing the right channel's input
   leaves the left channel's echo energy unchanged to within floating-point
   noise.
3. `dsp_host_render dual_digiplex` renders a burst end to end with finite
   output across both channels.

`patches/eventide/dual_digiplex/` (3-knob mapping: Left = Left Delay,
Mid = Right Delay, Right = shared Mix - Feedback stays at its built-in
default of 0.3 on both channels since the hardware only has 3 knobs, the
JUCE plugin exposes independent Left/Right Feedback) and the
`EventideDualDigiplexPlugin` JUCE target both build clean, verified by
launching the actual Standalone build headlessly (Xvfb) and confirming
both the parameter list (all 10 parameters, including the Glide
Enabled/Repeat/Stereo Input checkboxes) and the architecture diagram
(Left Input/Right Input feeding L Delay/R Delay with correctly-labeled
mono/stereo-mode connections, each with its own feedback loop, into
independent Left/Right Output) render correctly. As with the other
patches in this archive, the ARM cross-toolchain isn't available in this
sandbox, so the Patch adapter is verified via `make -n` plus a host-compiler
build under `-fno-exceptions -fno-rtti`, not an actual `.endl` build.
