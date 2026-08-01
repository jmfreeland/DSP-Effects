# Eventide H3000-style Swept Reverb

Stage 1 (functional): the seventh Eventide H3000 algorithm in this
archive, Algorithm 106 per the Instruction Manual's own numbering (right
after Swept Combs, Algorithm 105 - see `docs/eventide-h3000-notes.md`
and `docs/eventide-swept-combs.md`). Per `CLAUDE.md`'s Primitive →
Component → Block → Graph layering:

- **Primitives**: reuses `dsp/include/dsp/LFO.h`'s `nextRandomWalk()`
  (built for Swept Combs) and, notably, `dsp/include/dsp/FeedbackMatrix.h`'s
  `householderMix()` - previously only used by the Lexicon PCM81 reverb
  cores' own 8-line tank (`dsp::algorithms::ReverbCore`). This is the
  first time a Primitive has been shared across the PCM81 and Eventide
  device families in this archive.
- **Block**: `dsp/include/dsp/algorithms/SweptReverb.h` - six
  independently-swept `DelayLine`s (own base Delay/Rate/Depth, same
  Master-scaling pattern as Swept Combs) feeding a Householder-mixed
  feedback network instead of a stereo mixer.
- **Graph**: `dsp/include/dsp/graphs/SweptReverbAlgorithm.h` - the Block
  plus input trim.

## Why this algorithm, seventh

Straight numeric order again, and the direct sequel to Swept Combs:
Algorithm 106's own manual page describes "the same" six-delay-line/
sweep-generator shape as Algorithm 105, changed only in what the six
lines feed into - a "Reverb Network" instead of a stereo mixer. Building
it right after Swept Combs let the sweep-generator design (rate/depth
ranges, random-walk LFO) carry over directly, isolating this Block's
actual new design question to just the network topology.

## Why reuse `householderMix()` from the PCM81 side

The manual documents every parameter of the "Reverb Network" (it's
controlled by the same Feedback/Delay/Rate/Depth ideas as the rest of
the algorithm) but not its internal mixing topology - expected, since
the real PEL firmware isn't public (see `CLAUDE.md`'s "inspired by"
framing, restated in this Block's own doc comment). Rather than invent a
new, undocumented mixing scheme from scratch, this Block reuses the
Householder reflection matrix already built, tested, and shipped for the
PCM81's own 8-line tank (`dsp::algorithms::ReverbCore`) - the same
well-understood technique (distributes energy densely across all lines
every pass without changing total energy) applied to 6 lines instead of
8. This is a deliberate, documented choice to reuse a proven mechanism
rather than a claim that the real H3000 hardware works this way
internally.

## Design choices not fully specified by the manual

Same category as Swept Combs (see that doc's own section) - sweep
rate/depth ranges and default per-line values are original, reasonable
choices, not manual-verified facts. Additionally: the manual's own
Feedback parameter is applied here as a single flat gain multiplying the
Householder-mixed signal before it's written back into each line (rather
than, say, per-line/per-frequency-band decay gains like the PCM81
tank's), since Swept Reverb's own page doesn't document a frequency-
dependent decay control the way Reverb Factory (Algorithm 107, next)
does.

## Known simplifications

Same as Swept Combs (no Glide, Repeat mutes input rather than truly
freezing playback), plus the same fixed random-walk-seed-sharing bug -
see `docs/eventide-swept-combs.md`. This Block already staggered each
line's LFO *phase* (`setPhase(i/kNumLines)`), which wasn't enough on its
own: phase only changes cycle timing, not the shared xorshift state
`nextRandomWalk()` draws its targets from, so the six lines' sweep
*content* was still identical before this fix.

## Status

Verified via:
1. The `SweptReverb` Block directly: an impulse produces substantial
   energy in the first 100ms (0.108) and meaningfully decayed but still
   finite energy around t=1s (0.00008) at Feedback=0.7 - a real, decaying
   reverb tail, not silence or runaway feedback.
2. `dsp_host_render swept_reverb` renders an impulse response end to end
   with finite output and a sensible RMS decay curve.

`patches/eventide/swept_reverb/` (3-knob mapping: Left = Feedback, Mid =
Master Delay, Right = Mix - Rate/Depth stay at their built-in defaults
since the hardware only has 3 knobs, the JUCE plugin exposes both
Masters plus the full per-line Tedium set) and the
`EventideSweptReverbPlugin` JUCE target both build clean, verified by
launching the actual Standalone build headlessly (Xvfb) and confirming
both the parameter list and the architecture diagram (including the
Feedback loop arrow back into the six lines) render correctly. As with
the other patches in this archive, the ARM cross-toolchain isn't
available in this sandbox, so the Patch adapter is verified via
`make -n` plus a host-compiler build under `-fno-exceptions -fno-rtti`,
not an actual `.endl` build.
