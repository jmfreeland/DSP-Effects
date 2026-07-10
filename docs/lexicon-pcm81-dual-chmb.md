# Lexicon PCM81-style Dual-Chmb Algorithm

Stage 1 (functional): the second of the PCM81's seven Pitch algorithms
(the first of five true "Dual FX" algorithms that use the manual's own
Submixer routing system - Quad>Hall, already built, is the only Pitch
algorithm without one). Per `CLAUDE.md`'s Primitive → Component → Block →
Graph layering:

- **New Component**: `dsp/include/dsp/Submixer.h` - the Sends/Returns
  arithmetic shared by all five true Dual-FX Pitch algorithms (Dual-Chmb,
  Dual-Plt, Dual-Inv, Stereo-Chmb, VSO-Chmb): routes the two main stereo
  inputs into a Rvb block's and an FX block's own stereo inputs (Sends),
  and their two stereo outputs back to the main outputs (Returns), per
  the manual's own discrete-valued Sends/Returns tables. Deliberately
  doesn't handle the third control, Routing - that's a sequencing
  decision only the owning Graph can make.
- **No dedicated Block**: like Quad>Hall, Dual-Chmb reuses `Chamber`
  exactly as built and needs no new reverb-core capability, so
  `dsp/include/dsp/graphs/DualChmbAlgorithm.h` owns the Submixer, the
  Chamber reverb instance, and the two `PitchShiftVoice`s (the "Dual
  Shifter" FX block) directly.
- **Graph**: `DualChmbAlgorithm` - Submixer-routed Chamber + Dual
  Shifter, with Routing (Parallel / Rvb-into-FX series / FX-into-Rvb
  series) handled in `processSample()` since only the Graph owns both
  sub-effects and can sequence them.

## Why this algorithm, and the Dual Shifter's feedback shape

Per the manual: "This algorithm includes a dual pitch shifter combined
with the Chamber reverb. The pitch shifter has two voices. Each voice
has independent controls for pitch, level, delay, pan, feedback and
cross-feedback." Unlike the H3000's own Dual Shift (fully independent
per-channel voices, no shared feedback point at all - see
`docs/eventide-dual-shift.md`), the PCM81's Dual Shifter's Fbk/X-Fbk
description matches the same "own channel vs. opposite channel" pattern
already used for Quad>Hall's own 4 voices (see
`docs/lexicon-pcm81-quad-hall.md`), just scaled down to 2: each voice's
Fbk recirculates into its own input, X-Fbk into the other voice's -
implemented with the same one-sample-latency `lastVoiceOutput_` array
QuadHall already established.

## The Submixer: Sends/Returns as discrete configurations, Routing as owner-sequenced

The manual's Submixer has three controls, but they're not symmetric in
implementation weight. Sends and Returns are each a small, closed set of
labeled stereo-routing configurations (0/100/150/200/300, with Stereo
appearing twice at 0 and 300 as a documented redundant labeling) -
`dsp::Submixer` models these as two 4-value enums with pure arithmetic
`send()`/`receive()` methods, verified directly against every entry in
the manual's own tables. Routing is different: it decides whether the
Rvb and FX blocks run independently (Parallel) or one literally becomes
the other's input (either series direction) - a control-flow decision,
not arithmetic, and one only the Graph itself can make since it alone
owns both block instances and must call them in the right order. The
manual's own note that "Routing value takes precedence over Sends/
Returns" falls out naturally from this split: in series mode, the Graph
doesn't call `submixer_.receive()` with the upstream block's raw output
at all - it zeroes it before merging, so whichever Returns setting the
user picked can only ever look at the *series chain's* other end.

## Verifying all three Routing modes without a diagram to trust

The Pitch class's own block diagrams (unlike the 6-Voice class, which
had a full parameter glossary rundown) come with the Submixer's "Useful
Configurations" diagrams directly, so this Graph's `processRvb()`/
`processFx()` split was checked against all three: Parallel produces
independent, differently-decaying energy from each block; Rvb-into-FX
produces a pitch-shifted reverb tail (audibly and numerically distinct
from Parallel); and a `RvbMix=0, FxMix=0` passthrough check (after
letting the Hi-Cut filter settle) confirms Stereo Returns sums both
blocks' contributions exactly as the manual's own table specifies (not
a bypass - `Sends::kStereo` + `Returns::kStereo` with both blocks fully
transparent doubles the input, since both blocks independently pass it
through to the same output channel).

## Known simplifications

- **RvbInWidth/FxInWidth/RvbOutWidth/FxOutWidth/RvbHiCut/RvbLoCut/
  FxHiCut/FxLoCut are not modeled** - the manual's own Submixer row
  lists independent stereo-width and filter controls per block, on top
  of the In Level/Mix this Graph does implement; deferred as secondary
  polish controls on top of the already-verified core routing, the same
  way Quad>Hall deferred MstrCents/Low Pitch.
- **Splice is a single shared control across both voices**, matching
  Quad>Hall's own simplification.
- **FX/Rvb Width behavior** is the same original `rotateStereoWidth()`
  reconstruction used throughout this archive's PCM81 side.

## Status

Verified via `DualChmbAlgorithm` smoke tests: all three Routing modes
produce finite, musically-distinct output; the dry-passthrough/Returns
arithmetic check above. `dsp::Submixer` itself is verified against every
entry in the manual's own Sends/Returns tables in isolation. `dsp_host_
render dual_chmb` renders a 3-second 220Hz tone burst through the
default parallel patch with a decaying RMS curve. `patches/lexicon/
dual_chmb/` (3-knob mapping: Left = Spread - scales the 2 voices' pitch
amounts symmetrically from unison to +-1 octave, Mid = FX Mix, Right =
Mix; footswitch press = bypass, hold = toggle Routing between Parallel
and Rvb-into-FX series) and the `LexiconDualChmbPlugin` JUCE target both
build clean. As with every other patch in this archive, the ARM
cross-toolchain isn't available in this sandbox, so the Patch adapter is
verified via `make -n` plus a host-compiler build under `-fno-exceptions
-fno-rtti`, not an actual `.endl` build.

`Dual-Plt` and `Dual-Inv` reuse this exact Submixer + Dual Shifter shape
with Plate and Inverse in place of Chamber, respectively - see
`docs/lexicon-pcm81-reference.md`'s "The Pitch algorithms" section for
what's next (Stereo-Chmb, VSO-Chmb, and Pitch Correct).
