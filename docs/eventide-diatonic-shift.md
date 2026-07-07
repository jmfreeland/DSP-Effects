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
input -> Delay (0-1s) -> Pitch Shifter -> output (Mix blends against dry)
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

## Confirmed against the primary source

The H3000 Instruction Manual's own "Algorithm 100 - Diatonic Shift" page
(p.45-47) turned up after this Block was already built, and the real
topology differs from this one more substantially than the "known
simplifications" below originally suggested - not in ways that were
guessed and then hedged, but in ways now directly confirmed:

- **Mono-in, stereo-out**, not stereo dual-mono. The real algorithm sums
  Left+Right input, runs it through *one* shared Delay and *one* shared
  Pitch Tracking stage, whose detected note then drives two independent
  Voice generators (Left Voice, Right Voice) - this engine instead runs
  two fully independent per-channel chains with no shared pitch analysis.
- **Real-time monophonic pitch tracking is the actual mechanism**, not
  an optional refinement. The manual's own description states it plainly:
  "the H3000 tracks your pitch and plays the correct notes" - the correct
  *number of semitones* for "a third up" depends on which scale degree is
  currently sounding (4 semitones above the root, 3 above the 2nd degree,
  in the same major scale), and there's even a `Shownote` display
  parameter confirming the detected pitch is a first-class part of the
  algorithm. This engine has no pitch detection at all: `setScaleDegree()`
  computes a fixed transposition anchored at the scale's tonic
  (`dsp::diatonicSemitones()`) and applies it uniformly regardless of what
  note is actually playing - on-scale and musically coherent, but not
  "diatonic shift" in the sense the name and the manual describe. This is
  this engine's most significant gap, not a minor one.
- **Left Voice / Right Voice are discrete interval choices** (`-octave,
  -seventh, ... -second, +second, ... +octave unison, lo ton ped, hi ton
  ped, lo dom ped, hi dom ped, scale 1, scale 2`), not a continuous
  scale-degree integer - and `scale 1`/`scale 2` are fully custom,
  user-defined 12-entry interval tables (one arbitrary cents offset per
  chromatic input note, expert-mode parameters `#11-22`/`#23-34`), far
  richer than this engine's fixed Major/Minor/Dorian/Mixolydian enum.
- Independent `L Feedback`/`R Feedback` and `L Mix`/`R Mix` per channel
  (this engine shares one `Regen` and one `Mix` across both).
- A separate `Quantize` on/off (pitch-corrects the output to the nearest
  equal-tempered note) and a `Tune` reference control (-50..+50 cents, to
  match the H3000's A-440 to an external instrument) - neither exists
  here.
- Delay range is genuinely 0-1000ms (this engine's `kMaxDelaySeconds`,
  1.5s, was carried over from the *service* manual's general 64K-word
  delay-memory figure, not from this algorithm's own parameter range).

None of this is fixed yet - see "Open item" below for the recommended
next step if closer fidelity is wanted.

## Known simplifications

- **No real-time pitch tracking of the input** - see "Confirmed against
  the primary source" above; this is now a confirmed core-mechanism gap,
  not a hedged guess. A monophonic pitch tracker (e.g. autocorrelation or
  a zero-crossing-based F0 estimator) would be the natural next
  Primitive/Component if this gets revisited, alongside restructuring the
  Block to mono-sum the input and drive independent Left/Right Voice
  generators from one shared tracked pitch.
- **Key is a stored parameter, not yet used by the shift math.** The math
  is anchored at the scale's own tonic regardless of the `Key` setting -
  wiring `Key` in is straightforward once real note tracking exists (today
  it would only rotate which absolute pitches count as "in scale" for a
  feature that isn't looking at absolute pitch yet).
- **Front end is a generic wrapper, not the real algorithm's control
  surface.** Now that Algorithm 100's own page is available (see above),
  `DiatonicShiftAlgorithm`'s front end (In Level, Width) is confirmed to
  not match the real one - the real algorithm doesn't have a generic
  "Width" control at all, since it's mono-in / genuinely-stereo-out via
  independent Left/Right Voice generators rather than a stereo signal
  whose width gets rotated.
- Regen's cascade uses the same Delay/PitchShifter path for both
  directions of travel (feed-forward and feedback); the two stereo
  channels are independent dual-mono, not cross-coupled the way the
  PCM81 tank is - simpler than the real hardware's shared-bus/mailbox
  design (`docs/eventide-h3000-notes.md`), and a reasonable simplification
  for a first pass, though now superseded by the mono-in/dual-voice
  topology point above as the more significant structural difference.

## Open item

Closing the pitch-tracking gap is a real Stage 1 correctness question,
not Stage 2 polish - "tracks your pitch and plays the correct notes" is
the algorithm's stated core mechanism, not a refinement of something
that already works. Revisiting this would mean: (1) a monophonic pitch
detector Primitive, (2) restructuring `DiatonicShift` to mono-sum input
and drive independent Left/Right Voice generators from one shared
tracked pitch, (3) a discrete interval-choice parameter (matching the
real list) instead of a continuous scale-degree integer, and (4)
independent per-channel Mix/Feedback. Not started - flagging the scope
rather than doing it silently, since it's a substantially bigger rebuild
than the rest of this file's simplifications.

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
