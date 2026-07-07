# Eventide H3000-style Diatonic Shift

Stage 1 (functional): the first algorithm from a second device family in
this archive, after the five Lexicon PCM81 reverb cores. Per `CLAUDE.md`'s
Primitive → Component → Block → Graph layering:

- **Primitives**: `dsp/include/dsp/PitchShifter.h` (delay-line/grain-based
  real-time pitch shifter) and `dsp/include/dsp/DiatonicScale.h`
  (`diatonicSemitones()`, a scale-degree-to-semitone lookup).
- **Block**: `dsp/include/dsp/algorithms/DiatonicShift.h` — stereo
  dual-mono, each channel a `DelayLine` feeding a `PitchShifter`, with the
  shifted output regenerating back into the delay.
- **Graph**: `dsp/include/dsp/graphs/DiatonicShiftAlgorithm.h` — the Block
  plus input level trim and output stereo width.

## Why this device, why this algorithm, first

`docs/eventide-h3000-notes.md` documents the H3000 service manual's
hardware facts (three TMS32010 PELs, 64K-word shared delay memory,
variable-rate D/A as the real pitch-shift mechanism, 1.5s max delay). One
detail from that manual stood out as a starting point: the factory/default
program after an OS reset is **program 100, "DIATONIC SHIFT"** - about as
clear a statement as exists that diatonic-aware pitch shifting is this
box's signature effect, the way Concert Hall's shared "Reverb Shell"
topology was the natural starting point for the PCM81 side of this
archive.

## Topology

```
input -> Delay (0-1.5s) -> Pitch Shifter -> output (Mix blends against dry)
                              |
                              +--(* Regen)--> back into the Delay's input
```

Each lap through the loop shifts by the same diatonic interval again, so
a single input note cascades into an ascending (or descending) arpeggio
of repeats, spaced by the Delay time and decaying at a rate set by Regen.

## The pitch shifter

The H3000's actual mechanism - re-clocking its D/A to a different rate
than its A/D - has no equivalent in a fixed-sample-rate software engine,
so `PitchShifter.h` reconstructs the *audible* effect the classic
software way instead: two read taps into a delay line, each tap's delay
swept continuously at a rate set by the target pitch ratio and wrapping
every "grain" (default 70ms), offset from each other by half a grain so
a triangular crossfade masks the wrap. This is an original
reconstruction of the *behavior*, not the H3000's internal algorithm
(which isn't public) - the manual's own architecture notes ("a
delay-line-based pitch shifter... diatonic scale-quantized shift
amount") describe the category of technique this belongs to, not a
specific formula.

## Known simplifications

- **No real-time pitch tracking of the input.** True H3000-style diatonic
  harmonization needs to know which scale degree the currently-playing
  note sits on, because the same "one third up" interval is a different
  number of semitones depending on where you start (a major 3rd above the
  root vs. a minor 3rd above the 2nd degree, in the same major scale).
  That requires monophonic pitch detection - a substantial feature in its
  own right (the PCM81 manual's own "Pitch Correct" algorithm needed the
  same thing, see `docs/lexicon-pcm81-reference.md`). Without it, this
  engine computes the shift as a fixed transposition of N diatonic scale
  steps anchored at the scale's tonic (`dsp::diatonicSemitones()`) -
  musically coherent and always on-scale, but it won't re-derive a
  different interval size per input note the way the real hardware does.
  Flagged here rather than glossed over; a monophonic pitch tracker is the
  natural next Primitive/Component to build if this gets revisited.
- **Key is a stored parameter, not yet used by the shift math.** The math
  is anchored at the scale's own tonic regardless of the `Key` setting -
  wiring `Key` in is straightforward once real note tracking exists (today
  it would only rotate which absolute pitches count as "in scale" for a
  feature that isn't looking at absolute pitch yet).
- **No documented H3000 front-end to match, yet.** An Instruction Manual
  excerpt has turned up (see `docs/eventide-h3000-notes.md`), but it stops
  one page short of "Algorithm 100 - Diatonic Shift" itself, so
  `DiatonicShiftAlgorithm`'s front end (In Level, Width) is still an
  honestly-generic wrapper, not a verified reconstruction of the real
  box's control surface for this specific algorithm - unlike the PCM81
  Graphs, which mirror a manual-confirmed "Reverb Shell". Revisit once
  that algorithm's own page is available.
- Regen's cascade uses the same Delay/PitchShifter path for both
  directions of travel (feed-forward and feedback); the two stereo
  channels are independent dual-mono, not cross-coupled the way the
  PCM81 tank is - simpler than the real hardware's shared-bus/mailbox
  design (`docs/eventide-h3000-notes.md`), and a reasonable simplification
  for a first pass.

## A bug this caught

The first version of this Block fed the pitch shifter's output back into
its own input on the very next sample, with no delay in the loop at all.
That decays almost instantly (within milliseconds) rather than producing
audible, spaced-out repeats, because a same-sample feedback multiply at
48kHz decays orders of magnitude faster than a human ear reads as a
"repeat." The host harness's tone-burst render caught this concretely:
RMS was already down to -110dBFS by t=1s instead of ringing as a cascade.
The fix added an explicit `Delay` stage ahead of the shifter in the loop
path (0.4s default), so each lap is audibly spaced and the decay reads as
a diminishing sequence of transposed echoes instead of near-silence.

## Status

Verified via `dsp_host_render diatonic_shift` (tone-burst render, finite-
sample check, decay curve showing the cascade spread over ~2s rather than
dying within one) and a standalone smoke test of `PitchShifter` in
isolation (zero-crossing frequency estimate confirms shift direction and
approximate magnitude for +/-1 octave and +/-a fifth).

`patches/eventide/diatonic_shift/` (3-knob mapping: Left=Scale Degree,
Mid=Regen, Right=Mix, Scale fixed to Major; footswitch Press=bypass,
Hold=freeze the cascade via near-unity Regen) and the
`EventideDiatonicShiftPlugin` JUCE target (full parameter set including
Key/Scale/Grain/Delay) both build clean. As with the Lexicon patches, the
ARM cross-toolchain isn't available in this sandbox, so the Patch adapter
is verified via `make -n` plus a host-compiler build under
`-fno-exceptions -fno-rtti`, not an actual `.endl` build.
