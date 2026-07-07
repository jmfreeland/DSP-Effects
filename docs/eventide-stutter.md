# Eventide H3000-style Stutter

Stage 1 (functional): the thirteenth Eventide H3000 algorithm in this
archive, Algorithm 112 per the Instruction Manual's own numbering (right
after Patch Factory, Algorithm 111 - see `docs/eventide-h3000-notes.md`
and `docs/eventide-patch-factory.md`). Per `CLAUDE.md`'s Primitive →
Component → Block → Graph layering:

- **New Primitive**: `dsp/include/dsp/StutterCapture.h` - the manual's
  own named "Stutter Control" block: continuously records its input,
  and on `trigger()` freezes recording and replays the most recently
  captured window forward, repeated a settable number of times, before
  resuming live passthrough.
- **Block**: `dsp/include/dsp/algorithms/Stutter.h` - per channel, a
  `PitchShifter` (base Coarse/Fine cents plus whichever sweep
  generator(s) target that channel) feeds a `DelayLine`
  (Delay+Feedback) into a `StutterCapture`; two independent sweep
  generators and an Auto trigger sequencer sit alongside.
- **Graph**: `dsp/include/dsp/graphs/StutterAlgorithm.h` - the Block
  plus independent Left/Right input trim.

## Why this algorithm, thirteenth

Straight numeric order. Per the manual: "The Stutter algorithm is used
to create that popular st..st..stutter sound - in real-time, without the
need for a sampler or cumbersome digital delay acrobatics." Mechanically
this is a genuinely new idea in this archive - not a delay/reverb/pitch-
shift variant, but a *capture-and-loop-on-trigger* effect, needing the
new `StutterCapture` primitive (freeze recording, replay the frozen
window exactly on each pass, since not writing during the stutter is
what keeps repeats bit-identical rather than drifting).

## Scoping the manual's second patch matrix

Patch Factory (#111, the previous algorithm) already required building
one general routing/patch-matrix engine for this archive. Stutter's own
manual page layers a *second*, narrower patch matrix on top of its
signal path: 4 trigger keys, each patchable to two of about 15
stutter/sweep/random-pitch variants (with independent Left/Right/Both
targeting per variant). Building a full second general patch matrix for
one narrower purpose would mostly duplicate Patch Factory's own
machinery rather than add new DSP substance, so this Block instead
exposes a **fixed, representative subset directly as trigger methods**:
`triggerStutter1()`/`triggerStutter2()` (the two Length/Count presets),
and per sweep generator, `triggerSweepUp()`/`triggerSweepDown()`/
`triggerRandomPitch()` - eight trigger methods total, each firing on
both channels at once (matching the manual's "l&r" trigger-list variants,
the most commonly useful case) rather than the full per-channel
combinatorics. `setSweepTarget1()`/`setSweepTarget2()` (None/Left/
Right/Both) still let either sweep generator drive either channel
independently, keeping the "two generators, two channels, flexible
routing" character without a second full matrix. See "Known
simplifications" below.

## Sweep generators: one-shot ramps, not continuous LFOs

Per the manual's Sweep Menu, each generator has an Up rate/max and a Dn
rate/min, plus a random-pitch max. This Block treats a sweep trigger as
a one-shot gesture: `triggerSweepUp()` resets the generator's value to 0
cents and ramps it toward `upMax` at a rate-derived speed, holding once
it arrives; `triggerSweepDown()` mirrors this toward `dnMin`;
`triggerRandomPitch()` jumps instantly to a random value within
±`randMax` and holds. A fresh trigger of any kind restarts the gesture
from 0 (or jumps again, for random). This is a deliberate, documented
simplification rather than a guess at continuously-cycling LFO behavior
the manual's own "the effect may consist of... a pitch sweep" phrasing
doesn't actually specify.

## Auto sequencer

`setAuto(true)` engages a simple internal sequencer (matching #4-6:
Auto On/Off, Speed, Program) that fires a trigger at a Speed-derived
interval (lerp from ~2s at Speed=0 down to ~50ms at Speed=100, per the
manual's "At a setting of 0, triggers occur very infrequently; at 100
triggers occur constantly"), choosing among the eight trigger methods
per the selected Program: `kTotalRandom` (any of the 8), `kRandomSweep`
(the 4 sweep-up/down actions only), `kRandomPitch` (the 2 random-pitch
actions only), `kJustStutter` (the 2 stutter actions only) - matching
the manual's own four Program names exactly.

## Known simplifications

- **No full per-channel l/r/l&r trigger-list combinatorics** - see
  "Scoping" above; every trigger method here fires both channels.
- **Sweep gestures are one-shot ramps that hold at their target**, not
  continuously-cycling LFOs - the manual doesn't specify auto-reset
  behavior once a sweep reaches its max/min.
- **No Deglitch (#41-43: Low/High Note, Source) parameters** - these
  tune the underlying `PitchShifter`'s internal grain/tracking
  optimization on real hardware; this archive's `PitchShifter` doesn't
  expose an equivalent tunable, matching the same simplification already
  made for Diatonic Shift/Patch Factory's own Deglitch parameters.

## Status

Verified via:
1. `StutterCapture` primitive smoke test: idle passthrough returns the
   live input exactly; a triggered 10-sample/3-pass stutter replays the
   captured window (the 10 samples immediately preceding the trigger)
   identically on all 3 passes, confirmed sample-for-sample; passthrough
   resumes with fresh live input immediately after the stutter ends.
2. `Stutter` Block smoke tests: idle passthrough preserves signal energy
   within a loose tolerance (Coarse/Fine=0, Delay=0, Feedback=0);
   triggering a stutter genuinely overrides live input (none of the
   stuttered output samples equal the distinctive live-input value fed
   during the stutter); triggering Sweep Up 1 measurably changes the
   output versus a non-swept render of the same input; Auto mode at
   Speed=100 with Program=Just Stutter runs 2 real-time seconds without
   producing non-finite output.
3. `dsp_host_render stutter` renders a tone playing normally for 1s,
   then triggered into a stutter for the second second, with finite
   output throughout.

`patches/eventide/stutter/` (3-knob mapping: Left = Length 1, Mid =
Count 1, Right = shared Mix; footswitch press = bypass, hold = manually
fire Trigger Stutter 1 - the algorithm's core performance gesture) and
the `EventideStutterPlugin` JUCE target both build clean. The JUCE
plugin exposes the full parameter set plus all eight triggers as
self-resetting momentary checkboxes (checking one fires the
corresponding `Stutter::trigger*()` call in `processBlock()`, then the
processor immediately resets the checkbox to unchecked, so each click
behaves as a one-shot button rather than a persistent toggle) - reusing
`LoomPluginEditor`'s existing generic bool-parameter rendering rather
than adding new editor machinery. Verified by launching the actual
Standalone build headlessly (Xvfb) and confirming both the parameter
list (all continuous/choice parameters plus all 8 trigger checkboxes)
and the architecture diagram render correctly. As with the other patches
in this archive, the ARM cross-toolchain isn't available in this
sandbox, so the Patch adapter is verified via `make -n` plus a
host-compiler build under `-fno-exceptions -fno-rtti`, not an actual
`.endl` build.
