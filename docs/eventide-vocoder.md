# Eventide H3000-style Vocoder

Stage 1 (functional): the sixteenth Eventide H3000 algorithm in this
archive, Algorithm 115 per the Instruction Manual's own numbering (right
after Dense Room, Algorithm 114 - see `docs/eventide-h3000-notes.md` and
`docs/eventide-dense-room.md`). Per `CLAUDE.md`'s Primitive → Component
→ Block → Graph layering:

- **Primitives reused**: `StateVariableFilter` (already built for Patch
  Factory's tuneable filters) and `OnePoleLowpass` (already built for
  the PCM81 side) - no new primitives.
- **Block**: `dsp/include/dsp/algorithms/Vocoder.h` - a 12-band Analysis
  filterbank tracking the voice input's per-band energy, driving an
  identically-shaped Synthesis filterbank processing the instrument
  input.
- **Graph**: `dsp/include/dsp/graphs/VocoderAlgorithm.h` - the Block
  plus independent Left/Right input trim.

## Why this algorithm, sixteenth, and the channel-inversion gotcha

Straight numeric order. Per the manual: "A vocoder is used to impress
the articulatory characteristics of one instrument onto the timbre and
pitch of another... Make sure to get the channel inputs right when
you're using this program. **The right channel input is the analysis
(voice) input, and the left input is the synthesis (instrument)
input**." This is the opposite of the usual "Left is primary" pattern
elsewhere in this archive (e.g. every Left-In-only algorithm), so it's
called out explicitly here, in the Block's own doc comment, and in the
Patch adapter's comment - getting the channels backward means the
effect won't track anything.

## Reconstructing "Formant Speed" vs. "Envelope Speed" as two cascaded smoothing stages

The manual gives this algorithm two speed parameters with very similar-
sounding descriptions - #0 Formant Speed ("speed at which the synthesis
filter tracks the *spectrum* of the analysis input") and #1 Envelope
Speed ("speed at which the synthesis filter tracks the *articulation* of
the analysis input") - plus expert parameters #6 Max Resonance and #7
Min Error ("determines how close the synthesis filter tracks the input
spectrum"), whose language ("tracking error," "resonance") reads more
like an adaptive/LPC-style filter than a classic fixed-band channel
vocoder. The real PEL firmware isn't public, so rather than guessing at
an LPC implementation (Levinson-Durbin coefficient extraction is a large
undertaking, and PC81/H3000-era fixed-point DSPs are known to have used
exotic tricks not worth reverse-engineering blind), this Block builds
the well-understood classic channel-vocoder shape instead: a fixed bank
of 12 log-spaced bandpass filters (`StateVariableFilter`, 90Hz-5kHz)
analyzes the voice input's per-band energy through **two cascaded
one-pole smoothing stages** - Formant Speed first, Envelope Speed second
- matching the manual's own two-speed-parameter structure without
claiming to reproduce whatever the real adaptive mechanism was. Max
Resonance directly maps to filter Q ("how 'ringy'... may result in more
accurate tracking, but may result in more 'blurbles'" - genuinely
descriptive of a resonant filter). Min Error has no equivalent in this
design (no adaptive tracking loop exists to have an error metric) and is
listed under Known Simplifications rather than faked.

## Formant Shift, pseudo-stereo, and the noise gate

Formant Shift (#2) multiplies the Synthesis filterbank's center
frequencies upward relative to the Analysis filterbank's (up to 2.5x at
100%) - the classic "vocoder bands mismatched from the voice's own
formants" munchkinization effect the manual describes. Depth/Width
(#3/#4) implement the pseudo-stereo effect as a short delay tap
(Haas-effect-style): the vocoded mono signal feeds Left directly and a
Width-delayed copy feeds Right, blended in by Depth. Threshold (#8) is a
straightforward level gate on the analysis input's smoothed envelope,
matching the manual's "built-in noise gate which eliminates
mis-tracking caused by input noise or hum."

## Known simplifications

- **No real adaptive/LPC tracking** - see above; Formant Speed/Envelope
  Speed are two cascaded fixed-filterbank smoothing stages, not a
  genuine formant-tracking filter.
- **No Min Error parameter** - no adaptive tracking loop exists in this
  design to have a tracking-error criterion for.
- **12 fixed bands**, not user-configurable band count/frequencies - the
  manual doesn't expose per-band controls at all, so this is an internal
  implementation choice rather than a documented parameter gap.

## Status

Verified via `Vocoder` Block smoke tests:
1. With the analysis (Right) input silent, a loud synthesis (Left)
   input produces near-silent output - confirming the noise gate holds
   shut with nothing to track.
2. With a loud analysis tone present, the same synthesis input produces
   real, finite, non-silent output - confirming the gate opens and the
   filterbank passes audio through.
3. Depth=0 produces nearly-identical Left/Right output (mono); Depth=1
   with nonzero Width produces measurably different Left/Right output -
   confirming the pseudo-stereo effect is real.

`dsp_host_render vocoder` renders a noise (synthesis) + tone (analysis)
pair end to end with finite output. `patches/eventide/vocoder/` (3-knob
mapping: Left = Formant Shift, Mid = Envelope Speed, Right = Mix;
footswitch press = bypass, hold = toggle pseudo-stereo Depth between
mono/full) and the `EventideVocoderPlugin` JUCE target both build clean,
verified by launching the actual Standalone build headlessly (Xvfb) and
confirming both the parameter list and the architecture diagram render
correctly. As with the other patches in this archive, the ARM
cross-toolchain isn't available in this sandbox, so the Patch adapter is
verified via `make -n` plus a host-compiler build under
`-fno-exceptions -fno-rtti`, not an actual `.endl` build.
