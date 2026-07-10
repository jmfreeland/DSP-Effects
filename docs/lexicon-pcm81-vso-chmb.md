# Lexicon PCM81-style VSO-Chmb Algorithm

Stage 1 (functional): the sixth of the PCM81's seven Pitch algorithms,
and the last of the five true Dual-FX ones. Per the manual: "Like the
Stereo-Chmb algorithm, VSO-Chmb is combined with a stereo chamber
reverb" - identical to `docs/lexicon-pcm81-stereo-chmb.md` in every
respect but one added `Varispeed` parameter.

- **No new Primitives/Components/Blocks**: `dsp/include/dsp/graphs/
  VSOChmbAlgorithm.h` is `class VSOChmbAlgorithm : public
  StereoChmbAlgorithm` - inheriting the entire Submixer/Routing/Stereo
  Shifter machinery wholesale (matching the Block tier's own `Infinite :
  public Chamber` shape: "Chamber + different defaults") and adding
  exactly one new setter, `setVarispeed()`, rather than duplicating
  ~250 lines of already-proven Graph code for a single closed-form
  calculation on top of an existing parameter (`setShiftCents()`,
  already public).

## The Varispeed formula, derived and checked against both of the manual's own worked examples

Per the manual: "This algorithm is a utility program designed to
provide pitch correction of varispeed material... Simply match the
value of the Varispeed parameter to the varispeed setting of the
playback source." Working out the exact formula from its own two
worked examples (not just guessed) was necessary because the manual's
prose ("the playback speed must be increased by 20%") reads ambiguously
against the numbers that follow it:

- Example 1: compressing a 30-second spot to 24 seconds "must be
  increased by 20%," producing "an upward pitch shift of 386 cents."
  A literal "increase playback speed by 20%" (multiply by 1.20) gives
  30/1.20 = 25s, not 24s, and 1200*log2(1.20) = 316 cents, not 386 -
  neither number matches. Solving for the speed multiplier that
  actually produces both 24s *and* 386 cents simultaneously gives
  1.25x (30/1.25 = 24s exactly; 1200*log2(1.25) = 386.3 cents exactly) -
  meaning the manual's own "20%" describes the *duration reduction*
  (24 = 30*(1-0.20)), not the speed multiplier directly.
- Example 2: expanding 28s to 30s "must be decreased by 7.14%" checks
  out under the same convention: speed multiplier = 1/(1-(-7.14/100)) =
  1/1.0714 = 0.9333, and 28/0.9333 = 30s exactly.

So the formula this Graph implements:

```
speedMultiplier = 1 / (1 - varispeed/100)
shiftCents = -1200 * log2(speedMultiplier) = 1200 * log2(1 - varispeed/100)
```

verified directly against both worked examples in a smoke test (see
Status below) before being trusted - this project's standing rule of
not shipping a formula it hasn't checked against the primary source's
own numbers, not just its prose.

## Known simplifications

- **No separate Low Pitch control** - the manual's own edit matrix
  lists both `Low Pitch` and `Varispeed` as VSO-Chmb's two Pitch-row
  parameters (Stereo-Chmb has only `Low Pitch`); Low Pitch itself is
  already a documented simplification throughout this archive's
  Pitch-class algorithms (see `docs/lexicon-pcm81-quad-hall.md`).

## Status

Verified the Varispeed formula directly against both of the manual's
own worked examples (30s->24s at +20% gives -386.3 cents; 28s->30s at
-7.14% gives the correct 0.9333x multiplier), plus a finite-output
engine smoke test. `dsp_host_render vso_chmb` renders a 3-second 220Hz
tone burst with +20% varispeed correction through the default parallel
patch, with a decaying RMS curve. `patches/lexicon/vso_chmb/` (3-knob
mapping: Left = Varispeed -35%..+55%, Mid = FX Mix, Right = Mix;
footswitch hold toggles Routing) and the `LexiconVSOChmbPlugin` JUCE
target both build clean, verified via `make -n` plus a host-compiler
build under `-fno-exceptions -fno-rtti` for the Patch adapter (the ARM
cross-toolchain isn't available in this sandbox) and a headless Xvfb
Standalone launch for the plugin.

This completes all five true Dual-FX Pitch algorithms (Dual-Chmb,
Dual-Plt, Dual-Inv, Stereo-Chmb, VSO-Chmb). Only **Pitch Correct**
remains to complete the entire Pitch class and the PCM81's full
17-algorithm roadmap - see `docs/lexicon-pcm81-reference.md`'s "The
Pitch algorithms" section.
