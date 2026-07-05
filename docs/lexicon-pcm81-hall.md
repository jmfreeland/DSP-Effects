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

- Delay lengths are fixed sample counts, tuned for 48kHz (the Endless's
  native rate) and reused as-is by the JUCE plugin at other sample rates —
  actual delay *time* will drift slightly off-48kHz-tuning at other rates.
  A size-aware engine would rescale lengths by `sampleRate/48000`.
- Stereo decorrelation is a simple alternating-sign tap sum, not a true
  stereo-input/stereo-tank design; input is summed to mono before the tank.
- No pre-delay, bass-boost/crossover, or multiple algorithm variants
  (Random Hall, Ambience, etc.) yet — Stage 1 is one solid Hall algorithm
  as the reference building block for the rest of the reverb family.
