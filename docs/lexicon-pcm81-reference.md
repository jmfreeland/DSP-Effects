# Lexicon PCM81 — primary source reference

Source: Lexicon PCM81 User Guide, Rev 1 (Lexicon Part # 070-12614). These
are architecture and parameter facts taken directly from the manual, kept
here as shared grounding for every "PCM81-style" patch in this repo (not
just Hall) — the actual PEL/DSP code is proprietary and not reproduced
here; this is the *interface* Lexicon exposed, which tells us the
structural design of the algorithm behind it.

A scanned excerpt (chapters 2-3: Basic Operation, and The Algorithms and
Their Parameters) now lives in the repo at
`docs/references/lexicon-pcm81-user-guide-rev1.pdf`, so specific claims
below can be checked directly rather than taken on trust. This excerpt
runs through manual page 3-19: it covers the Reverb Shell + all 5 reverb
cores' block diagrams and edit matrices (p.3-2 to 3-7, the source for the
"five reverb cores" section below), **and** the block diagrams and edit
matrices for all 5 six-voice algorithms (p.3-8 to 3-15, the source for
the "six-voice algorithms" section below), followed by the start of the
alphabetical parameter glossary (Chorus, Controls, and Delay Time row
entries only - p.3-16 to 3-19). This was originally logged as covering
just the 5 reverb cores; re-reading it further turned up the 6-Voice
diagrams already sitting in the same file. Further excerpts (not yet
added to the repo as files, but reviewed against this codebase) carried
the manual through to its end - the rest of chapter 3's parameter
glossary, the full chapter 4 factory-preset catalog, chapter 5 MIDI
operation, chapter 6 troubleshooting, and chapter 7 specifications.
Together these confirmed our overall topology closely (the shared signal
path below, and each core's unique-control table) and turned up two
corrections, now fixed: Inverse's Low Slope/Mid Slope sign convention was
backwards (see `docs/lexicon-pcm81-inverse.md`), and Inverse has a real
`Shape` control - initially flagged as an unknown gap, then resolved once
a later excerpt's "Shape, Spread" glossary entry (p.3-35) explained it's
the same Chamber/Infinite swell mechanism with Spread fixed - now
implemented (see `docs/lexicon-pcm81-inverse.md`). The rest of the manual
(factory presets, MIDI, troubleshooting, specs) didn't surface anything
else actionable against the five reverb cores; it's mostly relevant to
hardware/MIDI operation outside this repo's scope. **Not yet in hand for
the 6-Voice class**: the rest of the alphabetical glossary past Delay
Time - specifically the Feedback/Cross-Feedback, Filters, Glide FX,
Levels, Modulation, Panning, and Resonance row entries, which would
pin down exact parameter ranges/units for Glide>Hall's glide controls,
M-Band+Rvb's per-voice filters, and Res1>Plate/Res2>Plate's resonator
row. **Not yet in hand at all**: the Pitch-class algorithms' own block
diagrams (Quad>Hall, Dual-Chmb, Dual-Plt, Dual-Inv, Stereo-Chmb,
VSO-Chmb, Pitch Correct) and their Submixer.

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

## Numbers worth reusing in our engines

- Native sample rates: 44.1kHz and 48kHz (word size 20-24 bit internal).
- A/D: >102dB S/N, <0.003% THD, 24-sample latency. D/A: >98dB S/N, <0.005% THD, 50-sample latency.
- Max reverb-related delay in the 4-Voice/6-Voice algorithms: up to 1.365s (individual voices), pre-delay up to 930ms, early reflections/echo up to 1.2s (800ms for Inverse specifically, per the manual - not yet reflected in this repo's uniform 1.2s constant, see `docs/lexicon-pcm81-inverse.md`).
- Pitch range context (not PCM81-specific, general pitch-shifting theory the manual states directly): raising pitch = compress + duplicate a segment; lowering = expand + remove a segment; splice points are the artifact source; large shifts, low-frequency content, and dense transients all increase splice audibility.

## Gap vs. this repo's current implementation

All five reverb cores are now built (`docs/lexicon-pcm81-hall.md`,
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

None of the 6-Voice or Pitch class algorithms are built yet. The 6-Voice
class now has enough primary-source material in hand (block diagrams +
edit matrices for all 5, plus enough of the parameter glossary to pin
down Chorus and Delay Time) to start; M-Band+Rvb's per-voice filter row
and Res1/Res2>Plate's Pitch/resonance row will need interpretation calls
documented per-algorithm the same way Inverse's Shape/Spread gap was
handled, unless a further glossary excerpt turns up first. The Pitch
class has no primary-source block diagrams in hand at all yet - hold
until pages covering it are available.
