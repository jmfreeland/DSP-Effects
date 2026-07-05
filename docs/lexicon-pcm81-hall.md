# Lexicon PCM81-style Concert Hall Reverb

`dsp/include/dsp/algorithms/ConcertHall.h`, wired into the Polyend Endless
as `patches/lexicon/hall/`. Composed entirely from the shared `dsp/`
primitives (`DiffuserChain`, `Crossover`, `Comb` is unused here but proven
separately, `LinearRamp`, `rt60ToGain`, `DelayLine`, `OnePoleLowpass`,
`LFO`, `FeedbackMatrix`'s Householder mix) — see
`docs/lexicon-pcm81-reference.md` for the primary-source parameter
definitions this was built from.

## Scope and honesty

This is an **original implementation inspired by** the general character of
Lexicon's algorithmic reverbs — dense, prime-length diffusion feeding a
mutually-coupled tank, gentle internal modulation, dual-band damped
feedback — not a disassembly or reverse-engineering of the PCM81's
proprietary DSP code (which isn't public). Treat every
"PCM81-style"/"Lexicon-style" label in this repo the same way: topology
and character, not a bit-exact clone.

## Topology

```
L,R -> mono sum -> PreDelay -+-> DiffuserChain<4> -> diffused
                              +-> early-reflection tap (RefDly/RefLvl) -> wet out

  8-line Householder FDN tank, per line i:
    tapped[i]   = delay_i.readLinear(length_i * sizeScale +/- Spin/Chorus wobble)
    damped[i]   = onePoleLowpass_i(tapped[i])              // Rt HC
    low, high   = crossover_i(damped[i])                   // split at Crossover Hz
    decayed[i]  = low*lowGain_i + high*midGain_i            // Low Rt / Mid Rt
  householderMix(decayed)                                  // lossless energy-preserving mix
  for each line i:
    delay_i.write(diffused * sign_i * 0.5 + decayed[i])

  wetLeft, wetRight <- signed sums of tapped[] + earlyTap*earlyReflectionLevel
  wet *= sizeMuteEnvelope (briefly mutes/fades-in on a Size change)
  output = lerp(dry, wet, mix)
```

- **PreDelay**: a plain `DelayLine`, 0-930ms, gap before the diffuser/tank see any signal.
- **Diffusion**: `DiffuserChain<4>`, prime lengths 211/431/751/1091 samples
  (~4.4/9.0/15.7/22.7ms @ 48kHz). `setDiffusion(0..1)` maps to the shared
  allpass coefficient (0-0.75) across all four stages.
- **Early reflections**: a single `DelayLine` tap (`RefDly`/`RefLvl`,
  0-1.2s) mixed straight into the wet output, parallel to the tank.
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
  lengths.
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

## Knob / footswitch mapping (Polyend Endless)

The hardware patch only exposes 3 knobs; everything else uses the
defaults set in `ConcertHall::prepare()`. The JUCE plugin exposes the
engine's full parameter set (Decay, Low Ratio, Crossover, Damping,
Diffusion, Size, Pre Delay, Early Reflections, Spin, Chorus, Mix, Freeze)
via `AudioProcessorValueTreeState` for deeper editing.

| Control | Range | Effect |
|---|---|---|
| Left knob | 0.3s .. 8s | Decay time (Mid Rt / master RT60) |
| Mid knob | 0 .. 1 | Damping (Rt HC: 0 = bright/20kHz, 1 = dark/1kHz) |
| Right knob | 0 .. 1 | Dry/wet mix |
| Footswitch press | toggle | Bypass (LED: dim white) |
| Footswitch hold | toggle | Freeze (LED: light yellow) |

Normal/active LED color is dim cobalt.

## Known simplifications / remaining gaps

- **`Definition` and `Depth` aren't implemented.** Real Concert Hall has
  `Definition` (echo-density buildup rate in the *latter* part of decay,
  as distinct from `Diffusion`'s initial buildup) and `Depth`
  (front-to-rear listener perspective). Both would need a time-varying
  diffusion/level contour rather than a fixed coefficient; left as an
  open gap rather than approximated.
- **`Link` isn't implemented** (ties Mid Rt/Spread scaling to Size) — more
  relevant to Chamber/Infinite's Shape+Spread than to Concert Hall.
- **No `FX Width`** (the -360..+360 mono/stereo/surround/phase-invert
  imaging control common to every algorithm) — only a fixed
  alternating-sign stereo tap sum.
- Early reflections are a single mono tap shared by both channels, not
  independent `RefDly L`/`RefDly R`.
- Delay lengths (diffuser, tank, pre-delay, early-reflection capacities)
  are fixed sample counts tuned for 48kHz and reused as-is by the JUCE
  plugin at other sample rates — actual delay *time* will drift slightly
  off-48kHz-tuning at other rates.
- Only one of the five reverb cores (Concert Hall) is wired up end-to-end.
  Plate, Chamber, Inverse, and Infinite each have their own distinct
  character/parameter per `docs/lexicon-pcm81-reference.md`, and can
  reuse every primitive here plus the still-unexercised `Comb` (for their
  `EkoDly`/`EkoFbk` pre-echo stage, which Concert Hall's diagram doesn't
  have).
