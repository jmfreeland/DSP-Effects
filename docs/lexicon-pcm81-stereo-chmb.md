# Lexicon PCM81-style Stereo-Chmb Algorithm

Stage 1 (functional): the fifth of the PCM81's seven Pitch algorithms,
and the first of the two (with VSO-Chmb) to use a genuinely different
FX block than Dual-Chmb/Dual-Plt/Dual-Inv's "Dual Shifter." Per
`CLAUDE.md`'s Primitive → Component → Block → Graph layering:

- **No new Primitives/Components**: reuses `dsp::algorithms::StereoShift`
  - already built for the Eventide H3000's own Algorithm 103 - unchanged
  as the FX block. The first Block reused across both device families in
  this archive's Pitch-shift-class algorithms, alongside `householderMix()`'s
  own existing cross-family reuse in the reverb tanks.
- **No dedicated new Block**: like Dual-Chmb, this Graph owns `Chamber`
  and `StereoShift` directly plus a `Submixer`, with no new reverb-core
  capability needed.
- **Graph**: `dsp/include/dsp/graphs/StereoChmbAlgorithm.h` - Submixer-
  routed Chamber + Stereo Shifter, with Routing handled in
  `processSample()` exactly like Dual-Chmb/Dual-Plt/Dual-Inv.

## Why StereoShift is exactly the right reuse

Per the manual: "The Stereo-Chmb algorithm is optimized for the best
possible shifted audio quality while maintaining the stereo imagery of
the source material... This is a true stereo pitch shifter which
maintains the stereo image of source material." That's a direct match
for the H3000's own Algorithm 103 description already quoted in
`docs/eventide-stereo-shift.md`: "The Stereo Pitch Shift algorithm is
for operation with true stereo inputs... Parameters of both channels
adjust together" - one shared cents value drives two independent
`PitchShiftVoice`s in lockstep (not two unrelated mono voices, as
Dual-Chmb's own "Dual Shifter" is), each channel's own feedback
returning only into itself. Building this confirmed the two
manufacturers described functionally the same mechanism for the same
underlying problem (stereo-coherent pitch shifting) independently -
exactly the kind of convergence this archive watches for as a signal
that a Component is honestly reusable, not coincidentally similar.

## Normalizing Mix across every Dual-FX Pitch algorithm

`StereoShift` has its own internal `setMix()` (it was built as a
complete standalone H3000 effect, dry/wet included). Reusing it here
as an FX *block* rather than a whole effect would double up the mix
control - the Submixer-based Graphs already have their own `fxMix_`
(matching `DualChmbAlgorithm`'s pattern: dry-block-input vs.
wet-block-output, blended by this Graph, not the block itself). So
`prepare()` pins `shifter_.setMix(1.0f)` permanently (always fully wet
internally) and lets this Graph's own `fxMix_` do the blending - the
same shape `PitchShiftVoice` (which has no Mix of its own at all)
already established for Dual-Chmb, now extended to a block that
*does* carry its own Mix, so every Dual-FX Pitch algorithm's FX Mix
control behaves identically regardless of which FX block sits behind
it.

## Status

Verified via `StereoChmbAlgorithm` smoke tests: finite output for both
Parallel and Rvb-into-FX Routing; a settled dry-passthrough check
(RvbMix=0, FxMix=0 -> output doubles the input, matching Dual-Chmb's
own established Stereo Sends/Returns arithmetic); a measurable-difference
check confirming FX-on output differs substantially from FX-off for
identical input (proving the shifter actually engages through the
Submixer wiring, not just passes through). `dsp_host_render stereo_chmb`
renders a 3-second 220Hz tone burst through the default parallel patch
with a decaying RMS curve. `patches/lexicon/stereo_chmb/` (3-knob
mapping: Left = Shift amount +-1 octave, Mid = FX Mix, Right = Mix;
footswitch hold toggles Routing) and the `LexiconStereoChmbPlugin` JUCE
target both build clean, verified via `make -n` plus a host-compiler
build under `-fno-exceptions -fno-rtti` for the Patch adapter (the ARM
cross-toolchain isn't available in this sandbox) and a headless Xvfb
Standalone launch for the plugin.

`VSO-Chmb` reuses this exact Stereo Shifter + Submixer shape, adding
one Varispeed parameter (a closed-form percentage-to-cents mapping) -
see `docs/lexicon-pcm81-reference.md`'s "The Pitch algorithms" section
for what's next (VSO-Chmb, then Pitch Correct to complete all seven).
