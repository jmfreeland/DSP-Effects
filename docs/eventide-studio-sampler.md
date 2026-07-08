# Eventide H3000-style Studio Sampler

Stage 1 (functional): the twenty-first Eventide H3000 algorithm in this
archive, Algorithm 120 per the Instruction Manual's own numbering (right
after Phaser, Algorithm 119 - see `docs/eventide-h3000-notes.md` and
`docs/eventide-phaser.md`). Per `CLAUDE.md`'s Primitive → Component →
Block → Graph layering:

- **New Component**: `dsp/include/dsp/SamplerVoice.h` - one channel of a
  record/playback engine: record live input into a fixed buffer on
  command, then play it back (once or looped, within a settable Start/
  End range) with independent Pitch and Time control and its own
  Attack/Release envelope. Reuses `PitchShifter` unchanged for the
  independent-pitch case - no new Primitive was needed.
- **Block**: `dsp/include/dsp/algorithms/StudioSampler.h` - two fully
  independent `SamplerVoice`s, one per channel, matching the manual's own
  "Mono Mode" Block Diagram.
- **Graph**: `dsp/include/dsp/graphs/StudioSamplerAlgorithm.h` - the
  Block plus independent Left/Right input trim.

## Algorithm 121 isn't a separate algorithm at all

Before this page, `docs/eventide-h3000-notes.md` carried an "Open item"
flagging Algorithm 121 as a confirmed gap in the manual's own Table of
Contents - present in the algorithm numbering, absent as its own
described entry. Reading Studio Sampler's own manual page to the end
resolves it: "To help you save time when you wish to record a stereo
sample, we have provided algorithm 121. The default of this algorithm is
set to stereo, thus saving you the tedium of changing the default of the
mono/stereo option parameter... Therefore, when you wish to record a
mono sample, we suggest you use algorithm 120. When you wish to record a
stereo sample, use 121." Algorithm 121 is Algorithm 120, with its Record
Mode parameter's default flipped from mono to stereo - not a distinct
program. There is nothing separate to build.

## Why this algorithm, twenty-first, and more buildable than expected

Straight numeric order. This one was flagged in the task list as "likely
partial/skip" going in, since its manual page is dominated by hardware
workflow: physical record/stop/play buttons, an LCD VU meter, MIDI
keyboard triggering, and interactive "rock 'n' reel" point editing (turn
a knob to scrub a virtual tape reel) - none of which any of this
project's three consumers can reach (no display, no MIDI input pathway
anywhere in the codebase). But the manual's own Shift Mode parameter
(#18) turned out to make the actual DSP core straightforward: "In
constant length mode, splicing is used to shift the pitch of the sample
without changing the playback length. In generic sample mode, the sample
is simply played back faster or slower to alter the pitch." The second
mode is literally a single varispeed buffer read (no new mechanism at
all); the first is exactly the technique already proven for Timesqueeze
(Algorithm 113) - feed a computed-rate signal through `PitchShifter` for
an independent pitch trim - just applied to `SamplerVoice`'s own
buffer-scrub output instead of a live input. Nothing here needed a
phase-vocoder or other from-scratch time-stretch engine.

## Fixed Start/End instead of interactive "rock 'n' reel" editing

The manual's own point-editing workflow ("turn the knob... think of it
as a reel on that imaginary tape recorder") is fundamentally interactive
and needs the H3000's own display to show the current edit position -
nothing this project targets has an equivalent. `SamplerVoice` instead
exposes `setStartFraction()`/`setEndFraction()` (0..1 of the recorded
buffer) as direct, settable parameters - the same underlying purpose
(bounding what portion of the recording plays back) without the
interactive scrubbing workflow.

## Audio-level record triggering needs no MIDI at all

Trigger Mode/Threshold (#9-11) are exactly what they sound like: arm
record, then start actually capturing only once the input level crosses
a threshold. Since this is audio-domain (not MIDI), it's kept:
`SamplerVoice::record()` enters an `Armed` state when Trigger Mode is
Audio, monitoring `std::fabs(input)` against `Threshold` each sample and
only beginning to write into the record buffer once it's crossed. MIDI
Mode, Base Note, Split Point, and Drum Trigger (#12-17) are pure MIDI
routing and dropped entirely, per this project's established precedent
(no consumer implements MIDI input - see e.g.
`docs/eventide-band-delay.md`).

## Recording passes the dry input straight through

Per the manual: "the H3000 will be passing its audio input to both
output channels" while recording. `StudioSampler::processSample()`
implements this literally - while a channel's `SamplerVoice` is
recording (or armed and waiting for the trigger), that channel's output
is the live dry signal regardless of the Mix setting, not blended
through the normal dry/wet mix.

## Known simplifications

- **No MIDI-driven triggering, Base Note, Split Point, or Drum Trigger**
  - see above.
- **No interactive "rock 'n' reel" point editing** - Start/End are
  direct settable fractions instead.
- **Record Mode (mono/stereo capture linking) isn't modeled** - this
  Block's two channels are always fully independent, matching Algorithm
  120's own default ("Mono Mode") rather than Algorithm 121's linked
  single-sample stereo capture, which per the "Algorithm 121 isn't
  separate" note above is the same program with one default flipped, not
  a distinct topology worth a second implementation.
- **8 seconds of recording time per channel** (`kMaxRecordSeconds`),
  chosen as a reasonable value well under both this project's per-Patch
  working-buffer budget and the manual's own headline "11.8 seconds of
  stereo" figure.

## Status

Verified via `SamplerVoice` Component smoke tests:
1. Generic sampler mode, Pitch=0/Time=100%: playback reproduces the
   recorded input exactly, sample-for-sample (max diff 0.000000 in the
   steady region past the brief attack ramp-in).
2. Generic sampler mode, Pitch=+1200 cents: frequency measurably doubles
   (zero-crossing counting, ~600Hz from a 300Hz source) *and* playback
   finishes in half the time - confirming pitch and speed are genuinely
   tied together in this mode, per the manual.
3. Constant-length mode, Pitch=+1200 cents: frequency still doubles, but
   playback is still running at 0.9s into a 1-second source (duration
   preserved) - confirming independent pitch/time control.
4. Constant-length mode, Time=200%: playback finishes by 0.6s (duration
   roughly halved) with pitch unaffected.
5. Loop: playback continues past the natural end when Loop is enabled.
6. Audio-triggered recording: input below Threshold while armed doesn't
   advance the write cursor at all (`recordedLength() == 0`); once input
   crosses Threshold, real capturing begins immediately.

`dsp_host_render studio_sampler` records a 300Hz tone into both channels,
then plays back Left with Pitch=+1200 cents (octave up, same duration)
and Right with Time=200% (double speed, same pitch), with finite output
throughout. `patches/eventide/studio_sampler/` (3-knob mapping, applied
to both channels together: Left = Pitch, Mid = Time, Right = Mix;
footswitch press = bypass, hold = a single-button looper-pedal gesture
cycling idle → record → play → stop) and the
`EventideStudioSamplerPlugin` JUCE target both build clean, verified by
launching the actual Standalone build headlessly (Xvfb) and confirming
both the parameter list (the full per-channel set × 2, plus Mix) and the
architecture diagram render correctly. As with the other patches in this
archive, the ARM cross-toolchain isn't available in this sandbox, so the
Patch adapter is verified via `make -n` plus a host-compiler build under
`-fno-exceptions -fno-rtti`, not an actual `.endl` build.
