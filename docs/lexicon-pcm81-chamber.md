# Lexicon PCM81-style Chamber Algorithm

Wired up end-to-end (Block + Graph + Patch + JUCE plugin). Per
`CLAUDE.md`'s Primitive → Component → Block → Graph layering:

- **Block**: `dsp/include/dsp/algorithms/Chamber.h` — a subclass of the
  shared `dsp/include/dsp/algorithms/ReverbCore.h` (see
  `docs/lexicon-pcm81-hall.md` for that shared topology), adding the
  things `docs/lexicon-pcm81-reference.md` calls out as
  Chamber-specific: **Shape**, **Spread**, and a recirculating
  **EkoDly/EkoFbk** pre-echo (shared mechanism with Plate, different
  default tuning).
- **Graph**: `dsp/include/dsp/graphs/ChamberAlgorithm.h` — the Block
  above wrapped in the same 4-Voice "Reverb Shell" front end as
  `ConcertHallAlgorithm.h`/`PlateAlgorithm.h`, plus pass-throughs for
  Shape/Spread/EkoDly/EkoFbk.

## What Chamber adds on top of ReverbCore

- **Pre-echo (`setEkoDelaySeconds`/`setEkoFeedback`)**: identical
  mechanism to Plate's (see `docs/lexicon-pcm81-plate.md`), tuned more
  subtly here to match Chamber's "even, dimensionless" character rather
  than Plate's percussive one.
- **Shape + Spread (`setShape`/`setSpread`, 0..1 each)**: "envelope
  contour" and "sustain" per the manual's one-line description — the
  exact shaping is an **original reconstruction**, not a verified match.
  A rising-edge transient detector (same technique as Plate's Attack)
  retriggers a two-stage gain envelope applied to the final wet output
  via the `outputEnvelope()` hook:
  - **Attack stage**: on a new onset, the envelope ramps from its
    current value up to a peak of `1 + Shape * 1.5` over a duration that
    also grows with Shape (5ms..300ms) — `Shape=0` produces no
    perceptible swell (peak stays at 1.0, i.e. a no-op); `Shape=1`
    produces a pronounced, slower swell.
  - **Release stage**: once the attack ramp completes, the envelope
    relaxes back down to unity over a duration set by Spread
    (50ms..3s) — during that relax, the tank's own ongoing exponential
    decay is heard at an elevated level, reading as extended sustain.
    `Spread=0` snaps back quickly; `Spread=1` lingers for seconds.
- Default tuning is more moderate than Plate/Concert Hall, matching the
  manual's character ("even… little color change over decay"): Diffusion
  0.5, Damping 0.4 (vs. Plate's 0.85/0.25).

## Status

Block level: verified via `dsp_host_render chamber` (impulse response,
decay curve, finite-sample check) and a standalone smoke test confirming
both envelope stages independently — Shape controls the attack
peak/duration, Spread controls the release duration, with Spread's effect
only visible once the test window extends past the (Shape-dependent)
attack phase.

End-to-end: `patches/lexicon/chamber/` (3-knob mapping: Left=Decay,
Mid=Shape, Right=Mix — Spread stays at the Graph default since the
hardware only has 3 knobs; footswitch Press=bypass, Hold=freeze) and the
`LexiconChamberPlugin` JUCE target both build clean alongside Concert
Hall and Plate. As with Plate, the ARM cross-toolchain isn't available in
this sandbox, so the Patch adapter is verified via `make -n` (build
graph) plus compiling `PatchImpl.cpp` on the host with
`-fno-exceptions -fno-rtti`, not an actual `.endl` build.

Inverse and Infinite are still open. Infinite in particular should be
close to free: it's documented as "Chamber + a freeze switch," and the
freeze mechanism (`setFrozen`) already exists generically on `ReverbCore`
(added for Concert Hall's footswitch-hold behavior, never removed).
