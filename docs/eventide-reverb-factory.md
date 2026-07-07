# Eventide H3000-style Reverb Factory

Stage 1 (functional): the eighth Eventide H3000 algorithm in this
archive, Algorithm 107 per the Instruction Manual's own numbering (the
third and last of the "six swept... six-line" family alongside Swept
Combs/Swept Reverb - see `docs/eventide-h3000-notes.md` and
`docs/eventide-swept-reverb.md`). Per `CLAUDE.md`'s Primitive →
Component → Block → Graph layering:

- **Primitives reused**: `dsp::householderMix()` (from the PCM81 tank,
  already reused by Swept Reverb), `dsp::rt60ToGain()` (from the PCM81
  side, first reuse in the Eventide family), `Crossover` (from the PCM81
  side, first reuse in the Eventide family), `OnePoleLowpass` (as an
  envelope follower and a crossfade smoother).
- **Block**: `dsp/include/dsp/algorithms/ReverbFactory.h` - a Predelay
  ahead of six *fixed* (non-swept) delay lines feeding a Householder
  network, plus a dynamics Gate (envelope follower + Threshold/Speed/
  Time) that crossfades each line's decay gain and tone between two
  independent "On"/"Off" settings.
- **Graph**: `dsp/include/dsp/graphs/ReverbFactoryAlgorithm.h` - the
  Block plus input trim.

## Why this algorithm, eighth

Direct continuation of the "six delay lines feeding a network" family
established by Swept Combs/Swept Reverb, per the manual's own
back-to-back numbering. Unlike its two predecessors, Reverb Factory's
lines are *not* swept (no Rate/Depth/random-walk modulation is mentioned
on this algorithm's own page) - the six lines are static, and the
algorithm's actual distinguishing feature is the Gate-driven dual decay/
EQ, which reuses machinery from the *Lexicon* side of this archive
(`rt60ToGain()`, `Crossover`) rather than anything H3000-specific,
continuing the cross-device-family reuse pattern Swept Reverb started
with `householderMix()`.

## The Gate mechanism

Per the manual: "The built in Gate has Response Time and Threshold
controls as well as separate parametric EQ on both the open and closed
gate. Two decay times are also provided. Softer sounds (below the gate
threshold) can have one decay time and EQ while loud sounds (above the
gate threshold) can have different decay and EQ." Implemented as
an envelope follower (`OnePoleLowpass` tracking `|input|`, response time
set by Gate Speed) compared against Gate Threshold; crossing it opens
the gate and (re-)starts a Gate Time hold counter (explicitly
re-triggerable, matching the manual's own note); once the hold expires
without re-triggering, the gate closes. The resulting open/closed state
is smoothed (a fixed ~80Hz `OnePoleLowpass`) into a continuous 0-1
crossfade amount, used to `lerp()` between the Off and On decay gains
(via `rt60ToGain()`, computed per line from that line's own actual delay
length) and EQ gains, applied *before* the Householder mix - mirroring
exactly how `dsp::algorithms::ReverbCore` already shapes its own tank
lines' decay per-band, just with the blend driven by a gate envelope
instead of a fixed per-line gain table.

Per the manual's own note - "If the gate is Disabled the reverb uses
only the Gate On EQ settings" - `setGateEnabled(false)` forces the
crossfade to the On state permanently rather than freezing wherever it
happened to be, matching that specific documented behavior exactly.

## Known simplifications

- **EQ is a single crossover point, not independent low-shelf/high-
  shelf.** The manual's Expert Mode gives each of On/Off *two* frequency/
  gain pairs (On L Freq/Low dB *and* On H Freq/Hi dB, same for Off) - a
  3-band shape (cut below L Freq, flat between, cut above H Freq). This
  Block implements one `Crossover` split per line (matching
  `ReverbCore`'s own existing 2-band low/high model exactly) with a
  single gain applied to the high band, crossfaded between On/Off - the
  low-shelf half of the manual's spec isn't implemented. A reasonable,
  documented reduction of a secondary Expert-mode refinement, not the
  algorithm's defining mechanism (the Gate + dual decay is).
- **No Glide** on Predelay/decay changes (matches the pattern already
  documented for Swept Combs/Swept Reverb).
- **Per-line Delays have no Master scale.** Unlike Swept Combs/Reverb's
  "m Delay," Reverb Factory's own manual page doesn't document a master
  Delay control - each line's Tedium value is used directly.

## Status

Verified via:
1. The `ReverbFactory` Block directly: a loud 100ms burst produces
   substantial early energy and triggers the Gate (confirmed indirectly
   via the burst-then-decay shape); a second run with the Gate disabled
   runs without producing any non-finite samples, confirming the "always
   On settings" path is stable.
2. `dsp_host_render reverb_factory` renders a gated burst end to end
   with finite output and a sensible RMS decay curve (loud onset, decay
   toward the Off-decay tail once the Gate's hold time elapses).

`patches/eventide/reverb_factory/` (3-knob mapping: Left = On Decay, Mid
= Gate Threshold, Right = Mix - Off Decay/EQ/Gate Time/Speed stay at
their built-in defaults since the hardware only has 3 knobs, the JUCE
plugin exposes the full parameter set including per-line Delay) and the
`EventideReverbFactoryPlugin` JUCE target both build clean, verified by
launching the actual Standalone build headlessly (Xvfb) and confirming
both the parameter list (including the Gate Enabled checkbox) and the
architecture diagram render correctly. As with the other patches in this
archive, the ARM cross-toolchain isn't available in this sandbox, so the
Patch adapter is verified via `make -n` plus a host-compiler build under
`-fno-exceptions -fno-rtti`, not an actual `.endl` build.
