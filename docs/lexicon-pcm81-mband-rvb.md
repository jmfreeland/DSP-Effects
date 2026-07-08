# Lexicon PCM81-style M-Band+Rvb Algorithm

Stage 1 (functional): the third of the PCM81's five 6-Voice algorithms,
after Glide>Hall and Chorus+Rvb (see `docs/lexicon-pcm81-glide-hall.md`,
`docs/lexicon-pcm81-chorus-rvb.md`, and `docs/lexicon-pcm81-reference.md`).
Per `CLAUDE.md`'s Primitive → Component → Block → Graph layering:

- **New Primitives/Components**: none — reuses `DelayLine`,
  `DiffuserChain`, `OnePoleLowpass`, and `StateVariableFilter` directly.
- **Block**: `dsp/include/dsp/algorithms/MBandRvb.h` — a 6-voice
  multiband EQ'd delay (three voices reading from a shared left-channel
  tapped delay line, three from a shared right-channel one, each with an
  independent 2-pole HiCut/LoCut filter pair) running **in parallel**
  with a fixed, owned `Chamber` reverb instance, with feedback re-
  entering *through* the shared diffuser stage each pass.
- **Graph**: `dsp/include/dsp/graphs/MBandRvbAlgorithm.h` — the Block
  plus the Controls row's In Lvl/Pan and the shared FX Width/Hi-Cut/
  Adjust/Mix chain.

## Diffusion inside the feedback loop — the real topology difference from Chorus+Rvb

Per the manual: "This effect features six separately adjustable voices,
each with its own level control, delay time, low and high frequency
filters, feedback and pan controls. The multi-band effect is in parallel
with a Chamber effect... Note also that, in this particular algorithm,
the diffuser is within the feedback paths of the multi-band voices. This
allows you to create filtered echoes that grow more diffuse with each
repeat, or to create effects with filtered echoes passing through the
reverberator."

Chorus+Rvb's feedback bypasses its diffuser entirely and writes straight
back to its delay bank (see `docs/lexicon-pcm81-chorus-rvb.md`). M-Band
+Rvb is architecturally different: `MBandRvb::processSample()` sums each
channel's raw input with that channel's own filtered voices' feedback
*before* running it through `leftDiffuser_`/`rightDiffuser_`, and only
the diffuser's *output* is written into `leftBank_`/`rightBank_`. Every
recirculation therefore passes through the allpass chain again, matching
the manual's own description of progressively more diffuse repeats.

## Independent per-voice HiCut/LoCut, ~12dB/octave

The generic Filters row confirms: "The low cut and high cut filters
operate at 12dB/octave. There are individual low cut and high cut
filters for each of six voices... 20-20,000Hz." `setVoiceHiCut()`
cascades two `OnePoleLowpass` stages (6dB/octave each, no cutoff ceiling,
covering the full 20-20,000Hz range) and `setVoiceLoCut()` uses a single
`StateVariableFilter`'s highpass output (a genuine 2-pole/12dB-octave
response). The SVF's own documented stability ceiling (`sampleRate/6`,
~8kHz at 48kHz — see `dsp/include/dsp/StateVariableFilter.h`) caps LoCut
below the full 20kHz the manual states; this project's plugin/patch
expose LoCut over a practical 20-2000Hz range instead, since a "cut lows"
filter set anywhere near 8-20kHz would mute nearly the entire voice
regardless — a documented simplification, not a verified match to the
hardware's exact filter topology (which likely isn't SVF-based at all).

## Known simplifications

- **LoCut range capped below the manual's full 20-20,000Hz spec** — see
  above.
- **No cross-feedback** — like Chorus+Rvb, the manual groups M-Band+Rvb
  into the same Feedback row description as own-channel-only: "In the
  Chorus+Rvb and M-Band+Rvb algorithms, six voice parameters control the
  feedback level of the voice delays... left to left... right to right,"
  with no Cross-Feedback parameters listed (that combination is reserved
  for Glide>Hall alone).
- **FX/Rvb Width behavior** is the same original `rotateStereoWidth()`
  reconstruction used throughout this archive's PCM81 side.

## Status

Verified via isolated Block and Graph smoke tests (`dsp::algorithms::
MBandRvb`, `dsp::graphs::MBandRvbAlgorithm`): finite output with real
energy for an impulse through a representative 6-voice multiband patch;
an isolated single-voice decay test confirms the diffuser-in-feedback-
loop topology stays bounded (RMS measured ~2.18e-8 one second in vs.
~2.39e-18 three seconds in, a clean exponential decay rather than a
runaway); `FxMix=0` vs. `FxMix=1` measurably differ (confirming the
parallel topology actually routes through the reverb); `Mix=0` returns
the dry signal unchanged. `dsp_host_render mband_rvb` renders a 6-second
impulse response with a monotonically-decaying RMS curve (-48dBFS down
to below -159dBFS). `patches/lexicon/mband_rvb/` (3-knob mapping:
Left = all-voice Feedback, Mid = FX Mix, Right = Mix; footswitch
press = bypass, hold = freeze) and the `LexiconMBandRvbPlugin` JUCE
target both build clean. As with every other patch in this archive, the
ARM cross-toolchain isn't available in this sandbox, so the Patch
adapter is verified via `make -n` plus a host-compiler build under
`-fno-exceptions -fno-rtti`, not an actual `.endl` build.
