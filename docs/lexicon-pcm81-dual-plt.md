# Lexicon PCM81-style Dual-Plt Algorithm

Stage 1 (functional): the third of the PCM81's seven Pitch algorithms,
identical in shape to Dual-Chmb (see `docs/lexicon-pcm81-dual-chmb.md`
for the full design rationale - Submixer, Dual Shifter feedback pattern,
Routing precedence, and verification methodology all carry over
unchanged) with **Plate** in place of Chamber.

- **Block**: reuses `Plate` exactly as built for the 4-Voice class - no
  new reverb-core capability needed.
- **Graph**: `dsp/include/dsp/graphs/DualPltAlgorithm.h` - line-for-line
  the same Submixer + two-`PitchShiftVoice` "Dual Shifter" shape as
  `DualChmbAlgorithm`, with `Plate`'s own `setAttack()` in place of
  Chamber's `setShape()`/`setSpread()` (Plate has no Shape/Spread swell -
  see `docs/lexicon-pcm81-plate.md`) and its `setEkoFeedback()`/
  `setEkoDelaySeconds()` pre-echo pass-throughs kept (Plate has its own
  recirculating pre-echo, same as Chamber).

## Status

Verified the same way as Dual-Chmb: finite output for both Parallel and
Rvb-into-FX Routing modes. `dsp_host_render dual_plt` renders a
3-second 220Hz tone burst through the default parallel patch with a
decaying RMS curve. `patches/lexicon/dual_plt/` (same 3-knob mapping as
Dual-Chmb: Left = Spread, Mid = FX Mix, Right = Mix; footswitch hold
toggles Routing) and the `LexiconDualPltPlugin` JUCE target both build
clean, verified via `make -n` plus a host-compiler build under
`-fno-exceptions -fno-rtti` for the Patch adapter (the ARM cross-toolchain
isn't available in this sandbox) and a headless Xvfb Standalone launch
for the plugin.
