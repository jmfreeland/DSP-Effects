# Lexicon PCM81-style Chorus+Rvb Algorithm

Stage 1 (functional): the second of the PCM81's five 6-Voice algorithms,
right after Glide>Hall (see `docs/lexicon-pcm81-glide-hall.md` and
`docs/lexicon-pcm81-reference.md`). Per `CLAUDE.md`'s Primitive →
Component → Block → Graph layering:

- **New Primitives/Components**: none — reuses `DelayLine`, `LFO`, and
  `DiffuserChain` directly.
- **Block**: `dsp/include/dsp/algorithms/ChorusRvb.h` — a 6-voice stereo
  chorus (three voices reading from a shared left-channel tapped delay
  line, three from a shared right-channel one, each independently
  modulated by its own `LFO`) running **in parallel** with a fixed,
  owned `Plate` reverb instance.
- **Graph**: `dsp/include/dsp/graphs/ChorusRvbAlgorithm.h` — the Block
  plus the Controls row's In Lvl/Pan and the shared FX Width/Hi-Cut/
  Adjust/Mix chain. Thinner than `GlideHallAlgorithm.h`: this algorithm's
  own Controls row has no separate Voice Diffusion slot.

## Why parallel, not series — and what "shared Diffusion" means

Per the manual: "The 6-voice chorus is in parallel with a plate
algorithm, providing two independent stereo effects" and "In the
M-Band+Rvb and the Chorus+Rvb algorithms, the reverb effect is in
parallel with the 6-voice effect. Use FX Mix to set the balance of the
6-voice effect and the reverb" — the opposite of Glide>Hall/Res1>Plate/
Res2>Plate's series topology. `ChorusRvb::setFxMix()` blends two signals
computed from the *same* input rather than chaining one into the other:
0 = the chorus voices only, 1 = the Plate reverb only.

The manual adds one more wrinkle: "Note that the Diffusion parameter
(Rvb Design 2.1) is shared by both the reverb and the chorus effect."
This project reads that as a shared *control value*, not shared
*processing* — the chorus path gets its own small `DiffuserChain` pair
(`chorusDiffuserLeft_`/`Right_`, matching `ConcertHallAlgorithm`'s own
Voice Diffusion stage lengths) rather than tapping into Plate's internal
diffuser, but `ChorusRvb::setDiffusion()` drives both at once.

## Six independently-modulated chorus voices, no cross-feedback

Unlike Glide>Hall, the manual's Feedback row text for this algorithm is
explicit that only *own-channel* feedback exists: "Voices 1, 2, and 3
Fbk control the individual voice feedback levels from the left channel
voice delay outputs to the left channel delay feedback input. Voices 4,
5, and 6 Fbk control ... right channel ... right channel" — no
Cross-Feedback parameters are listed for this algorithm (the manual
reserves that combination for Glide>Hall alone: "In the Glide>Hall
algorithm, the Feedback row provides both Feedback and Cross Feedback
parameters"). `ChorusRvb.h`'s two write buses (`leftBank_`/`rightBank_`)
therefore only sum each bank's own three voices' feedback, simplifying
the topology relative to `GlideHall.h`.

Each voice's own modulation follows the Chorus row's parameter pair
directly: "Depth provides settings of 0-500ms... Rate parameter can be
set to 0Hz (Off) or 0.01-3.50Hz... Depths of 10-30ms combined with Rates
as high as 0.50Hz provide subtle chorusing and multivoicing effects.
Depths of hundreds of milliseconds combined with higher Rates provide a
wide range of pitch shifting effects." Each voice owns an independent
`dsp::LFO` whose sine output scales that voice's own Depth (converted to
samples) and is added to its base Delay Time tap position before a
`DelayLine::readLinear()` call — matching the manual's own description of
per-voice Depth/Rate rather than the block diagram's single "Rate" box,
which reads as a simplified visual grouping of six independent LFO
generators rather than one shared oscillator (the itemized parameter
list, Chorus row 5.0-5.6, is the source of truth here, the same
resolution principle used elsewhere in this archive when a diagram and a
parameter table disagree on granularity).

Master Depth/Rate (0-200%) scale every voice's own Depth/Rate together
without altering the underlying per-voice values — the same "Master"
proportional-scaling convention Swept Combs already established for its
own six lines.

## Known simplifications

- **FX Width** uses `rotatePcm81Width()` (see `dsp/StereoRotate.h`), a
  re-phasing of the archive's original `rotateStereoWidth()`
  reconstruction so its degrees convention matches the manual's own
  display table (0=MONO, 45=STEREO/normal) instead of that primitive's
  own 0-is-identity convention - real preset Width values previously
  collapsed one channel to a fraction of the other's level.
- **Chorus path's own Diffusion is a separate `DiffuserChain` instance**
  from Plate's internal one (see above) — a shared control value, not a
  shared signal path, since the manual's block diagram draws the chorus
  path's Diffusion box distinctly from Plate's own sub-diagram. The same
  "one decoded value, two internal filters" pattern applies to Controls
  High Cut: `ChorusRvbAdapter::importPcm80Preset()` applies it to both
  the Graph's shared `hiCut` and the Block's own `chorusHighCut` (ahead
  of the delay bank) - it used to only touch `hiCut`, leaving
  `chorusHighCut` stuck at a hardcoded 10kHz default for every preset,
  an extra unintended low-pass stage.
- **Per-voice LFO phases are now staggered** (`i/kNumVoices` around the
  cycle, set once in `prepare()`) instead of every voice defaulting to
  phase 0 - found via a hardware-vs-VST comparison where real Prime Blue
  sounded like "a proper chorus" and the VST sounded "hollow and
  metallic". Voice1-3 (and separately Voice4-6) share one `DelayLine`
  per channel (`leftBank_`/`rightBank_`), so their read taps sum
  directly; with every voice starting in phase, two voices with similar
  Rates (real presets routinely have several - Prime Blue's own Voice1
  is 2.10Hz, Voice3 2.05Hz) stay near-synchronized for a long time,
  producing a static comb-filter notch pattern instead of a continuously
  sweeping one - confirmed directly with a white-noise render through
  just Voice1(19ms)/Voice3(9ms): a persistent ~100Hz-spaced peak/null
  structure (matching their fixed 10ms delay difference) that becomes
  measurably less locked to that static grid once phases are staggered.

## Status

Verified via isolated Block and Graph smoke tests (`dsp::algorithms::
ChorusRvb`, `dsp::graphs::ChorusRvbAlgorithm`): finite output with real
energy for a sustained tone through a representative 6-voice chorus
patch; `FxMix=0` vs. `FxMix=1` measurably differ (confirming the parallel
topology actually routes through the reverb); Master Depth=0 collapses
the chorus to a static (non-modulating) tap, confirmed by checking that
a steady input tone's output is periodic at the input's own period
(no residual time-varying modulation); `Mix=0` returns the dry signal
unchanged. `dsp_host_render chorus_rvb` renders a sustained 220Hz tone
through the default 6-voice chorus + Plate patch with finite output and
a printed RMS curve. `patches/lexicon/chorus_rvb/` (3-knob mapping:
Left = Chorus Master Depth, Mid = FX Mix, Right = Mix; footswitch
press = bypass, hold = freeze) and the `LexiconChorusRvbPlugin` JUCE
target both build clean. As with every other patch in this archive, the
ARM cross-toolchain isn't available in this sandbox, so the Patch
adapter is verified via `make -n` plus a host-compiler build under
`-fno-exceptions -fno-rtti`, not an actual `.endl` build.
