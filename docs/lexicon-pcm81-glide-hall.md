# Lexicon PCM81-style Glide>Hall Algorithm

Stage 1 (functional): the first of the PCM81's five 6-Voice algorithms
(the manual's own second algorithm class, after the 5 already-built
4-Voice reverb cores — see `docs/lexicon-pcm81-reference.md`). Per
`CLAUDE.md`'s Primitive → Component → Block → Graph layering:

- **New Primitives/Components**: none — reuses `DelayLine` and
  `GlideParameter` directly (no new Component needed; the glide-tap and
  six-voice bank wiring is algorithm-specific enough to keep inline in
  the Block, matching precedent like `BandDelay.h`'s own shared-tapped-
  delay-line approach).
- **Block**: `dsp/include/dsp/algorithms/GlideHall.h` — a stereo pair of
  gliding 2-tap delays (A/B, `GlideParameter`-smoothed) feeding six delay
  voices (three from the left glide output, three from the right), whose
  combined output runs **in series** into a fixed, owned `ConcertHall`
  reverb instance.
- **Graph**: `dsp/include/dsp/graphs/GlideHallAlgorithm.h` — the Block
  plus the Controls row's In Lvl/Pan, a Voice Diffusion stage, and the
  shared FX Width/Hi-Cut/Adjust/Mix chain.

## Why this algorithm, and why series (not parallel)

Straight order per the manual's own 6-Voice section (`docs/references/
lexicon-pcm81-user-guide-rev1.pdf`, pages 3-8..3-9): "A stereo pair of
2-tap gliding delays feeds six individually adjustable delay voices...
The output of these delay voices is fed into a Concert Hall reverb
algorithm." The manual is explicit that Glide>Hall (along with Res1>Plate
and Res2>Plate) runs its 6-voice effect **in series** with the reverb,
while Chorus+Rvb and M-Band+Rvb run theirs **in parallel** — confirmed by
the shared "About the 6-Voice Algorithms" page: "In the Glide>Hall,
Res1>Plate and Res2>Plate algorithms, the reverb effect is in series with
the 6-voice effect. Use FX Mix to set the relative level of dry and
reverberated effect." `GlideHall::setFxMix()` implements exactly that:
0 = the raw six-voice signal only, 1 = fully reverbed, matching the
existing `ConcertHallAlgorithm`'s own `fxMix_` shape even though the two
topologies (parallel vs. series) differ — the reverb's *input* is the
six-voice output rather than a copy of the dry signal.

## Two coupled feedback buses, not per-voice self-contained combs

Unlike the existing 4-Voice `Voice` Component (a self-contained `Comb` —
each voice's feedback recirculates only into itself), the manual's
Feedback/Cross-Feedback row describes something more coupled for every
6-Voice algorithm: "Voices 1, 2, and 3 Fbk control the individual voice
feedback levels from the left channel voice delay outputs to the left
channel delay feedback input... Voices 1, 2, and 3 X-Fbk control the
individual voice feedback levels from the left channel voice outputs to
the right channel delay feedback input" (and the symmetric statement for
Voices 4-6 into the right channel). That's two shared write buses (left,
right), each fed by a weighted sum of every voice's *previous* sample —
own-bank voices contribute via Fbk, opposite-bank voices via X-Fbk. The
same shape applies one level up, to the two glide taps' own Fbk/X-Fbk.

`GlideHall.h` models this directly: `leftBank_`/`rightBank_` are each one
shared `DelayLine` (matching `BandDelay.h`'s "one shared delay line, many
independent read taps" pattern) with three read taps apiece (Voice 1-3 on
`leftBank_`, Voice 4-6 on `rightBank_`), and a single write per sample
that sums the incoming glide signal with every voice's weighted
feedback/cross-feedback contribution. Because `DelayLine::read()` always
happens before that sample's `write()`, every voice naturally reads the
*previous* sample's bus contents without any extra one-sample-latency
bookkeeping — unlike the H3000 patch-matrix algorithms (Patch Factory,
Mod Factory One/Two), which need that bookkeeping explicitly because
their sources are arbitrary user-patched destinations rather than a fixed
pair of buses.

## A real stability bug the manual's own guideline caught

The manual warns: "The sum of all Fbk and X-Fbk values for each channel
should be less than 100%." The first host-render smoke test used 0.3 own
feedback + 0.05 cross-feedback per voice, three voices per bus — 1.05
total, over the manual's own stated limit — and the impulse response's
RMS was *rising* over the render window (0.003 → 0.0046 across seconds
1-4) instead of decaying, exactly the runaway-feedback symptom the
manual's guideline exists to prevent. Lowering the demo defaults to 0.15
own + 0.03 cross (0.54 total, safely under the limit) fixed it — the same
impulse response now decays monotonically (-50dBFS → -116dBFS across the
same window). The Block itself doesn't clamp this sum (the manual frames
it as a user guideline, not a hard limit — real feedback networks like
this can be pushed into self-oscillation deliberately, the same way a
delay pedal's feedback knob can), but both `host/src/main.cpp`'s
`renderGlideHall()` and `patches/lexicon/glide_hall/PatchImpl.cpp`'s
default patch now respect it.

## Where "Voice Dif" applies (an interpretation call)

The Controls row lists a `Voice Dif` parameter (present in every 6-Voice
algorithm's row 0), but Glide>Hall's own diagram doesn't draw an explicit
diffusion box the way the 4-Voice Reverb Shell does for its parallel
Voice bank. This project places it as a stereo pair of independent
`DiffuserChain`s applied to the material entering the glide delays (in
the Graph, before the Block) — a reasonable, documented placement rather
than a verified match to an undrawn diagram detail.

## Known simplifications

- **No GldResp/GldRange control for the glide taps** — the manual's own
  Glide FX row (Gld Lvl, A/B Left/Right, Fbk L/R, X-Fbk L/R) has no glide
  *response* parameter of its own (unlike the 4-Voice algorithms'
  post-delay, which explicitly has `GldResp`/`GldRange`). A fixed,
  fast-but-audible internal glide response (60/100, 1s range - a ~0.2s
  tap-change chirp rather than a multi-second pitch-bend) produces the
  "pitch modulation, flange" character the manual describes without
  inventing an unlisted control, or leaving the signal detuned for
  seconds after every tweak.
- **No PstMix/PstGld post-delay** — the manual scopes those to the
  4-Voice algorithms only; Glide>Hall's own Rvb Time row (Low Rt/Mid
  Rt/Crossover/Rt HC/Pre Delay/Ref Lvl+Dly) has no post-delay position.
- **FX/Rvb Width behavior** is the same original `rotateStereoWidth()`
  reconstruction used throughout this archive's PCM81 side, not a
  verified match to the hardware's exact labeled value table.

## Status

Verified via isolated Block and Graph smoke tests (`dsp::algorithms::
GlideHall`, `dsp::graphs::GlideHallAlgorithm`): finite output with real
energy for an impulse through a representative 6-voice/glide patch;
`FxMix=0` vs. `FxMix=1` measurably differ (confirming the series topology
actually routes through the reverb); `Mix=0` returns the dry signal
unchanged (bit-exact passthrough). `dsp_host_render glide_hall` renders a
5-second impulse response with a monotonically-decaying RMS curve (see
the stability bug above). `patches/lexicon/glide_hall/` (3-knob mapping:
Left = glide amount — sweeps both A/B tap delay times together from a
fast flange character to a slower chorus-like smear, Mid = FX Mix,
Right = Mix; footswitch press = bypass, hold = freeze) and the
`LexiconGlideHallPlugin` JUCE target both build clean. As with every
other patch in this archive, the ARM cross-toolchain isn't available in
this sandbox, so the Patch adapter is verified via `make -n` plus a
host-compiler build under `-fno-exceptions -fno-rtti`, not an actual
`.endl` build.
