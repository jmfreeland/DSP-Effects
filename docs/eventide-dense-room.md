# Eventide H3000-style Dense Room

Stage 1 (functional): the fifteenth Eventide H3000 algorithm in this
archive, Algorithm 114 per the Instruction Manual's own numbering (right
after Timesqueeze, Algorithm 113 - see `docs/eventide-h3000-notes.md`
and `docs/eventide-timesqueeze.md`). Per `CLAUDE.md`'s Primitive →
Component → Block → Graph layering:

- **Primitives reused**: `DelayLine`, `DiffuserChain<3>`, `rt60ToGain()`,
  `OnePoleLowpass`, `householderMix()` - all already built for the PCM81
  side and/or Reverb Factory (Algorithm 107). No new primitives.
- **Block**: `dsp/include/dsp/algorithms/DenseRoom.h` - PreDelay ->
  3-stage Diffusion -> a 6-line Householder-mixed tank with independent
  per-line Delay/Pan/Level, single Rev Time, simple High Cut.
- **Graph**: `dsp/include/dsp/graphs/DenseRoomAlgorithm.h` - the Block
  plus input trim, mono-in.

## Why this algorithm, fifteenth, and its real lineage

Straight numeric order. Per the manual: "This algorithm offers a much
improved early response characteristic over the original 'Reverb
Factory' program... The parametric EQ of Reverb Factory has been
replaced by a simple 'high-cut' control, and the noise gate has been
removed." That's not incidental phrasing - Dense Room is a genuine,
named evolution of the same 6-line Householder-tank family as
`ReverbFactory.h` (Algorithm 107). Its own manual page (unlike Reverb
Factory's) also adds a Diffusion stage ahead of the tank and explicit
per-line Pan/Level (#21-32) rather than Reverb Factory's fixed
alternating panning, while dropping the Gate/dual-decay entirely in
favor of one Rev Time. This Block reuses `ReverbFactory`'s own core
techniques (`rt60ToGain()`-driven per-line decay, `Crossover`/
`OnePoleLowpass`-style damping, `householderMix()`) directly rather than
re-deriving them, and adds `DiffuserChain<3>` (already built for the
PCM81 side) for the new Diffusion stage.

## A real bug caught while testing: diffuser stages left at their fixed buffer-capacity delay

The first implementation allocated each of the 3 diffusion allpass
stages a buffer sized to match the tank lines' own maximum (~240ms) and
never called `Allpass`'s opt-in `setDelaySamples()` extension (built for
Ultra-Tap - see `docs/eventide-ultra-tap.md`), so each stage silently
fell back to `Allpass`'s *default* fixed delay: the buffer's own full
capacity minus one. That made the Diffusion stage itself act like a
slow, independent secondary reverb - each allpass stage recirculating
over a ~240ms period with a diffusion coefficient up to 0.75 - feeding
fresh energy into the tank on a ~200-300ms cycle regardless of the
tank's own Rev Time setting entirely. A Block smoke test comparing a
0.3s vs. 5s Rev Time at t=0.5s should have shown a huge energy gap (the
math: `10^(-3*0.5/0.3)` ≈ 1e-5 amplitude vs. `10^(-3*0.5/5)` ≈ 0.5
amplitude) but instead showed nearly identical energy for both settings
- the tell that something *other* than the tank's own decay was
sustaining the tail. Fixed by sizing the diffuser buffers to the
manual's own stated Allpass Delay range (0-5000 samples) and calling
`setStageDelaySamples()` with short, sensible defaults (337/563/809
samples) via `setAllpassDelaySamples()` - exposed as a genuine
per-stage-independent expert parameter (#15-17) rather than left at an
uncontrollable, oversized default. Re-verified: the two Rev Time
settings now diverge by roughly the expected multiple orders of
magnitude at t=0.5s.

## Position and Early Mix: two established reconstruction mechanisms, not one new guess

Neither Position (#4, "apparent listener location") nor Early Mix (#6,
"the nature of the early response... coherent vs diffuse") is specified
mechanistically by the manual - typical for this whole family, since the
real PEL firmware isn't public. Rather than inventing a new mechanism for
each, this Block reuses two techniques already built, documented, and
verified on the PCM81 side of this archive: Position blends between the
Diffusion stage's own output (an "early," less-processed signal -
matching the manual's own Block Diagram, which draws a separate "Early
Mix/Pan" path branching directly off Diffusion, parallel to the
Reverberator) and the tank's mixed output, the same early/tank balance
mechanism as `ReverbCore::setDepth()`. Early Mix blends each line's raw
(pre-Householder-mix) tap against its fully-mixed value, the same
premix/postmix mechanism as `ReverbCore::setDefinition()`.

## Known simplifications

- **Mono-in** - the manual's own Block Diagram draws only a "Left Input"
  arrow (no Right Input at all), matching the same convention already
  used for Long Digiplex/Layered Shift/Reverse Shift/etc.
- **No independent per-stage Allpass Gain 1-3** (#18-20) - all three
  Diffusion stages share one Diffusion coefficient via
  `DiffuserChain::setDiffusion()`, the same single-knob approach already
  used throughout this archive's other diffusion stages, rather than
  exposing three fully independent gains.
- **Rev Time capped at 120s internally** (JUCE/Patch UI cap at 10s for a
  practical sweep) rather than the manual's literal "infinity" - a
  standard finite-but-very-long approximation, the same one used
  elsewhere in this archive for "infinite" decay claims.

## Status

Verified via `DenseRoom` Block smoke tests:
1. An impulse produces finite, non-silent output on both channels, with
   measurable energy still present a full second later (a real
   reverberant tail, not a dry pass-through).
2. Rev Time=5.0s retains real energy at t=0.5s while Rev Time=0.3s has
   decayed to the noise floor by then - confirming Rev Time genuinely
   controls decay rate (this is the check that caught the diffuser-delay
   bug above).
3. Position=0 (front) and Position=1 (rear) produce audibly different
   output for the same impulse, confirming the early/tank balance is
   real.

`dsp_host_render dense_room` renders an impulse end to end with finite
output. `patches/eventide/dense_room/` (3-knob mapping: Left = Rev Time
0.1-10s, Mid = Size, Right = Mix; footswitch press = bypass, hold =
toggle Position between front/rear) and the `EventideDenseRoomPlugin`
JUCE target both build clean, verified by launching the actual
Standalone build headlessly (Xvfb) and confirming both the parameter
list (all 9 core parameters plus the expert Allpass Delay/per-line
Delay/Pan/Level set) and the architecture diagram render correctly. As
with the other patches in this archive, the ARM cross-toolchain isn't
available in this sandbox, so the Patch adapter is verified via
`make -n` plus a host-compiler build under `-fno-exceptions -fno-rtti`,
not an actual `.endl` build.
