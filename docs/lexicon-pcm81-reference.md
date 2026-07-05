# Lexicon PCM81 — primary source reference

Source: Lexicon PCM81 User Guide, Rev 1 (Lexicon Part # 070-12614). These
are architecture and parameter facts taken directly from the manual, kept
here as shared grounding for every "PCM81-style" patch in this repo (not
just Hall) — the actual PEL/DSP code is proprietary and not reproduced
here; this is the *interface* Lexicon exposed, which tells us the
structural design of the algorithm behind it.

## The 17 algorithms

Three classes, each wrapping one of 5 reverb "cores" with a class-specific
front end:

- **4-Voice** (reverb + a general-purpose 4-voice delay "toolbox," the
  "Reverb Shell"): **Concert Hall**, **Plate**, **Chamber**, **Inverse**,
  **Infinite**.
- **6-Voice** (reverb + a specialized 6-voice effect): **Glide>Hall**
  (gliding delay + Concert Hall, series), **Chorus+Rvb** (6-voice chorus +
  Plate, parallel), **M-Band+Rvb** (6-voice multiband EQ'd delay +
  Chamber, parallel, diffuser inside the multiband feedback loops),
  **Res1>Plate** (6 chromatic round-robin resonators + Plate),
  **Res2>Plate** (6 diatonically-harmonized resonators + Plate).
- **Pitch** (a "Dual FX" submixer routing one of 3 reverbs against one of
  3 pitch-shift blocks, freely series/parallel): **Quad>Hall**,
  **Dual-Chmb**, **Dual-Plt**, **Dual-Inv**, **Stereo-Chmb**, **VSO-Chmb**,
  plus **Pitch Correct** (monophonic vocal pitch correction).

## The five reverb cores

Common signal path (from the manual's block diagrams):

```
input -> Diffusion -> PreDelay -> [ Low Rt | Mid Rt | Crossover ]
                                   [   Rt HC | Size  | <algo-specific> ]
                                   [   Spin  | <algo-specific 2> | Link ]
                                -> RvbOut
early reflections (parallel):  RefDly L/R -> RefLvl L/R
pre-echo w/ feedback (Plate/Chamber/Infinite only): EkoDly L/R -> EkoFbk L/R
```

Per-algorithm character and unique third/fourth slot:

| Algorithm | Character | Unique control(s) |
|---|---|---|
| Concert Hall | Clean, stays behind the source; low initial density building gradually | **Definition** (echo-density buildup rate in the *latter* part of decay), **Depth** (front-to-rear listener perspective), **Chorus** (randomizes delay times to kill metallic ringing) |
| Plate | High initial diffusion, bright; good on percussion | **Attack** (sharpness of initial response, first 50ms only) |
| Chamber | Even, "dimensionless," little color change over decay; good on vocals | **Shape** (envelope contour) + **Spread** (sustain) |
| Inverse | Envelope slope is controllable — decay, gate, or rise | **Duration** (time before cutoff), **Low Slope**/**Mid Slope** (envelope shape per band, replacing Low Rt/Mid Rt) |
| Infinite | Chamber + a freeze switch; tail rings forever, reverb input ramps off | **Infinite** (on/off) — validates this repo's existing footswitch-hold "freeze" design |

## Parameter glossary (the parts worth carrying forward)

**Rvb Design row** (structural):
- `Size` — rate of diffusion build-up *after* the initial period; ≈ largest dimension of the space in meters.
- `Diffusion` — initial echo density build-up (universal to all algorithms). High = denser/smeared (good for percussion); low/moderate = clearer vocals/piano.
- `Spin` — continuously alters the tail's timbre so it doesn't sit static; typical useful range ~10-50.
- `Link` — ties Mid Rt (and Shape/Spread) to scale together with Size.
- `Rvb Width`, `Rvb In/Out` — reverb-only stereo width and input/output trim (Rvb In: 0 to -85dB; Rvb Out: 0 to -24dB, doesn't affect early reflections).

**Rvb Time row** (time-based):
- `Mid Rt` — the master reverb time (RT60).
- `Low Rt` — **multiplier** of Mid Rt for low-frequency decay (e.g. Low Rt=2x, Mid Rt=2s -> low-freq RT60=4s). Recommend ≤1.5x for natural halls.
- `Crossover` — frequency where Mid Rt hands off to Low Rt; set ~2 octaves above the frequency you want to boost.
- `Rt HC` — a single 6dB/octave (one-pole) lowpass on the reverberated signal only (not the reflections) — this is the "damping" control.
- `Pre Delay` — gap between input and reverb onset, up to 930ms. Distinct from early reflections; natural spaces are better emulated via Spread than a big PreDelay.
- `RefLvl L/R` / `RefDly L/R` — a single pair of early-reflection taps, up to 1.2s.
- `EkoFbk L/R` / `EkoDly L/R` — Plate/Chamber/Infinite only: a *recirculating* pre-echo (has feedback) ahead of the main tail, up to 1.2s.

**Controls row** (universal, Row 0 of every algorithm):
- `Mix` (dry/wet), `FX Adjust` (wet output trim, +12 to -73dB), `FX Mix` (balance of reverb vs. the class-specific voice/chorus/multiband effect), `FX Width` (mono/stereo/surround imaging, -360..+360 with 0/±360=mono, ±45=stereo, ±90=L-R/R-L surround), `High Cut` (global lowpass), `InLvl`/`InPan` L&R (dry input level/pan into the effect).

## Numbers worth reusing in our engines

- Native sample rates: 44.1kHz and 48kHz (word size 20-24 bit internal).
- A/D: >102dB S/N, <0.003% THD, 24-sample latency. D/A: >98dB S/N, <0.005% THD, 50-sample latency.
- Max reverb-related delay in the 4-Voice/6-Voice algorithms: up to 1.365s (individual voices), pre-delay up to 930ms, early reflections/echo up to 1.2s.
- Pitch range context (not PCM81-specific, general pitch-shifting theory the manual states directly): raising pitch = compress + duplicate a segment; lowering = expand + remove a segment; splice points are the artifact source; large shifts, low-frequency content, and dense transients all increase splice audibility.

## Gap vs. this repo's current Hall implementation

See `docs/lexicon-pcm81-hall.md` for how `dsp/algorithms/LexiconHall.h`
compares to this reference and what a Stage 2 pass would change.
