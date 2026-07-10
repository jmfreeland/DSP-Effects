# Lexicon PCM81-style Dual-Inv Algorithm

Stage 1 (functional): the fourth of the PCM81's seven Pitch algorithms,
identical in shape to Dual-Chmb/Dual-Plt (see
`docs/lexicon-pcm81-dual-chmb.md` for the full design rationale) with
**Inverse** in place of Chamber.

- **Block**: reuses `Inverse` exactly as built for the 4-Voice class -
  Inverse's decay isn't RT60-exponential at all (see
  `docs/lexicon-pcm81-inverse.md`), so this Graph exposes
  `setDuration()`/`setLowSlope()`/`setMidSlope()`/`setShape()` instead
  of `setDecaySeconds()`/`setLowRatio()`/`setLink()`, matching
  `InverseAlgorithm.h`'s own existing setter surface exactly (those base-
  class methods are still technically inherited/callable but fixed
  internally per Inverse's own doc comment, so this Graph doesn't expose
  them at all, avoiding a misleadingly-no-op control).
- **Graph**: `dsp/include/dsp/graphs/DualInvAlgorithm.h` - the same
  Submixer + two-`PitchShiftVoice` "Dual Shifter" shape as
  `DualChmbAlgorithm`, with no `EkoDly`/`EkoFbk` pre-echo pass-throughs
  (the manual scopes that pre-echo mechanism to Plate/Chamber/Infinite
  only, not Inverse).

## Status

Verified the same way as Dual-Chmb: finite output for both Parallel and
Rvb-into-FX Routing modes. `dsp_host_render dual_inv` renders a
3-second 220Hz tone burst through the default parallel patch with a
decaying RMS curve. `patches/lexicon/dual_inv/` (same 3-knob mapping as
Dual-Chmb: Left = Spread, Mid = FX Mix, Right = Mix; footswitch hold
toggles Routing) and the `LexiconDualInvPlugin` JUCE target both build
clean, verified via `make -n` plus a host-compiler build under
`-fno-exceptions -fno-rtti` for the Patch adapter (the ARM cross-toolchain
isn't available in this sandbox) and a headless Xvfb Standalone launch
for the plugin.

This completes the "same shape, different reverb core" trio (Dual-Chmb,
Dual-Plt, Dual-Inv) that make up three of the five true Dual-FX Pitch
algorithms. The remaining two, Stereo-Chmb and VSO-Chmb, use a
genuinely different FX block (a "Stereo Shifter" - one shared cents
value driving two `PitchShifter`s in lockstep, rather than two
independent voices) - see `docs/lexicon-pcm81-reference.md`'s "The
Pitch algorithms" section.
