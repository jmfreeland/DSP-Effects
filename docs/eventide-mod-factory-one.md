# Eventide H3000-style mod factory|one

Stage 1 (functional): the twenty-second Eventide H3000 algorithm in this
archive, Algorithm 122 per the Instruction Manual's own numbering (right
after Studio Sampler, Algorithms 120/121 - see
`docs/eventide-h3000-notes.md` and `docs/eventide-studio-sampler.md`).
Per `CLAUDE.md`'s Primitive → Component → Block → Graph layering:

- **New Primitives**: `dsp/include/dsp/MultiWaveLFO.h` (13 waveforms -
  6 continuous, 5 audio-triggered one-shot sweeps, 2 toggle sweeps) and
  `dsp/include/dsp/EnvelopeDucker.h` (an attack/decay envelope follower
  with a second, compressor-style "ducker" output) - neither had an
  equivalent anywhere in the archive.
- **Block**: `dsp/include/dsp/algorithms/ModFactoryOne.h` - a genuine
  modular patch-bay: 2 sweepable delays, 2 state-variable filters, 2
  `MultiWaveLFO`s, 2 `EnvelopeDucker`s, 2 amplitude modulators, 4
  two-input mixers, 2 modulation scalers, and 2 mod knobs, wired by a
  settable 28-destination x 26-source patch matrix.
- **Graph**: `dsp/include/dsp/graphs/ModFactoryOneAlgorithm.h` - the
  Block plus independent Left/Right input trim.

## Why this algorithm, twenty-second, and the largest yet

Straight numeric order. Per the manual's own "Using mod factory" intro:
"mod factory for the Eventide H3000 is a collection of two new
algorithms and one hundred preset effects patches... Each algorithm
gives the user access to a dozen or so basic digital signal processing
'modules'. Using software 'patch cords', the user can connect the
modules to create literally thousands of unique signal processing
algorithms." This isn't one more fixed effect - it's a dedicated
build-your-own-algorithm toolkit, and its patch matrix (28 destinations
x 26 sources) is more than double Patch Factory's own (13 x 16,
Algorithm 111 - see `docs/eventide-patch-factory.md`), the only other
genuine patch-bay in this archive. The same one-sample-latency technique
that keeps Patch Factory's matrix inherently acyclic for any user-chosen
patch is reused unchanged here, just at a larger scale.

## Two genuinely new modules needed genuinely new Primitives

- **`MultiWaveLFO`**: the manual's own LFO module lists thirteen
  waveforms across three families - six continuous (sine, square,
  sawtooth, triangle, exponential sawtooth, exponential triangle) that
  free-run; five "triggered" waveforms (named literally "triggered
  sine," "triggered saw," etc.) that idle until an audio input crosses a
  threshold, then sweep through exactly one cycle and hold; two "toggle"
  waveforms that alternate sweeping up and down on each successive
  trigger. No existing primitive in this archive (including the simple
  sine/triangle/random-walk `LFO` used elsewhere) covered this. The
  manual's own prose ("the first 8 waveforms... are continuous; the next
  5... are triggered; the last 2... are toggle") adds up to 15 against
  its own 13-item list - an internal inconsistency (this archive has
  documented others, e.g. Multi-Shift's Feedback parameter units).
  `MultiWaveLFO` follows the individual item *names* instead (six are
  plainly continuous shapes, five are literally named "triggered ...",
  two literally named "toggle ..."), which are unambiguous where the
  aggregate count isn't.
- **`EnvelopeDucker`**: the manual's own Envelope Detector module has two
  outputs from one attack/decay follower - a plain envelope output, and
  a "ducker" output that starts near full level and shrinks
  compressor-style once the input crosses a threshold, meant to be
  patched into another module (typically an Amplitude Modulator) so one
  signal can duck another out of the way.

Both were caught with real bugs during smoke-testing before being
trusted:
- `MultiWaveLFO`'s one-shot sweep initially never completed - the
  general-purpose phase-advance helper it shared with the continuous
  waveforms always wrapped at 1.0, so a triggered sweep's own completion
  check (`phase >= 1.0`) could never fire; it kept silently wrapping and
  cycling forever instead of sweeping once and holding. Fixed by giving
  one-shot sweeps their own non-wrapping phase advance.
- The toggle waveforms' direction tracking flipped its own "which way to
  sweep" flag *before* using it for the sweep that flip was supposed to
  describe, so the first trigger swept the wrong direction. Fixed by
  computing the sweep's `from`/`to` endpoints directly from the LFO's own
  currently-held value at trigger time, rather than maintaining a
  separately-flipped direction flag that could fall out of sync with it.

## Tempo sync

Every delay and LFO has its own BPM-subdivision parameter (0-96 in 1/24
units) alongside its raw ms/Hz value, matching the manual's own worked
example ("To get a quarter note delay, the delay BPM should be set to
24/24... An 8th note delay, 12/24... a setting of 8/24 will give quarter
note triplets") and its own stated fallback rule ("When strict beat per
minute control... is desired, [the raw parameter] should be set to
zero"). Implemented as literally described: the tempo contribution
(`beats/24 * 60000/BPM` for delays in ms, or its Hz reciprocal for LFOs)
adds on top of the raw parameter rather than replacing it, with the raw
parameter set to 0 for pure BPM-locked timing per the manual's own
convention.

## Known simplifications

- **No MIDI Damper Pedal BPM tap-in** - the manual's own default routing
  for "tapping in" a tempo via MIDI is dropped, since no consumer in
  this project implements MIDI input.
- **No HS322/HS395 expansion-board delay times** (11s/32s max) - this
  project has no equivalent hardware-option concept, so delays use the
  standard H3000's own 700ms maximum.
- **Filter Q (1-1000) is linearly mapped onto `StateVariableFilter`'s own
  0..1 Q range** - the manual doesn't specify the underlying curve.
- **Mod Knobs have no special smoothing behavior** - the manual's own
  stated purpose for them ("converts the digital, quantized nature of
  parameter entry on the H3000 to a smoother, analog-style control") is
  moot here, since every parameter in this project's own three consumers
  is already a continuous float, not a quantized hardware knob step.

## Status

Verified via `MultiWaveLFO`, `EnvelopeDucker`, and `ModFactoryOne` Block
smoke tests:
1. `MultiWaveLFO`: a 10Hz sine shows ~10 peaks per second; a square wave
   never leaves +/-1; a triggered waveform stays at its resting value
   until the input crosses threshold, then sweeps exactly once and holds
   stably at the end value; a toggle waveform alternates sweeping to +1
   then -1 on successive triggers.
2. `EnvelopeDucker`: the envelope output tracks a step input's rise and
   fall at the configured attack/decay rates; the ducker output stays
   near 1.0 below threshold and drops clearly above it, with a higher
   ratio ducking harder for the same input than a lower one.
3. `ModFactoryOne`: the default patch produces finite output for a test
   tone; a straight-through patch (Left Input -> Mixer 1 -> both
   outputs) reproduces the input exactly, one sample later (the patch
   matrix's own deliberate one-sample latency); a Delay 1 BPM setting of
   24/24 at 120 BPM produces an impulse response peak within 2 samples
   of the exact expected 500ms (one quarter note); an Amplitude
   Modulator with Amount=100% patched to Fullscale passes audio through
   at unity, and patched to Zero mutes it completely - both exactly
   matching the manual's own stated behavior.

`dsp_host_render mod_factory_one` renders the module doc's own suggested
example patch - "the output of Knob 1 would be patched to the modulation
input of delay 1... a very simple flanger" (using LFO 1 instead of a mod
knob, matching this Block's own default patch) - with finite output and
a printed RMS curve. `patches/eventide/mod_factory_one/` (the module
doc's manual-flanger patch wired by default; 3-knob mapping: Left = LFO 1
Rate, Mid = Delay 1 Mod depth, Right = Mix; footswitch press = bypass,
hold = toggle Delay 1 Feedback between 0% and 40% for a more resonant
character) and the `EventideModFactoryOnePlugin` JUCE target both build
clean. The JUCE plugin exposes every module parameter plus all 28 patch
destinations as source dropdowns (matching Patch Factory's own plugin
approach at a larger scale), verified by launching the actual Standalone
build headlessly (Xvfb) and confirming both the parameter list and the
architecture diagram render correctly. As with the other patches in this
archive, the ARM cross-toolchain isn't available in this sandbox, so the
Patch adapter is verified via `make -n` plus a host-compiler build under
`-fno-exceptions -fno-rtti`, not an actual `.endl` build.
