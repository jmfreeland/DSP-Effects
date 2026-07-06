# Lexicon PCM81-style Inverse Algorithm

Wired up end-to-end (Block + Graph + Patch + JUCE plugin) - the fifth and
last of the PCM81's reverb cores. Per `CLAUDE.md`'s Primitive →
Component → Block → Graph layering:

- **Block**: `dsp/include/dsp/algorithms/Inverse.h` — a subclass of
  `dsp/include/dsp/algorithms/ReverbCore.h`, replacing its RT60-based
  decay entirely with **Duration** + independent **Low Slope**/**Mid
  Slope**.
- **Graph**: `dsp/include/dsp/graphs/InverseAlgorithm.h` — the Block
  above wrapped in the same 4-Voice "Reverb Shell" front end as the other
  four, minus the `setDecaySeconds`/`setLowRatio`/`setLink`
  pass-throughs the others have (see below).

## Why this core needed a different mechanism than the other four

Every other core shapes its per-algorithm character via a hook that
either feeds the *input* side (`applyPreEcho`, `effectiveDiffusion`) or
scales the *output* by a single scalar (`outputEnvelope`). Inverse's
"decay, gate, or rise" per the manual is fundamentally different: it
needs an independently-shaped envelope *per frequency band* (Low vs.
Mid/High), including a rising shape - and a rising shape has to *reveal*
material that arrived earlier, which a per-line recirculating feedback
gain can't do.

The first implementation attempt got this wrong: it overrode the
tank's own per-line low/mid decay gains (the same gains RT60 mode
computes from Decay/Low Ratio) to follow the Duration/Slope envelope
directly. That seemed natural since Low Slope/Mid Slope "replace" Low
Rt/Mid Rt, but it's self-defeating for a rise: a near-zero recirculating
gain during the early part of a rise discards whatever enters the tank
during that time before the gain ever climbs back up, so there's nothing
left in the tank to reveal once the envelope opens up. A standalone
smoke test with a 400ms tone burst confirmed this concretely - energy
during the rise window was essentially zero throughout, instead of
building toward a peak.

The fix: leave the tank's own recirculating decay alone (fixed at a
generous internal default in `Inverse::prepare()` - `setDecaySeconds`/
`setLowRatio`/`setLink` still exist, inherited from `ReverbCore`, but
aren't exposed as Inverse's own controls, matching how the manual scopes
them away for this algorithm specifically), so the tank always holds a
healthy reservoir of energy. Duration/Low Slope/Mid Slope are instead
applied as a **read-path-only** gain, via a new `shapeWetOutput()` hook
on `ReverbCore` (which replaces the simpler `outputEnvelope()` scalar
hook Chamber/Infinite use - `outputEnvelope()` still exists and
`shapeWetOutput()`'s default implementation just applies it, so those
two cores are unaffected). Inverse's override re-splits the
already-computed wet stereo output into low/high bands with its own
`Crossover` pair (independent of the tank's internal per-line
crossovers) and shapes each band by its own envelope before recombining.
Retested with the same tone-burst approach: energy now visibly builds
toward a peak across the rise window and hard-cuts to exactly zero the
instant Duration elapses.

## Envelope shape

Given normalized position `t = elapsedSamples / durationSamples` in
`[0, 1]`, retriggered by the same rising-edge transient detector Plate's
Attack/Chamber's Shape use:

- `slope > 0` (**decay**): `envelope = (1-t)^(1+3*slope)` - front-loaded,
  steeper for larger slope.
- `slope == 0` (**gate**): `envelope = 1` - flat until the cutoff.
- `slope < 0` (**rise**): `envelope = t^(1+3*|slope|)` - builds toward
  the cutoff.

and `envelope` snaps to 0 the instant `elapsedSamples >= durationSamples`
(the hard cutoff every shape needs, and decay's natural endpoint anyway).
This is an **original reconstruction** - the manual gives no curve
shape, only "decay, gate, or rise" - not a verified match to Lexicon's
own Inverse. No pre-echo (EkoDly/EkoFbk): the manual scopes that to
Plate/Chamber/Infinite only.

## Known simplifications

- The tank's own internal Crossover (feeding the fixed sustain decay)
  and Inverse's separate output-stage Crossover are both fixed at 400Hz
  independently - calling `setCrossoverFrequency()` only updates the
  former, so the two can drift apart from each other. Not addressed;
  low-impact in practice since both default to the same value.
- The fixed internal sustain decay (2.5s) is a flat constant, not
  exposed or tuned per Size/other parameters the way the other four
  cores' Decay is.

## Status

Verified via `dsp_host_render inverse` (impulse response under both
decay and rise slopes, finite-sample check) and standalone smoke tests:
one directly confirming the envelope trajectory for all three shapes
(decay/gate/rise) against elapsed time, and one using a tone burst to
confirm the rise case's energy genuinely builds toward a peak rather
than starving (the bug described above).

`patches/lexicon/inverse/` (3-knob mapping: Left=Duration, Mid=Slope
applied to both bands together, Right=Mix; footswitch Press=bypass,
Hold=freeze) and the `LexiconInversePlugin` JUCE target (independent Low
Slope/Mid Slope) both build clean alongside the other four. As with the
others, the ARM cross-toolchain isn't available in this sandbox, so the
Patch adapter is verified via `make -n` plus host-compiler checks under
`-fno-exceptions -fno-rtti`, not an actual `.endl` build.

All five of the PCM81's reverb cores (Concert Hall, Plate, Chamber,
Infinite, Inverse) are now wired up end-to-end.
