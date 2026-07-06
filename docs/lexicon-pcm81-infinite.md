# Lexicon PCM81-style Infinite Algorithm

Wired up end-to-end (Block + Graph + Patch + JUCE plugin). Per
`CLAUDE.md`'s Primitive → Component → Block → Graph layering:

- **Block**: `dsp/include/dsp/algorithms/Infinite.h` — a subclass of
  `dsp/include/dsp/algorithms/Chamber.h` (see
  `docs/lexicon-pcm81-chamber.md`), adding nothing of its own beyond
  different default tuning (a longer default decay). Per the manual,
  Infinite *is* "Chamber + a freeze switch" — and the freeze mechanism
  (`setFrozen()`) already existed generically on `ReverbCore` before this
  algorithm was built (added early for Concert Hall's footswitch-hold
  behavior), so this is the cheapest of the five cores.
- **Graph**: `dsp/include/dsp/graphs/InfiniteAlgorithm.h` — the Block
  above wrapped in the same 4-Voice "Reverb Shell" front end as the other
  three.

## What building this surfaced

Building Infinite is what actually exercised freeze long enough to find
a real bug in it: `ReverbCore::processSample()` was still running the Rt
HC damping filter on every tank tap even while frozen, so a "frozen" tail
measurably decayed over a few seconds instead of ringing indefinitely —
only the RT60 low/mid decay gains were being pushed to near-unity, not
the damping filter. Fixed by bypassing the damping filter entirely while
frozen (see the `frozen_` branch around `damping_[i].process(...)` in
`ReverbCore::processSample()`). Verified via a standalone smoke test:
freezing an already-decaying tail and comparing its energy in a 100ms
window right after freezing vs. ~4 seconds later.

## Known simplifications

- "Reverb input ramps off" in the manual implies a smooth fade when
  freeze engages; the current implementation gates the diffuser input
  instantly (an existing simplification, not new to Infinite), so
  toggling freeze can click faintly.
- Even after the damping fix, Spin/Chorus's continuous delay-length
  modulation costs a small amount of energy per pass — linear
  interpolation between continuously wobbling read positions is
  inherently lossy. With Spin/Chorus at their defaults, a frozen tail
  loses roughly 25-30% of its energy over 4 seconds; with Spin/Chorus
  rolled back to 0, that drops to roughly 10% over the same window.
  A genuinely lossless "forever" freeze isn't automatically enforced —
  the user (or a smarter freeze that could roll modulation back
  automatically, not implemented) has to roll Spin/Chorus back for the
  closest approximation to true infinite hold.

## Status

Verified via `dsp_host_render infinite` (impulse response, freezing 1s
in, confirming the tail holds near-steady afterward rather than
continuing to decay to silence) and the standalone freeze-energy smoke
test described above.

`patches/lexicon/infinite/` (3-knob mapping: Left=Decay, Mid=Shape,
Right=Mix; footswitch Press=bypass, Hold=freeze — the footswitch-hold
*is* the "Infinite" switch) and the `LexiconInfinitePlugin` JUCE target
both build clean alongside the other three. As with Plate/Chamber, the
ARM cross-toolchain isn't available in this sandbox, so the Patch
adapter is verified via `make -n` plus host-compiler checks under
`-fno-exceptions -fno-rtti`, not an actual `.endl` build.

Inverse is the only reverb core still open — see
`docs/lexicon-pcm81-reference.md`. Its envelope-driven decay (Duration,
Low Slope/Mid Slope replacing Low Rt/Mid Rt) is structurally different
from the other four and will need a new decay-gain computation hook on
`ReverbCore` rather than reusing `rt60ToGain`.
