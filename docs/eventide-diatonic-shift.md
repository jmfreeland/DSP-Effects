# Eventide H3000-style Diatonic Shift

Stage 1 (functional): the first algorithm from a second device family in
this archive, after the five Lexicon PCM81 reverb cores. Per `CLAUDE.md`'s
Primitive → Component → Block → Graph layering:

- **Primitives**: `dsp/include/dsp/PitchShifter.h` (delay-line/grain-based
  real-time pitch shifter), `dsp/include/dsp/PitchDetector.h`
  (autocorrelation-based real-time monophonic pitch tracker), and
  `dsp/include/dsp/DiatonicScale.h` (`diatonicSemitones()`,
  `nearestScaleDegreeIndex()`, and the `HarmonicInterval` enum - the
  scale-degree math that turns a tracked pitch plus a chosen interval into
  an absolute semitone shift).
- **Block**: `dsp/include/dsp/algorithms/DiatonicShift.h` — mono-sums the
  stereo input, runs it through one shared `Delay` and one shared
  `PitchDetector`, and drives two independent `PitchShifter`-based Voice
  generators (Left, Right) from the tracked note.
- **Graph**: `dsp/include/dsp/graphs/DiatonicShiftAlgorithm.h` — the Block
  plus input level trim (no generic width/stereo-rotate control - see
  "Why no Width control" below).

## Why this device, why this algorithm, first

`docs/eventide-h3000-notes.md` documents the H3000 service manual's
hardware facts (three TMS32010 PELs, 64K-word shared delay memory,
variable-rate D/A as the real pitch-shift mechanism). One detail from that
manual stood out as a starting point: the factory/default program after an
OS reset is **program 100, "DIATONIC SHIFT"** - about as clear a statement
as exists that diatonic-aware pitch shifting is this box's signature
effect, the way Concert Hall's shared "Reverb Shell" topology was the
natural starting point for the PCM81 side of this archive.

## History: two versions, and why the first one wasn't good enough

**Version 1** (fixed transposition) computed a shift by anchoring at the
scale's tonic regardless of what note was actually playing - "a third up"
was always +4 semitones (a major 3rd from the root), whether the input was
the root or the 2nd scale degree (where a diatonically-correct 3rd is only
+3 semitones). This was flagged as a "known simplification" at the time,
built before the H3000 Instruction Manual's own "Algorithm 100" page (its
Instruction Manual, not just the Service Manual used until then) had
turned up.

That page changed the picture: it states plainly that "the H3000 tracks
your pitch and plays the correct notes," and shows a `Shownote` display
parameter confirming detected pitch is a first-class part of the
algorithm - not an optional refinement, but the thing the name refers to.
Version 1 didn't do this at all. **Version 2** (current) rebuilds the
Block around genuine real-time monophonic pitch tracking, matching the
manual's own block diagram:

```
Left In, Right In -> sum (mono) -> Delay (0-1s) -+-> Pitch Tracker
                                                  +-> Left Voice  (PitchShifter) -> Left Out
                                                  +-> Right Voice (PitchShifter) -> Right Out
Left Out  * L Feedback ---\
Right Out * R Feedback ---+--> back into the mono sum
```

Both Voices shift the *same* delayed mono signal - the difference between
them is entirely in which harmonic interval each one is set to.

## The shift math

Given the tracker's detected frequency, `dsp::nearestScaleDegreeIndex()`
finds which of the current scale's 7 degrees the note is closest to (in
`Key`+`Scale` terms, spanning octaves as needed). A chosen
`HarmonicInterval` (e.g. `kThirdUp`) adds a scale-degree offset via
`harmonicIntervalDegreeOffset()`; `diatonicSemitones()` converts the
resulting target degree back to an absolute semitone position; the
`PitchShifter` is set to shift by `target - detected` (using the
*continuous* detected pitch, not the snapped degree, for that subtraction)
so a slightly out-of-tune input still lands exactly on the intended target
pitch - equal-tempered pitch correction as a side effect of the math,
without a separate `Quantize` stage. This is what makes "a third up" 4
semitones from the root but only 3 from the 2nd degree - the actual reason
this rebuild happened.

`HarmonicInterval` also includes the four pedal-tone choices from the
manual's list (`kLowTonicPedal`, `kHighTonicPedal`, `kLowDominantPedal`,
`kHighDominantPedal`) - these target an *absolute* scale degree (the tonic
or the 5th) in a register relative to the detected note's own octave,
rather than a relative offset from it.

## The pitch detector

`PitchDetector.h` is normalized autocorrelation over a sliding window,
run once per ~10ms hop (not every sample - the search itself is the
expensive part): for each candidate lag in the configured frequency
range, compute the normalized correlation between the signal and itself
shifted by that lag; the lag with the highest correlation (above a
confidence threshold) is the detected period. This is the same family of
technique as YIN, chosen over zero-crossing counting (too fragile against
harmonics) and over FFT/phase-vocoder methods (autocorrelation needs no
transform, fitting the same no-allocation constraints as the rest of
`dsp/`). One correction made during development: a pure or harmonically
simple tone often correlates almost as strongly at an integer submultiple
of its true period (half the lag = double the frequency), which biased a
naive global-max search an octave too low - a standalone smoke test
against known frequencies caught this concretely (110/220/440Hz inputs
were detected correctly, but a synthetic pure-sine round-trip through the
full Block reported half the true frequency). Fixed by preferring the
shortest lag that's still nearly as confident as the global best, halving
repeatedly while that holds - a standard octave-error mitigation.

This is an original reconstruction of a *technique category*, not the
H3000's internal algorithm (not public) - the same honesty framing as
`PitchShifter.h`.

## The pitch shifter

Unchanged from the original design: two read taps into a delay line, each
swept at a rate set by the target pitch ratio and wrapping every "grain"
(default 70ms), offset from each other by half a grain so a triangular
crossfade masks the wrap. The H3000's actual mechanism - re-clocking its
D/A to a different rate than its A/D - has no equivalent at a fixed
sample rate, so this reconstructs the *audible* effect the classic
software way instead.

## Why no Width control

The original Graph applied a generic `rotateStereoWidth()` after the
Block, matching a pattern used elsewhere in this archive. Now that the
real topology is confirmed genuinely mono-in / independently-stereo-out
(two separately-harmonizing Voices, not a stereo pair being rotated),
that control was removed rather than kept as cosmetic filler - rotating
mid/side content that's actually "two different chosen harmony notes"
would smear a distinction the algorithm is specifically designed to keep
separate.

## Known simplifications

- **No fully-custom Scale 1/Scale 2 tables**, and consequently **no just
  intonation.** The manual's Left Voice/Right Voice list includes two
  user-definable 12-entry interval tables (one arbitrary cents offset per
  chromatic input note - expert parameters `#11-22`/`#23-34`), richer than
  a diatonic-degree offset and outside this engine's `HarmonicInterval`
  model. The factory preset catalog confirms this is a real, used feature,
  not a hypothetical one: presets like "JUST 3RD & 5TH" and "JUST 4TH &
  6TH" use Scale 1/Scale 2 to store pure-harmonic-ratio (just intonation)
  cent offsets instead of the equal-tempered 100-cents-per-semitone this
  engine always uses. Not implemented; the 14 relative intervals plus 4
  pedal tones are - and are confirmed against real factory presets too
  ("G MAJ MOD WHEEL" uses exactly this engine's default Left/Right Voice
  pairing, a 3rd up and a 5th up).
- **Pedal-tone octave placement is an original reconstruction.** The
  manual states "lo ton ped"/"hi ton ped"/"lo dom ped"/"hi dom ped" exist
  but doesn't specify the exact octave convention; this engine places
  "low" one octave below the detected note's octave and "high" at the
  same octave, a reasonable but unverified guess.
- **No Source (polyphonic/solo) tracking-aggressiveness control.** The
  real hardware's pitch tracker is tunable toward "polyphonic" (fuller
  mixes) or "solo" (monophonic instruments); this detector always assumes
  a clean monophonic input. In practice this shows up when Feedback is
  nonzero: the shifted Voice outputs mix back into the shared mono input
  ahead of the tracker, so the tracker ends up analyzing a genuinely
  mixed (two-pitch) signal, which a plain autocorrelation search isn't
  robust against - the real hardware's Source control exists to help with
  exactly this kind of case. Not a bug (confirmed via a smoke test
  isolating Feedback=0, where tracking is accurate to within a few cents,
  document below), just an accuracy limit under cascading feedback.
- **Key doesn't yet affect *absolute* pitch class mapping beyond the
  simple `semitoneFromC - key` shift used to place the tonic** - this is
  correct for the interval math, but hasn't been checked against how the
  real hardware's own `Key` parameter behaves at the edges (e.g. exact
  octave/register conventions for `Shownote`-style display), since that
  detail isn't exposed by this engine.
- No documented H3000 front-end beyond the Block's own parameters - see
  "Why no Width control" above for the one control this repo previously
  added that didn't belong.
- **No zero-offset ("unison"/pitch-correction-only) relative interval.**
  `HarmonicInterval` deliberately has no zero-degree entry, matching
  Algorithm 100's own named Left/Right Voice interval list (2nd Down
  through Octave Up, skipping unison, plus the four pedal tones) - see
  `harmonicIntervalFromDegreeOffset()`'s comment in `DiatonicScale.h`.
  The factory preset catalog complicates this: preset #623 "PITCH
  QUANTIZE" is built on the DIATONIC SHIFT algorithm and its description
  reads "This program quantizes the input to the nearest chromatic
  interval" - i.e. straight pitch correction, no harmony, which needs a
  zero (or near-zero) relative offset this engine's named-interval model
  can't select. Left undecided rather than guessed: the catalog entry
  doesn't say how the real unit reaches that state (most likely a
  Scale 1/Scale 2 custom table set to an identity/0-cents mapping per
  chromatic note, which would tie back to the already-documented
  Scale 1/Scale 2 gap above, rather than a hole in the named-interval
  list specifically) so no code change was made against this alone.

The 630-preset factory catalog (`#1`-`#999`, read in full across two
manual excerpts) also confirms several DIATONIC SHIFT usages beyond the
"3rd up + 5th up" default already cited: #605 "A MINOR CHORDS" ("Play or
sing a solo line in A minor. The H3500 will generate two perfect 'in-key'
harmonies" - the two-Voice-harmony concept in the manual's own words),
#609 "DIATONIC DANCE" (delayed harmony - "after half a second, you get a
harmony," i.e. the Delay parameter used well above its default), #625
"THIRD & FIFTH" (the same interval choice as this engine's own default,
independently), and #626 "THIRD & OCTAVE" (one Voice up a third, the
other down an octave - confirming the two Voices are meant to be set to
unrelated/asymmetric directions, not just parallel harmonies). Preset
#701 "A LYDIAN 6THS" ("Play solo lines using A Lydian modal scales")
confirmed a real hardware scale option this engine was missing; Lydian
has now been added to `Scale`/`scaleSteps()` and the plugin's Scale
choice list as a direct, low-risk fix from that finding (whole-tone-up
4th relative to Major: `{0,2,4,6,7,9,11}`), unlike the Scale 1/Scale 2
and Pitch Quantize gaps above, which stay open pending clearer primary
sources.

## Status

Verified via three layers of smoke tests, each catching a real bug before
moving to the next:

1. `PitchDetector` in isolation: confirmed accurate to ~1Hz for 110/220/
   440Hz test tones, confirms silence reports no confident pitch, and
   (after the octave-error fix above) no longer reports half the true
   frequency for a pure sine.
2. `nearestScaleDegreeIndex()`/`harmonicIntervalDegreeOffset()` round-trip
   math: confirmed a detected D (2nd degree in C major) shifted up a
   diatonic 3rd lands on F (+3 semitones), not the +4 semitones a fixed
   root-anchored transposition would give - the actual proof this rebuild
   accomplished its goal.
3. The full `DiatonicShift` Block: a sustained D drives Left Voice to
   F (~349Hz) and Right Voice to A (~440Hz) as expected, and
   `dsp_host_render diatonic_shift` renders the same scenario end to end
   (feedback held at 0 for that specific render, to keep the demonstration
   about pitch-tracking correctness rather than the separate, documented
   feedback-contamination accuracy limit above).

`patches/eventide/diatonic_shift/` (3-knob mapping: Left = Left Voice
interval -7..+7 scale steps, with Right Voice trailing a fixed 5th above
it; Mid = shared Feedback; Right = shared Mix; Key/Scale fixed to C Major
since the hardware only has 3 knobs - the JUCE plugin exposes independent
Left/Right Voice, Key, Scale, Tune, and pitch-tracker frequency range) and
the `EventideDiatonicShiftPlugin` JUCE target both build clean, verified
by launching the actual Standalone build headlessly (Xvfb) and confirming
both the parameter list and the architecture diagram render correctly. As
with the Lexicon patches, the ARM cross-toolchain isn't available in this
sandbox, so the Patch adapter is verified via `make -n` plus a
host-compiler build under `-fno-exceptions -fno-rtti`, not an actual
`.endl` build.
