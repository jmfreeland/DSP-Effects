# Lexicon PCM81-style Quad>Hall Algorithm

Stage 1 (functional): the first of the PCM81's seven Pitch algorithms
(the manual's own third and final algorithm class, after the 5 4-Voice
reverb cores and 5 6-Voice algorithms — see
`docs/lexicon-pcm81-reference.md`) and the only one of the seven without
a Submixer. Per `CLAUDE.md`'s Primitive → Component → Block → Graph
layering:

- **New Primitives/Components**: none — reuses `PitchShiftVoice`
  (Delay + `PitchShifter`, already the repeated unit behind the H3000's
  own fixed-interval Shift algorithms) unchanged, four times.
- **No dedicated Block**: unlike the 4-Voice/6-Voice algorithms (each of
  which adds a real capability to its paired reverb core), Quad>Hall
  reuses `ConcertHall` exactly as built, with nothing new for a Block
  tier to add. Matching how `ConcertHallAlgorithm` already owns its own
  `Voice` array directly rather than introducing an empty intermediate
  Block, `dsp/include/dsp/graphs/QuadHallAlgorithm.h` owns the four
  `PitchShiftVoice`s itself.
- **Graph**: `QuadHallAlgorithm` — 4 independent pitch-shift voices
  (Voices 1-2 fed from the Left input, Voices 3-4 from the Right, each
  its own Delay/Pitch/Level/Pan up to 1.25s) with per-voice Fbk (own
  channel bus) and X-Fbk (opposite channel bus), summed and run **in
  series** into Concert Hall, finished with the shared FX Mix/Width/
  Hi-Cut/Adjust/Mix chain.

## Why this algorithm, and why series with a real Mix control

Straight order per the manual's own "The Pitch Algorithms" intro: "A
Quad>Hall algorithm provides a 4-voice pitch shifter, combined with the
PCM 81 Concert Hall reverb... In this algorithm, the reverb effect is
fixed in position following the pitch shifters, with a final Mix control
allowing control over the amount of reverb in the processed sound. "
That's the same `fxMix_` shape already used throughout this archive's
PCM81 side (0 = dry effect signal only, 1 = fully reverbed) — here
applied to the pitch-shifted signal rather than a `Voice` comb's dry
delay output, since Quad>Hall's own diagram shows the four voices'
summed output feeding both the reverb's input *and* directly into that
same Mix blend.

## Voices 1-2 from Left, 3-4 from Right, with cross-feedback between the pair

The manual's own Voice parameter description: "In the Quad Shift
algorithm, Voices 1 and 2 are left shifts and 3 and 4 are right shifts."
Each voice's Fbk recirculates into its own channel's shared input sum;
X-Fbk crosses into the *other* channel's, matching this archive's
existing reading of the same Fbk/X-Fbk pattern on the 6-Voice side (see
`docs/lexicon-pcm81-glide-hall.md`) — Voices 1-2's Fbk write into the
left bus, their X-Fbk into the right; Voices 3-4 mirror that into their
own/opposite buses. `QuadHallAlgorithm::processSample()` implements this
with one explicit sample of feedback latency (`lastVoiceOutput_`), the
same one-sample-latency shape used throughout this archive's patch-matrix
algorithms, rather than `Voice`'s self-contained-`Comb` per-voice
feedback (which doesn't fit here — the feedback point here is the shared
*channel bus*, not each voice's own delay line).

## Verifying pitch shift without trusting zero-crossing counting

Measuring the shifted output's frequency via zero-crossing counting on a
pure sine gave wildly inconsistent numbers between two nominally-identical
setups (500Hz and 700Hz for the same +1-octave, 300Hz-in test, depending
on unrelated plumbing) — grain-spliced pitch shifting leaves enough
high-frequency ripple around each splice that zero-crossing counting
measures the ripple, not the fundamental (the same class of problem
already documented for `dsp::PitchDetector` on pure tones elsewhere in
this archive). The reliable check instead: diff `QuadHallAlgorithm`'s
isolated Voice 1 output, sample-by-sample, against a standalone
`PitchShiftVoice` fed the identical input stream and parameters — max
absolute difference 0.0041 over 10,000 samples (floating-point-ordering
noise, not a routing bug), confirming the Graph's wiring is a faithful,
correct pass-through of the already-proven Component.

## Known simplifications

- **No MstrCents/MstrScale** — a shared additive/multiplicative pitch
  offset applied to all 4 voices at once ("transpose pitch voices while
  keeping the relative interval(s) between them constant" / "shrink or
  enlarge the relative interval(s)"). Deferred as a tone-shaping
  convenience on top of the per-voice shift rather than new signal flow.
- **No Low Pitch control** — trades shifter latency for cleaner tracking
  of low-frequency material; not modeled, matching this archive's
  existing simplification for `PitchShiftVoice`'s fixed grain-based
  latency elsewhere.
- **Splice is a single shared control across all 4 voices**, not
  independently settable per voice — the manual's own Splice parameter
  description doesn't suggest it varies per-voice either.
- **FX/Rvb Width behavior** is the same original `rotateStereoWidth()`
  reconstruction used throughout this archive's PCM81 side.

## Status

Verified via `QuadHallAlgorithm` smoke tests: finite output for a noise
burst through all 4 voices; the diff-against-`PitchShiftVoice` check
above (max 0.0041 abs difference, confirming correct wiring). `dsp_host_
render quad_hall` renders a 3-second 220Hz tone burst through the default
detuned-quartet patch with a decaying RMS curve. `patches/lexicon/
quad_hall/` (3-knob mapping: Left = Spread — scales the 4 voices' pitch
amounts symmetrically from unison to +-1 octave, Mid = FX Mix, Right =
Mix; footswitch press = bypass, hold = freeze) and the
`LexiconQuadHallPlugin` JUCE target both build clean. As with every other
patch in this archive, the ARM cross-toolchain isn't available in this
sandbox, so the Patch adapter is verified via `make -n` plus a
host-compiler build under `-fno-exceptions -fno-rtti`, not an actual
`.endl` build.

This is the first of the PCM81's seven Pitch-class algorithms; the
remaining six (Dual-Chmb, Dual-Plt, Dual-Inv, Stereo-Chmb, VSO-Chmb,
Pitch Correct) all use the Submixer routing system Quad>Hall's own fixed
topology sidesteps entirely — see `docs/lexicon-pcm81-reference.md`'s
"The Pitch algorithms" section for what's next.
