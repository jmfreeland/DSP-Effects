# Lexicon PCM81 — primary source reference

Source: Lexicon PCM81 User Guide, Rev 1 (Lexicon Part # 070-12614). These
are architecture and parameter facts taken directly from the manual, kept
here as shared grounding for every "PCM81-style" patch in this repo (not
just Hall) — the actual PEL/DSP code is proprietary and not reproduced
here; this is the *interface* Lexicon exposed, which tells us the
structural design of the algorithm behind it.

Four scanned excerpts live in the repo under `docs/references/`:
- `lexicon-pcm81-user-guide-rev1.pdf` - chapters 2-3 through manual page
  3-19: the Reverb Shell + all 5 reverb cores' block diagrams and edit
  matrices (p.3-2 to 3-7, the source for the "five reverb cores" section
  below), the block diagrams and edit matrices for all 5 six-voice
  algorithms (p.3-8 to 3-15, the source for the "six-voice algorithms"
  section below), and the start of the alphabetical parameter glossary
  (Chorus, Controls, and Delay Time row entries only - p.3-16 to 3-19).
- `lexicon-pcm81-user-guide-rev1-6voice-pitch.pdf` - picks up exactly
  where that leaves off (p.3-20 onward): the rest of the alphabetical
  parameter glossary rows that also cover the 6-Voice algorithms
  (Feedback/Cross-Feedback, Filters, Glide FX, Levels, Pitch, Resonance),
  the full Pitch-class section with its own block diagrams for all 6
  Dual-FX-style algorithms plus the Submixer routing system, and the
  Pitch Correct algorithm.
- `lexicon-pcm81-user-guide-rev1-presets-midi.pdf` - the chapter 4
  factory-preset catalog (300 presets) and the start of chapter 5 MIDI
  operation; useful for cross-checking naming/behavior claims but not
  load-bearing for any block's own design.
- `lexicon-pcm81-user-guide-rev1-midi-troubleshooting-specs.pdf` - the
  rest of chapter 5 (MIDI controller assignment, Program Change/SysEx/
  Dynamic MIDI, the MIDI Implementation Chart), chapter 6 Troubleshooting,
  and chapter 7 Specifications - completing the manual end to end.
  Entirely MIDI/hardware/troubleshooting content; confirms rather than
  changes this repo's existing "no MIDI input pathway" scoping, and
  surfaces nothing actionable for any Block's own design.

Together these confirmed our overall topology closely (the shared signal
path below, and each core's unique-control table) and turned up two
corrections, now fixed: Inverse's Low Slope/Mid Slope sign convention was
backwards (see `docs/lexicon-pcm81-inverse.md`), and Inverse has a real
`Shape` control - initially flagged as an unknown gap, then resolved once
a later excerpt's "Shape, Spread" glossary entry (p.3-35) explained it's
the same Chamber/Infinite swell mechanism with Spread fixed - now
implemented (see `docs/lexicon-pcm81-inverse.md`). The rest of the manual
(factory presets, MIDI, troubleshooting, specs) didn't surface anything
else actionable against the five reverb cores or the 6-Voice/Pitch
algorithms now described below.

## The 17 algorithms

Three classes, each wrapping one of 5 reverb "cores" with a class-specific
front end:

- **4-Voice** (reverb + a general-purpose 4-voice delay "toolbox," the
  "Reverb Shell"): **Concert Hall**, **Plate**, **Chamber**, **Inverse**,
  **Infinite**.
- **6-Voice** (reverb + a specialized 6-voice delay-voice effect,
  structurally the same "N delay voices summed into a reverb" shape as
  the 4-Voice Reverb Shell just scaled to six voices, plus one
  class-specific addition): **Glide>Hall**, **Chorus+Rvb**,
  **M-Band+Rvb**, **Res1>Plate**, **Res2>Plate**. See "The five
  six-voice algorithms" below.
- **Pitch** (a "Dual FX" submixer routing one of 3 reverbs against one of
  3 pitch-shift blocks, freely series/parallel via a dedicated Submixer):
  **Quad>Hall**, **Dual-Chmb**, **Dual-Plt**, **Dual-Inv**,
  **Stereo-Chmb**, **VSO-Chmb**, plus **Pitch Correct** (monophonic vocal
  pitch correction, no Submixer). See "The Pitch algorithms" below.

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
| Inverse | Envelope slope is controllable — decay, gate, or rise | **Duration** (time before cutoff), **Low Slope**/**Mid Slope** (envelope shape per band, replacing Low Rt/Mid Rt; negative=natural decay tail, 0=gate, positive=inverse/rise, confirmed by the manual text), **Shape** (same swell mechanism as Chamber/Infinite, but with Spread fixed rather than user-settable, per the manual's "Shape, Spread" glossary entry) |
| Infinite | Chamber + a freeze switch; tail rings forever, reverb input ramps off | **Infinite** (on/off) — validates this repo's existing footswitch-hold "freeze" design |

## The five six-voice algorithms

Per the manual's own "The 6-Voice Algorithms" intro (p.3-8): each one
pairs a specific 6-voice stereo effect with one of the reverb cores
above, in place of the general-purpose 4-Voice Reverb Shell. All five
share this structure: "Voices 1-3 are connected to input audio panned to
the left. Voices 4-6 are connected to input audio panned to the right.
(Use the InPan L and InPan R controls at Control mode 0.2 to pan input
audio.) Each voice has independent delay time, panning and level
controls, in addition to other parameters specific to the particular
effect." **Glide>Hall, Res1>Plate, and Res2>Plate run their 6-voice
effect in *series* with the reverb** (FX Mix sets dry-6-voice vs.
reverberated balance); **M-Band+Rvb and Chorus+Rvb run theirs in
*parallel*** (FX Mix sets the balance of the two, both fed from the same
input).

| Algorithm | Reverb core | 6-voice effect | Topology |
|---|---|---|---|
| Glide>Hall | Concert Hall | Stereo pair of 2-tap gliding delays feeding 6 delay voices (own level/feedback/delay/cross-feedback/pan each) | Series |
| Chorus+Rvb | Plate | 6 independently adjustable chorus voices (own depth/rate/level/delay/feedback/pan), sounds like "a rack of six digital delay boxes" | Parallel |
| M-Band+Rvb | Chamber | 6 voices, each with own level/delay/HiCut+LoCut filter/feedback/pan; the diffuser sits *inside* the multiband voices' own feedback paths (not just the reverb's), so filtered echoes can grow more diffuse each repeat | Parallel |
| Res1>Plate | Plate | 6 resonator voices excited by input transients, pitches assigned **chromatically, round-robin** (MIDI note numbers re-tune resonators to the last 6 notes played - a sustain-pedal-like effect) | Series |
| Res2>Plate | Plate | Same 6-resonator mechanism as Res1>Plate, but pitches are assigned **diatonically** - harmonized to a chosen key/scale/root | Series |

`Diffusion` (Rvb Design row) is explicitly shared between the reverb and
the 6-voice effect in Chorus+Rvb and M-Band+Rvb (the manual calls this
out on both algorithms' pages), rather than each having its own copy.

Edit-matrix row layout is consistent with the 4-Voice pattern (Row 0
Controls, Row 1 Rvb Time, Row 2 Rvb Design borrowed unchanged from the
paired core, then rows for the 6-voice effect's own Levels/Delay
Time/Feedback/Panning, then Modulation/Patches last) with one dedicated
extra row per algorithm for its distinguishing mechanism: Glide>Hall's
own "Glide FX" row (Glide Level, tap position, feedback%, and
cross-feedback L/R controls for the front-end 2-tap gliding delay -
exact cell labels are OCR-uncertain in the scan, so the body text above
is the source of truth, not the matrix grid), Chorus+Rvb's "Chorus" row
(per-voice Depth/Rate plus master Depth/Rate, both 0-200% scalers - see
Parameter glossary below), M-Band+Rvb's per-voice filters (no dedicated
glossary entry captured yet - see gap note above), and Res1>Plate /
Res2>Plate's "Pitch" row (`Assign`, `Tuning`, `Active`, `Unison` -
Res2>Plate additionally needs key/scale/root, likely under `Assign`'s
subparameters, not yet confirmed).

## Parameter glossary (the parts worth carrying forward)

**Chorus row** (Chorus+Rvb only):
- `MstDepth`/`MstRate` — two master scalers (0-200%) that simultaneously
  scale Depth and Rate for all 6 chorus voices at once, without altering
  each voice's own underlying setting - the same "master proportionally
  scales all lines" idea as the H3000 Swept Combs' Master row.
- Per-voice `Depth` (0-500ms, single-ms increments) and `Rate` (0Hz/Off,
  or one of 100 steps from 0.01-3.50Hz) — Depth is the time range the
  voice's delay sweeps across, Rate is how fast it sweeps that range.
  Manual guidance: Depths of 10-30ms + Rates up to 0.50Hz = subtle
  chorusing/multivoicing; Depths of hundreds of ms + higher Rates = a
  wide range of pitch-shifting effects (i.e. this Depth/Rate pair is
  functionally the same "LFO-modulated delay tap" mechanism already used
  for `StringModeller`'s Chorus stage, just with 6 independent voices
  instead of 1).

**Delay Time row** (universal, shared by every algorithm, all 4/6-Voice
Row 4 or 5 depending on the algorithm):
- `Master` — 0-200% scaler on all voices' delay times simultaneously
  (0% = all delays instantly become 0, 200% = all doubled).
- `GldResp`/`GldRange` — already implemented as `dsp::GlideParameter`.
  `GldResp` (0-100, default 50) is glide *response* - how quickly a
  changed delay time arrives at its new value (0 = ~a minute, ultra-slow/
  no audible pitch shift; 100 = near-instant, chirps). `GldRange`
  (0-1.365s) is glide *range* - the size of change that gets glided at
  all; changes larger than GldRange jump instantly instead of gliding.
- `Clear` — On/Off, instantly empties all delay-voice audio (e.g. patched
  to a footswitch for one-tap delay-trail removal); while on, no new
  audio passes through any voice delayed above 1ms.

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

## The Pitch algorithms

The PCM81 contains 7 Pitch algorithms combining "uncompromised Lexicon
reverb" with pitch shifting. Quad>Hall is a fixed-routing 4-voice
pitch shifter in series with Concert Hall (own Mix control, no
Submixer). The other six all use a genuinely different mechanism, the
**Submixer** (a dedicated parameter row present in every Dual-FX
algorithm): two independent effect blocks, a **Stereo Reverb** block
(one of Chamber/Plate/Inverse, fixed per algorithm) and a **Pitch FX**
block (one of Dual Shifter/Stereo Shifter/VSO Shifter, fixed per
algorithm), each with its own In Level/In Width/Out Level/Out
Width/HiCut/LoCut/Mix, freely arranged via three Submixer controls:
- `Sends` (0-300): how the two panned main inputs feed the two
  blocks' stereo inputs - Stereo/L=Rvb,R=FX/Mono (L+R to both)/
  L=FX,R=Rvb/Stereo (2nd stereo option, reversed channel order from
  the first).
- `Returns` (0-300): the mirror of Sends, for how the two blocks'
  stereo outputs feed the two main outputs.
- `Routing` (0-400, **takes precedence over Sends/Returns** - e.g. at
  "Rvb into FX" no signal is sent directly to FX's inputs and Rvb's
  outputs don't reach the main outs directly): Parallel / Rvb-into-FX
  (series 1) / FX-into-Rvb (series 2, reverse series) / Parallel
  again (400 is a second parallel option matching 0/200, per the
  manual's own redundant labeling).

  This one control genuinely reconfigures the signal graph at
  runtime (not just crossfades a fixed topology) - straightforward to
  implement as an explicit `switch` over 5-6 named configurations
  (Stereo Series 1/2, Mono-in Series 1/2, Stereo Parallel, Mono-in
  Parallel, Dual Mono-in Stereo-out, Dual Mono-in Mono-out - the
  manual's own "Useful Configurations" diagrams) rather than a
  continuous DSP-level crossfade, since Sends/Returns/Routing are each
  themselves discrete-valued (five or so labeled positions on a
  0-300/0-400 range) not continuous controls.
- **Dual-Chmb** / **Dual-Plt** / **Dual-Inv** = Submixer + a "Dual
  Shifter": two independent voices, each Delay(1 sample min)->
  Pitch(cents,±3 octaves,1-cent res)->Level->Pan, each with its own
  Fbk and X-Fbk back into *both* delay inputs - i.e. mechanistically
  identical to the H3000's own `PitchShiftVoice` pair pattern (Dual
  Shift/Layered Shift), reusable unchanged - feeding Chamber / Plate /
  Inverse respectively.
- **Stereo-Chmb** = Submixer + a "Stereo Shifter": *one* shift amount
  applied sample-synchronously to both channels ("a true stereo pitch
  shifter... maintain[s] the stereo image of source material") -
  mechanistically a single shared cents value driving two
  `PitchShifter`s in lockstep rather than two independent voices -
  feeding Chamber.
- **VSO-Chmb** = the same Stereo Shifter, plus one added `Varispeed`
  parameter (+55.00% to -35.00%, 0.01% steps) that directly computes
  the compensating pitch shift for a known playback-speed change
  (e.g. +20% speed => -386 cents to restore original pitch) - a
  closed-form mapping from a percentage to a cents value, not a new
  DSP mechanism - feeding Chamber.
- **Pitch Correct** = no Submixer; a fixed series chain: `Pitch
  Correct` block -> Chamber -> FX Width/HiCut/LoCut/Adjust, FX Mix
  fixed near 0% ("most applications require only pitch processing").
  Designed for monophonic vocal sources. `Detect` (Input/Fixed/MIDI)
  picks the pitch-detection source; for this repo (no MIDI input
  pathway - see the H3000 notes on the same constraint) only `Input`
  is in scope. `Correction` (0-100%) blends toward the nearest in-key
  pitch; `Low Pitch`/`High Pitch` bracket the detector's tracked
  range (also sets the shifter's inherent latency, same tradeoff as
  the Pitch-class shift voices' own Low Pitch control); `Tuning`
  (410-470Hz A reference); `Splice` (crossfade ms); `Shift Cents` +
  `Shift Semitones` (an *additional* fixed transpose on top of
  correction, additive to ±2 octaves); `GldResp`/`Tracking`
  (Fastest/Fast/Moderate/Slow/Hold - detector responsiveness). This
  is `dsp::PitchDetector` (autocorrelation pitch tracking) +
  `dsp::DiatonicScale.h`'s `nearestScaleDegreeIndex` (nearest in-key
  pitch) + `dsp::PitchShifter` (the correction itself) - the same
  three primitives already proven together in the H3000's Diatonic
  Shift, just monophonic-single-voice instead of harmonizing voices,
  and correcting *toward* the input's own nearest scale degree rather
  than transposing by a chosen interval.

**Rvb/FX block controls** (Submixer row, shared by all 6 Dual-FX
algorithms): `RvbMix`/`FXMix` (independent wet/dry per block),
`RvbInLvl`/`FXInLvl`, `RvbInW`/`FXInW` (independent stereo width *at
the input* to each block - distinct from the block's own internal
width control), `RvbHiCut`/`FXHiCut`, `RvbLoCut`/`FXLoCut` (independent
6dB/octave stereo filters on each block's output), `RvbOutLvl`/
`FXOutLvl`, `RvbOutW`/`FXOutW`.

## Numbers worth reusing in our engines

- Native sample rates: 44.1kHz and 48kHz (word size 20-24 bit internal).
- A/D: >102dB S/N, <0.003% THD, 24-sample latency. D/A: >98dB S/N, <0.005% THD, 50-sample latency.
- Max reverb-related delay in the 4-Voice/6-Voice algorithms: up to 1.365s (individual voices), pre-delay up to 930ms, early reflections/echo up to 1.2s (800ms for Inverse specifically, per the manual - not yet reflected in this repo's uniform 1.2s constant, see `docs/lexicon-pcm81-inverse.md`).
- Pitch range context (not PCM81-specific, general pitch-shifting theory the manual states directly): raising pitch = compress + duplicate a segment; lowering = expand + remove a segment; splice points are the artifact source; large shifts, low-frequency content, and dense transients all increase splice audibility.

## Gap vs. this repo's current implementation

All five reverb cores are built (`docs/lexicon-pcm81-hall.md`,
`-plate.md`, `-chamber.md`, `-infinite.md`, `-inverse.md` for each one's
specifics and known simplifications). One architectural note worth
tracking: the manual's own tables show `Definition`, `Depth`, and
`Chorus` as Concert Hall-*exclusive* (Plate/Chamber/Infinite/Inverse's
Rvb Design rows don't list them), but this repo's code keeps all three
as generic mechanisms on the shared `ReverbCore` base for implementation
simplicity - harmlessly inherited-but-unexposed by the other four cores'
Graphs/Patches/plugins, not a functional bug, just not matching the real
hardware's per-algorithm control surface exactly. Revisit if that
distinction ever matters (e.g. exposing them to Plate/Chamber would be a
one-line change to those Graphs, should someone want to experiment).

All 5 six-voice algorithms are now built (`docs/lexicon-pcm81-glide-hall.md`,
`-chorus-rvb.md`, `-mband-rvb.md`, `-res1-plate.md`, `-res2-plate.md` for
each one's specifics and known simplifications) - M-Band+Rvb's per-voice
filter row and Res1/Res2>Plate's Pitch/resonance row each needed a
documented interpretation call the same way Inverse's Shape/Spread gap
was originally handled, since the scanned excerpt's OCR of those specific
rows wasn't reliable enough to trust over the clearly-legible body text.
None of the 7 Pitch-class algorithms (Quad>Hall, Dual-Chmb, Dual-Plt,
Dual-Inv, Stereo-Chmb, VSO-Chmb, Pitch Correct) are built yet - that's
the next phase of work (see "The Pitch algorithms" above for what the
manual specifies for each). MIDI-sourced pitch detection (Pitch
Correct's `Detect: MIDI` option) is out of scope for the same reason
every other MIDI-only feature in this archive is: no consumer here
implements MIDI input.
