# Lexicon PCM81-style Hall Reverb

`dsp/include/dsp/algorithms/LexiconHall.h`, wired into the Polyend Endless
as `patches/lexicon/hall/`.

## Scope and honesty

This is an **original implementation inspired by** the general character of
Lexicon's algorithmic reverbs — dense, prime-length diffusion feeding a
mutually-coupled tank, gentle internal modulation, damped feedback — not a
disassembly or reverse-engineering of the PCM81's proprietary DSP code
(which isn't public). Treat every "PCM81-style"/"Lexicon-style" label in
this repo the same way: topology and character, not a bit-exact clone.

## Topology

```
L,R -> mono sum -> [4x series Allpass diffuser] -> diffused
                                                        |
                    +-----------------------------------+
                    v
  8-line Householder FDN tank:
    for each line i:
      tapped[i]  = delay_i.read(length_i, +/- LFO wobble on 4 of 8 lines)
      damped[i]  = onePoleLowpass_i(tapped[i])
    householderMix(damped)                  // lossless energy-preserving mix
    for each line i:
      delay_i.write(diffused * sign_i * 0.5 + damped[i] * feedbackGain_i)

  wetLeft, wetRight <- signed sums of tapped[] (decorrelated stereo taps)
  output = lerp(dry, wet, mix)
```

- **Diffusers**: 4 series Schroeder allpasses, prime lengths 211/431/751/1091
  samples (~4.4/9.0/15.7/22.7ms @ 48kHz), coefficient 0.6. Turns a transient
  into a dense cluster of echoes before it reaches the tank.
- **Tank**: 8 delay lines, prime lengths 977..4357 samples (~20ms to ~91ms),
  mutually coprime to avoid periodic/metallic ringing.
- **Feedback gain per line** is solved from the user's decay-time (RT60)
  parameter so all eight lines — despite different lengths — decay to
  -60dB at the same time: `gain_i = 10^(-3 * length_i/sampleRate / RT60)`.
- **Damping**: one-pole lowpass in each line's feedback path models the
  progressive high-frequency loss of a real space/plate, i.e. the tail
  gets darker as it decays.
- **Modulation**: 4 of the 8 lines get a slow (~0.08-0.19Hz), mutually
  detuned, small (+/-3 sample) delay wobble via fractional-interpolated
  reads. This is what keeps a static FDN from settling into an audibly
  discrete, metallic set of resonances — a defining Lexicon trait.
- **Mixing matrix**: Householder reflection (`I - (2/N)*J`) — lossless,
  so tank energy is redistributed, not amplified or lost, on every pass.

## Knob / footswitch mapping (Polyend Endless)

| Control | Range | Effect |
|---|---|---|
| Left knob | 0.3s .. 8s | Decay time (RT60) |
| Mid knob | 0 .. 1 | Damping (0 = bright, 1 = dark) |
| Right knob | 0 .. 1 | Dry/wet mix |
| Footswitch press | toggle | Bypass (LED: dim white) |
| Footswitch hold | toggle | Freeze — feedback gain forced to ~0.9999 and dry input excluded from the tank, so whatever's currently ringing sustains indefinitely (LED: light yellow) |

Normal/active LED color is dim cobalt.

## Known simplifications / future refinement (Stage 2 candidates)

Now grounded against the real interface (see
`docs/lexicon-pcm81-reference.md`), the gaps between this Stage 1 engine
and Lexicon's actual Concert Hall design are:

- **Decay is single-band, not dual-band.** Real Concert Hall has
  independent `Mid Rt` (master RT60) and `Low Rt` (a *multiplier* of Mid
  Rt, recommended ≤1.5x) either side of a `Crossover` frequency, plus a
  separate `Rt HC` one-pole high-cut. This engine only has one damping
  filter doing the job of `Rt HC`; there's no low-frequency-decay
  multiplier or crossover yet.
- **No Pre Delay, no early reflections.** Real Concert Hall has `Pre
  Delay` (gap before reverb onset, up to 930ms) and a distinct pair of
  early-reflection taps (`RefDly`/`RefLvl`) parallel to the diffuse tank.
  This engine only has the diffuser-into-tank path.
- **`Size` and `Diffusion` are conflated.** Lexicon treats `Diffusion`
  (initial echo density) and `Size` (the *rate* diffusion keeps building
  after that initial period, correlated to room size in meters) as two
  separate controls; this engine has one fixed diffuser chain and no size
  control at all (delay lengths are fixed).
- **`Chorus`/`Spin`/`Definition`/`Depth` aren't separated.** This engine's
  "4 of 8 lines wobble" is a rough stand-in for what Lexicon splits into
  `Spin` (continuous timbre movement, tail-wide) and `Chorus` (explicitly:
  randomizes delay times to kill metallic ringing — Concert Hall/Glide>Hall
  only). `Definition` (buildup rate late in the decay) and `Depth`
  (front/rear perspective) have no analog here yet.
- Delay lengths are fixed sample counts, tuned for 48kHz (the Endless's
  native rate) and reused as-is by the JUCE plugin at other sample rates —
  actual delay *time* will drift slightly off-48kHz-tuning at other rates.
- Stereo decorrelation is a simple alternating-sign tap sum, not a true
  stereo-input/stereo-tank design; input is summed to mono before the tank.
- Only one of the five reverb cores (Concert Hall) exists — Plate,
  Chamber, Inverse, and Infinite each have their own distinct
  character/parameter per `docs/lexicon-pcm81-reference.md` and are
  natural next patches, sharing this file's diffuser/tank/damping
  primitives.
