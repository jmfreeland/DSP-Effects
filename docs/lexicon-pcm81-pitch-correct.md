# Lexicon PCM81-style Pitch Correct Algorithm

Stage 1 (functional): the seventh and last of the PCM81's Pitch
algorithms, completing the archive's entire 17-algorithm PCM81 roadmap
(5 4-Voice + 5 6-Voice + 7 Pitch). Per the manual: "The Vocal Fix Pitch
Correct algorithm is designed to work with monophonic (one note at a
time) vocal sources. The algorithm contains an intelligent pitch shifter
combined with a PCM 81 Chamber reverb. The intelligent pitch shifter
detects the pitch of incoming audio and produces corrections based on
the detected pitch... The reverb follows the pitch shifter in series.
The FX Mix parameter is set to 0% reverb as most applications require
only pitch processing."

- **No new Primitives/Components**: `dsp/include/dsp/graphs/
  PitchCorrectAlgorithm.h` reuses `PitchDetector`, `PitchShifter`,
  `DelayLine`, `OnePoleLowpass`, and `Chamber` unchanged - the same
  building blocks the archive already has.
- **No dedicated Block**: like `QuadHallAlgorithm`, this Graph owns the
  detector/shifter/delay directly and reuses `Chamber` exactly as built,
  with no new reverb-core capability needed.
- **No Submixer**: unlike the other six Pitch algorithms, this one is a
  fixed series chain (corrector, then reverb) - the manual's own diagram
  shows a single signal path, not independent Rvb/FX blocks fed from a
  Sends matrix.

## Signal path

```
L,R -> InLvl -> mono sum -> Delay -+-> PitchDetector
                                    +-> PitchShifter (correction cents) -+-> Chamber -+
                                                                          |            |
                                                           FX Mix (0 = dry corrected, 1 = reverbed)
                                                                          -> FX Width/Hi-Cut/Adjust -> Mix -> out
```

## Chromatic, not diatonic - this is a genuinely different pitch row than Res2>Plate's

Res2>Plate (the 6-Voice class's own pitch-correcting algorithm) has a
Key/Scale/Root/Rule row in its edit matrix and corrects toward the
nearest *diatonic* scale degree - hence its need for
`dsp::DiatonicScale.h`. Pitch Correct's own edit matrix has **no**
Key/Scale/Root row at all: "notes are shifted as close as possible to
the frequency of the detected pitch" relative only to a `Tuning`
reference (410-470Hz, 440 standard). That's a plain 12-TET (equal
tempered) quantizer - round to the nearest semitone, no scale-membership
lookup - so this Graph doesn't need `DiatonicScale.h` at all. An earlier
planning pass (recorded before the manual's own Pitch Correct page had
been read closely) assumed it would reuse the diatonic harmonizer
machinery; re-reading the actual parameter list corrected that.

```cpp
auto rawSemitoneFromTuning = 12.0f * std::log2(detector_.frequencyHz() / tuningHz_);
smoothedSemitoneFromTuning_ = trackingFilter_.process(rawSemitoneFromTuning);
auto nearestSemitone = std::round(smoothedSemitoneFromTuning_);
auto correctionCents = (nearestSemitone - smoothedSemitoneFromTuning_) * 100.0f * correctionAmount_;
```

`Correction` (0..1) blends between no correction and full correction to
that nearest semitone, matching the manual's own 0-100% range. An
additional fixed `Shift Cents`/`Shift Semitones` pair (additive, +-2
octaves) lets the corrected pitch also be transposed, per the manual's
own edit matrix.

## Tracking, including Hold

`Tracking` (Fastest/Fast/Moderate/Slow/Hold) sets a one-pole lowpass
smoothing coefficient on the detected-pitch-in-semitones signal before
quantization - slower Tracking settings resist chasing vibrato or brief
inaccuracies, matching the manual's own description of the parameter as
a speed/stability tradeoff. `Hold` is a distinct mode, not just the
slowest smoothing rate: it freezes `smoothedSemitoneFromTuning_` at its
last value entirely (the smoothing filter stops being fed new readings),
"effectively turning any melody into a pedal tone" per the manual - a
genuinely different behavior than merely smoothing hard.

## No InPan - a deliberate omission, not an oversight

Every other Pitch-class Graph exposes an `InPan` alongside `InLvl`,
because their stereo bus stays split downstream (feeding a real L/R pair
of voices or shifters). Pitch Correct's own diagram shows one detector
and one shifter fed from a single mono-summed signal; a linear pan law's
L/R weights always sum to a constant, so panning the input before
summing it back to mono is mathematically inert (it cancels out
exactly) - unlike the other Graphs, adding an `InPan` here would be a
no-op control. See the class-level doc comment in
`PitchCorrectAlgorithm.h` for the full reasoning; this was caught and
fixed during development after an initial draft copied `QuadHall`'s
InLvl/InPan pattern uncritically.

## Known simplifications

- **No dedicated pitch-shifter Delay/Feedback controls** - the manual's
  own edit matrix doesn't expose them for this algorithm either (unlike
  the Dual-FX Pitch algorithms' Shift Delay/Feedback); Pitch Correct's
  `Delay` parameter is the detector/shifter's own processing latency, a
  different control than the "echo repeat" delay found elsewhere.

## Status

Verified: `PitchDetector` correctly tracks a deliberately "sour" 215Hz
test tone and `PitchShifter` corrects it toward the nearest chromatic
semitone below A3 (220Hz) - confirmed via zero-crossing measurement
(215Hz -> ~219.3Hz vs. the 220Hz target; a rough check, but the
correction amount here is large enough that grain-splice noise doesn't
swamp the measurement, unlike the smaller shifts seen in QuadHall's own
zero-crossing debugging). `Correction=0` passthrough and a finite-output
noise-burst test both confirmed no-crash behavior. `dsp_host_render
pitch_correct` renders a 2-second 215Hz tone burst through the default
patch (Correction=1, FX Mix=0), producing a decaying RMS curve from
-11.8dBFS to -19.6dBFS as the pitch-detector settles.
`patches/lexicon/pitch_correct/` (3-knob mapping: Left = Correction,
Mid = FX Mix, Right = Mix; footswitch hold toggles Tracking between
Fastest and Hold) and the `LexiconPitchCorrectPlugin` JUCE target both
build clean, verified via `make -n` plus a host-compiler build under
`-fno-exceptions -fno-rtti` for the Patch adapter (the ARM
cross-toolchain isn't available in this sandbox) and a headless Xvfb
Standalone launch for the plugin.

This completes the PCM81's entire 17-algorithm roadmap - see
`docs/lexicon-pcm81-reference.md`'s "The Pitch algorithms" section and
its Gap section for the closing status.
