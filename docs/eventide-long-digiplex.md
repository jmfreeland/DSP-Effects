# Eventide H3000-style Long Digiplex

Stage 1 (functional): the tenth Eventide H3000 algorithm in this
archive, Algorithm 109 per the Instruction Manual's own numbering (right
after Ultra-Tap, Algorithm 108 - see `docs/eventide-h3000-notes.md` and
`docs/eventide-ultra-tap.md`). Per `CLAUDE.md`'s Primitive → Component →
Block → Graph layering:

- **Primitives reused**: `DelayLine` and `GlideParameter` (already built
  for the Lexicon PCM81's own Voice/Post-Delay glide behavior) - no new
  primitives.
- **Block**: `dsp/include/dsp/algorithms/LongDigiplex.h` - a single
  delay line with feedback, Glide-smoothed delay-time changes, and
  Repeat.
- **Graph**: `dsp/include/dsp/graphs/LongDigiplexAlgorithm.h` - the
  Block plus input trim.

## Why this algorithm, tenth

Straight numeric order, and a deliberate change of pace after four
increasingly complex algorithms (105-108, the six-line and twelve-tap
families). Per the manual: "Algorithm 109 is one long delay line capable
of recirculating its output back to its input. The output is sent to
both right and left channels." This is the simplest H3000 algorithm
built in this archive so far - no new primitives needed at all, purely
a fresh combination of two already-proven ones (`DelayLine` +
`GlideParameter`).

## A real usage gotcha, caught while testing: Glide settings must be applied before the target that should skip it

`GlideParameter::setTarget()` decides glide-vs-jump using whatever
range/response was configured *at the time of that call* - it doesn't
retroactively reconsider a glide already in flight. A first attempt at
this Block's own smoke test called `setDelaySeconds()` before
`setGlide(..., false)`, intending to disable glide for a clean instant
jump; instead the delay change glided anyway (using the Glide settings
already in effect from `prepare()`'s own defaults), and the test's
timing assertions failed. Not a bug in `GlideParameter` - `LongDigiplex`
and its `LongDigiplexAlgorithm` Graph pass Glide's settings through in
the same order every process block (Glide first, then Delay - see
`LongDigiplexPluginProcessor::processBlock()`), so a live plugin/patch
never hits this ordering trap; it's specifically a "read the setters in
the order you call them" note for anyone driving the Block programmatically
(e.g. in a test or a host integration) rather than through the Graph's
own per-block update order.

## Known simplifications

- **Left In only**, matching the manual's own "the output is sent to
  both right and left channels" (implying a single source, not a stereo
  sum) - Right In isn't part of the signal path, same reasoning already
  used for Layered Shift/Reverse Shift.
- **No enforced 1.4s Repeat capture semantics beyond mute-on-repeat** -
  same simplification already documented for the Swept-family algorithms'
  own Repeat controls (see `docs/eventide-swept-combs.md`).

## Status

Verified via:
1. The full `LongDigiplexAlgorithm` Graph: a burst played into a 0.4s
   delay at Feedback=0.5 (with Glide explicitly disabled before setting
   the delay target, to get a clean instant onset for this specific
   check) produces echoes at exactly 0.4s, 0.8s, and 1.2s, each a
   quarter the energy of the previous (matching feedback=0.5 applied
   twice per repeat, 0.5² = 0.25) - and the Left and Right channels are
   sample-for-sample identical, confirming the "same signal to both
   outputs" behavior.
2. `dsp_host_render long_digiplex` renders a burst end to end with
   finite output (the printed RMS-at-t=0/t=1s summary reads near-silent
   for this specific render, since with Mix=1.0 the dry signal is fully
   replaced and the 100ms-wide echoes at 0.4/0.8/1.2s fall outside both
   the t=0s and t=1s 100ms sampling windows - the WAV file itself
   contains the actual repeating echoes).

`patches/eventide/long_digiplex/` (3-knob mapping: Left = Delay, Mid =
Feedback, Right = Mix - Glide stays at its built-in default since the
hardware only has 3 knobs, the JUCE plugin exposes Glide Speed/Enabled
directly) and the `EventideLongDigiplexPlugin` JUCE target both build
clean, verified by launching the actual Standalone build headlessly
(Xvfb) and confirming both the parameter list and the architecture
diagram (including the compact self-feedback loop arrow) render
correctly. As with the other patches in this archive, the ARM
cross-toolchain isn't available in this sandbox, so the Patch adapter is
verified via `make -n` plus a host-compiler build under
`-fno-exceptions -fno-rtti`, not an actual `.endl` build.
