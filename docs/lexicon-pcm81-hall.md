# Lexicon PCM81-style Concert Hall Algorithm

Two tiers, per `CLAUDE.md`'s Primitive → Component → Block → Graph layering:

- **Block**: `dsp/include/dsp/algorithms/ConcertHall.h` — the reverb core
  alone. Composed from `DiffuserChain`, `Crossover`, `LinearRamp`,
  `rt60ToGain`, `DelayLine`, `OnePoleLowpass`, `LFO`, `FeedbackMatrix`'s
  Householder mix.
- **Graph**: `dsp/include/dsp/graphs/ConcertHallAlgorithm.h` — the full
  PCM81 "Concert Hall" *algorithm*: the Block above wrapped in the
  4-Voice "Reverb Shell" (In Lvl/Pan, Voice Diffusion, 4 `Voice`
  Components, post-delay, Rvb Width, FX Mix/Width/Hi-Cut/Adjust). This is
  what `patches/lexicon/hall/` and the JUCE plugin actually wire up.

See `docs/lexicon-pcm81-reference.md` for the primary-source parameter
definitions both were built from. The full parameter set (~49 controls)
is exposed via the JUCE plugin's `AudioProcessorValueTreeState`; the
Polyend patch keeps its 3-knob mapping with sensible defaults for the rest.

## Scope and honesty

This is an **original implementation inspired by** the general character of
Lexicon's algorithmic reverbs — dense, prime-length diffusion feeding a
mutually-coupled tank, gentle internal modulation, dual-band damped
feedback — not a disassembly or reverse-engineering of the PCM81's
proprietary DSP code (which isn't public). Treat every
"PCM81-style"/"Lexicon-style" label in this repo the same way: topology
and character, not a bit-exact clone. A few controls here are **original
reconstructions of a described behavior** rather than a verified match to
Lexicon's exact implementation — called out explicitly below (Definition,
Depth, and the Width controls).

## Block topology (the reverb core)

```
L,R -> independent early-reflection taps (RefDly/RefLvl per channel) -> earlyTapL/R
L,R -> mono sum * RvbIn -> PreDelay -> DiffuserChain<4> -> diffused (tank input)

  8-line Householder FDN tank, per line i:
    tapped[i]   = delay_i.readLinear(length_i * sizeScale +/- Spin/Chorus wobble)
    damped[i]   = onePoleLowpass_i(tapped[i])              // Rt HC
    low, high   = crossover_i(damped[i])                   // split at Crossover Hz
    decayed[i]  = low*lowGain_i + high*midGain_i            // Low Rt / Mid Rt (Link: scale with Size)
  premix = decayed; householderMix(decayed)
  decayed = lerp(premix, decayed, mixAmount)   // Definition: blend toward unmixed as tank energy drops
  for each line i:
    delay_i.write(diffused * sign_i * 0.5 + decayed[i])

  tankWet = signed sums of tapped[] * RvbOut
  wet = tankWet*tankGain(Depth) + earlyTap*earlyReflectionLevel*earlyGain(Depth)
  wet *= sizeMuteEnvelope (briefly mutes/fades-in on a Size change)
  output = lerp(dry, wet, mix)   // forced to fully wet (mix=1) when owned by the Graph
```

- **Rvb In / Rvb Out**: gain trims (0..1) into and out of the tank
  specifically — Rvb In scales the mono signal entering PreDelay/
  Diffusion; Rvb Out scales only the tank's own contribution to the wet
  output, leaving the early-reflection taps unaffected (matching the
  source material).
- **PreDelay**: a plain `DelayLine`, 0-930ms, gap before the diffuser/tank see any signal.
- **Diffusion**: `DiffuserChain<4>`, prime lengths 211/431/751/1091 samples
  (~4.4/9.0/15.7/22.7ms @ 48kHz). `setDiffusion(0..1)` maps to the shared
  allpass coefficient (0-0.75) across all four stages.
- **Early reflections**: two independent `DelayLine` taps (`RefDly`/
  `RefLvl`, 0-1.2s, one per channel) mixed into the output, parallel to
  the tank.
- **Tank**: 8 delay lines, prime lengths 977-4357 samples (~20-91ms),
  mutually coprime to avoid periodic/metallic ringing. `Size` scales the
  *read* delay length (0.4x-1.0x of the tuned lengths) rather than
  resizing buffers, and — matching the source material's "audio is
  temporarily muted when Size is changed" — triggers a 30ms mute/fade-in
  via `LinearRamp` on change instead of clicking.
- **Dual-band decay**: each line's damped output is split by a `Crossover`
  (one-pole low/high split) at a settable frequency; the low band decays
  at `decaySeconds * lowRatio`, the high band at `decaySeconds`, each via
  `rt60ToGain(lineLength, sampleRate, rt60)` — so all eight lines hit
  -60dB at the same wall-clock time in each band despite differing
  lengths. **Link** (bool): when on, the decay time used here is scaled by
  the current Size instead of staying independent of it.
- **Definition** *(original reconstruction)*: an envelope follower
  (rectify + one-pole lowpass) tracks the tank's own output energy; as it
  drops, the Householder-mixed vector is blended back toward its
  *unmixed* per-line values, proportional to the Definition amount. The
  intent is Lexicon's "echo density buildup rate in the latter part of
  decay... raising it makes the sound choppier, with distinct repetitive
  echo trails" — reduced cross-line mixing late in a decaying tail lets
  individual lines' own echoes read as discrete rather than smoothly
  blended. Driven by signal energy rather than elapsed time since the
  tank is continuously fed, not triggered once. Default 0 (no effect).
- **Depth** *(original reconstruction)*: balances early-reflection vs.
  tank contribution to the output (`earlyGain = 2*(1-depth)`,
  `tankGain = 2*depth`, both = 1 at the neutral default of 0.5). Intended
  to approximate "front-to-rear listener perspective" as immediate/direct
  (front, more early reflections) vs. enveloping/diffuse (rear, more tank).
- **Rt HC damping**: a `OnePoleLowpass` per line, coefficient computed
  from a real cutoff-Hz value (`onePoleLowpassCoefficient`), applied
  before the crossover split — models progressive high-frequency loss
  independent of the low/mid decay-rate split.
- **Spin + Chorus modulation**: alternating tank lines get either a slow
  (~0.08-0.19Hz) "Spin" wobble or a faster (~0.6-1.3Hz) "Chorus" wobble
  via fractional-interpolated reads — both are what keep a static FDN
  from settling into an audibly discrete, metallic set of resonances.
- **Mixing matrix**: Householder reflection (`I - (2/N)*J`) — lossless,
  so tank energy is redistributed, not amplified or lost, on every pass.
- **Freeze**: forces both band gains to ~0.9999 and excludes the diffused
  input from the tank, so whatever's currently ringing sustains
  indefinitely — this matches Lexicon's own "Infinite" algorithm, not
  something invented for this repo.
- Exposes both a whole-block `process(span, span)` and a single-sample
  `processSample(float&, float&)` — the Graph uses the latter to
  interleave the reverb with its Voice Components sample-by-sample.

## Graph topology (the full 4-Voice algorithm)

```
L,R -> InLvl/InPan (2x2 mix, identity at defaults) -+-> ConcertHall (forced fully wet) -> RvbWidth -+-> postDelay L/R -+
                                                     |                                              |                 |
                                                     +-> VoiceDiffusion -> Voice 1..4 (parallel) ----+-- FX Mix -------+-- postDelayMix
                                                                                                                        |
                                                                                     FX Width (StereoRotate, -360..360deg)
                                                                                     Hi-Cut (one-pole)
                                                                                     FX Adjust (output gain, dB)
output = lerp(dry, that, mix)   // the top-level dry/wet control, dry = original pre-InLvl/Pan input
```

- **In Level / In Pan**: `In Level L/R` (-1..1, sign = phase) scales each
  input channel before it reaches the effects (not the dry path used for
  the final Mix blend). `In Pan L/R` (-1..1) route each physical input
  channel into the effect's left/right input via a 2x2 mix; defaults
  (-1, +1) are an exact identity pass-through, matching the source
  material's "50L/50R = unmodified stereo imaging."
- **Voice Diffusion**: a small `DiffuserChain<2>` (97/149-sample stages)
  ahead of the 4 Voices, independent of the reverb's own Diffusion —
  controls echo density in the delay-voice section specifically.
- **Voice** (Component, `dsp/include/dsp/Voice.h`): a `Comb` (continuously
  settable delay length, not fixed to buffer capacity) plus level and a
  simple linear pan. Four independent voices, each with Delay
  (0-1.365s)/Feedback/Level (both -1..1, negative = phase inverted)/Pan.
  Defaults give a modest slapback (Voice 1: 90ms, Voice 2: 130ms) so the
  layer is audible without being gimmicky; Voices 3-4 default off. No
  master controls (simultaneous scaling of all 4 voices) — each is set
  independently via `setVoice()`.
- **Post-delay**: two more settable-delay taps (`DelayLine`, 0-1.365s),
  fed from the reverb's own wet output (not the voices), blended back in
  via `PstDlyMix` — matches the PCM81's "delays after the reverb effect."
- **Glide (GldResp/GldRange)**: both the 4 Voices' delay times and the
  post-delay taps use `dsp/GlideParameter.h` — when a delay-time target
  changes by no more than the settable Range (seconds), it glides
  smoothly to the new value at a speed set by Response (0..100, ~60s at
  0 down to ~5ms at 100); larger changes jump instantly instead. This is
  a direct port of the manual's documented behavior (not a
  reconstruction) and is what makes live delay-time changes click-free,
  and enables tape-echo-style pitch-bend glides. Calling `setVoice()`/
  `setPostDelaySeconds()` repeatedly with the same value is a no-op, so
  it's safe for a plugin to call every block with the current parameter
  value without retriggering the glide.
- **Clear**: instantly flushes the 4 Voice delay lines on the rising edge
  and gates their input silent while held — "one tap removal of all old
  audio," typically patched to a footswitch on the original hardware.
- **Rvb Width / FX Width** *(original reconstruction)*: both use
  `dsp/StereoRotate.h`'s mid/side rotation over a continuously-variable
  -360..360 degree range (0 = pass-through, ±180 = fully phase-inverted,
  periodic every 360°) — matching the PCM81 Width controls' described
  *character* (a single knob sweeping through narrow/normal/wide/
  surround/inverted stereo images) rather than a verified match to its
  exact labeled value table. Rvb Width is scoped to just the reverb's own
  output (before FX Mix combines it with the Voices path); FX Width
  applies to the combined signal after post-delay.
- **FX Mix**: balances the Voices path against the reverb path before
  post-delay/width/hi-cut/adjust are applied to their sum.
- **Hi-Cut / FX Adjust**: a final `OnePoleLowpass` (real Hz cutoff) and a
  dB output-gain multiply, applied after Width.

## Knob / footswitch mapping (Polyend Endless)

The hardware patch only exposes 3 knobs; everything else uses the
defaults set in `ConcertHallAlgorithm::prepare()`. The JUCE plugin exposes
the full parameter set (~54 params, including Voice/Post-Delay Glide and
Clear) via `AudioProcessorValueTreeState` for deeper editing.

| Control | Range | Effect |
|---|---|---|
| Left knob | 0.3s .. 8s | Decay time (Mid Rt / master RT60) |
| Mid knob | 0 .. 1 | Damping (Rt HC: 0 = bright/20kHz, 1 = dark/1kHz) |
| Right knob | 0 .. 1 | Dry/wet mix (the Graph's top-level Mix) |
| Footswitch press | toggle | Bypass (LED: dim white) |
| Footswitch hold | toggle | Freeze (LED: light yellow) |

Normal/active LED color is dim cobalt.

## Known simplifications / remaining gaps

- **Definition, Depth, Rvb Width, and FX Width are original
  reconstructions** of the source material's *described* behavior, not
  verified matches to Lexicon's exact implementation — see above for what
  each actually does here.
- **No master Voice controls** (simultaneous scaling of all 4 voices'
  level/delay/pan) — each voice is set independently via `setVoice()`.
- Delay lengths (diffuser, tank, pre-delay, early-reflection, voice-
  diffusion, Voice, and post-delay capacities) are fixed sample counts
  tuned for 48kHz and reused as-is by the JUCE plugin at other sample
  rates — actual delay *time* will drift slightly off-48kHz-tuning at
  other rates.
- Only one of the five reverb cores (Concert Hall) is wired up end-to-end.
  Plate, Chamber, Inverse, and Infinite each have their own distinct
  character/parameter per `docs/lexicon-pcm81-reference.md`, and can
  reuse every Primitive/Component here, including `Comb`/`Voice`/
  `StereoRotate`, plus their own `EkoDly`/`EkoFbk` pre-echo stage (a plain
  `Comb`), which Concert Hall's diagram doesn't have.
