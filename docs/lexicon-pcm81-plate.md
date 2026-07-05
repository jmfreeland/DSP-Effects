# Lexicon PCM81-style Plate Algorithm

Block-tier only so far — no Graph/Patch/JUCE plugin yet (see "Status"
below). Per `CLAUDE.md`'s Primitive → Component → Block → Graph layering:

- **Block**: `dsp/include/dsp/algorithms/Plate.h` — a subclass of the
  shared `dsp/include/dsp/algorithms/ReverbCore.h` (see
  `docs/lexicon-pcm81-hall.md` for that shared topology), adding the two
  things `docs/lexicon-pcm81-reference.md` calls out as Plate-specific:
  **Attack** and a recirculating **EkoDly/EkoFbk** pre-echo.

## What Plate adds on top of ReverbCore

- **Pre-echo (`setEkoDelaySeconds`/`setEkoFeedback`, L/R independent, up
  to 1.2s delay)**: a plain `dsp::Comb` per channel, applied to the raw
  L/R input (via the `applyPreEcho()` hook) before it's summed to mono
  and fed into PreDelay/Diffusion. Concert Hall/Inverse leave this at its
  inert default (0 feedback); Plate turns it on by default, matching the
  manual's "pre-echo w/ feedback (Plate/Chamber/Infinite only)."
- **Attack (`setAttack`, 0..1)**: sharpness of the initial response, first
  ~50ms only, per the manual's one-line description — the exact shaping
  curve is an **original reconstruction**, not a verified match. A
  transient detector (input level vs. a slow-following envelope, rising-
  edge triggered so a sustained loud passage doesn't keep retriggering)
  fires a 50ms `LinearRamp` on each new onset. While that ramp is active,
  it pulls the effective Diffusion coefficient (via the
  `effectiveDiffusion()` hook) down toward a floor (15% of the set
  Diffusion amount) and releases back to the set amount over the window.
  `Attack=1` disables the dip entirely (full density immediately - a
  "sharp" onset); `Attack=0` makes the dip deepest (density visibly
  builds over the 50ms window - a "soft" onset).
- Default tuning differs from Concert Hall to match the manual's
  character ("high initial diffusion, bright; good on percussion"):
  higher default Diffusion (0.85 vs. 0.6) and less damping (brighter).

## Status

Verified at the Block level only, via `dsp_host_render plate` (impulse
response, decay curve, finite-sample check) and a standalone smoke test
confirming the Attack mechanism's effective-diffusion trajectory matches
the design (dips to the floor at onset, releases linearly back to the
set amount by exactly 50ms, holds afterward; `Attack=1` shows no dip at
all). Not yet wired into a Graph (the 4-Voice "Reverb Shell" front end
that `ConcertHallAlgorithm.h` provides), a Polyend `Patch` adapter, or the
JUCE plugin — that reuse is the point of `ReverbCore.h` existing, but
doing it is follow-up work, along with Chamber, Inverse, and Infinite.
