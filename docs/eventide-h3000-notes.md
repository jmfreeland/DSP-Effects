# Eventide H3000 — reference notes

Source: Eventide H3000 Ultra-Harmonizer Service Manual (First Printing,
October 1989), plus the H3000 Ultra-Harmonizer Instruction Manual (First
Printing March 1992, models combined edition January 1996) - the *user*
manual this doc's "Open item" was waiting on, now partially in hand (an
excerpt covering the front/rear panel, running the unit, MIDI, and
parameter modulation, but not yet the per-algorithm parameter pages -
"The Algorithms" section starts at its page 45 with Algorithm 100,
Diatonic Shift, which is just past where the current excerpt ends).
These are hardware/architecture facts from the primary sources, kept here
to ground later "H3000-style" algorithm work — they are not, by
themselves, a DSP algorithm (neither manual documents the PEL firmware,
which isn't public).

## Architecture (as documented)

- Three identical **Processing Elements ("PELs")**, each a Texas
  Instruments **TMS32010** DSP, sharing one global bus along with the A/D,
  D/A, host interface, and delay memory. All three run in lockstep
  (hardware-synchronized clocks; the manual repeatedly stresses that a
  single dropped/lagging cycle across the three "can completely destroy
  the audio at the output").
- **44.1kHz** input sampling, 16-bit. PEL clock is 18.3MHz -> 218ns
  instruction time -> **104 single-cycle instructions per PEL per sample
  period** (22us). That tiny compute budget per sample, times three
  PELs, is the whole DSP power of the box.
- **64K-word delay memory** shared by all PELs over the global bus — the
  substrate for every delay-based effect (pitch shift, chorus, delay,
  reverb-ish diffusion). Max documented delay: **1.5 seconds**.
- **Variable-rate D/A**: output sample rate is not fixed at 44.1kHz — a
  pair of frequency synthesizers can run the D/A anywhere from 22kHz to
  88kHz. This is the hardware trick behind the pitch shifter: rather than
  (only) doing digital resampling/interpolation, the box can literally
  re-clock the output DAC to a different rate than the input ADC, "in
  order to allow pitch shifting without the need for digital
  decimation/interpolation filters."
- Inter-PEL communication happens via three 16-bit **"mailbox"**
  registers — the primary way audio data moves from PEL to PEL (e.g. a
  pitch-shifted signal computed on one PEL being handed to another for
  further processing).
- Documented pitch range: **3 octaves up, 3 octaves down** - corrected
  from an earlier read of the service manual as "1 octave up, 2 octaves
  down"; the Instruction Manual's own Specifications page states plainly
  "H3000: 3 octaves up, 3 octaves down," and as the actual product
  spec sheet it supersedes the earlier figure. Frequency response
  5Hz-20kHz, distortion 0.01% (0.007% typical) at 1kHz, 0dB shift, full
  level. Also newly confirmed: 16-bit resolution at 44.1kHz (matches the
  service manual), and delay "up to 23.7 seconds" in the general spec -
  this is likely describing memory-expanded family members (the
  Instruction Manual's own product list separately calls the base
  pitch-shift/delay memory "Long Digiplex: a 1.5 second delay," matching
  the service manual's 64K-word/1.5s figure above), not a correction to
  it - flagged rather than reconciled further without the per-algorithm
  pages.
- The factory/default program after an OS reset is **program 100,
  "DIATONIC SHIFT"** — as close to a canonical statement as exists that
  diatonic-aware pitch shifting is the H3000's signature algorithm.

## Implication for our implementation

We don't have the PEL firmware, so the actual per-sample pitch-shift
algorithm (window/crossfade shape, number of taps, diatonic quantization
table) is our own design, built in the spirit of the hardware constraints
above: a delay-line-based pitch shifter (not FFT/phase-vocoder — the
100-instruction/sample budget on a 1989 fixed-point DSP rules that out),
diatonic scale-quantized shift amount, and modest polyphony (three PELs
total, often split across shift + mix + a second voice for MicroPitch's
detuned pair).

## Status

The PCM81 User Guide did turn up and all five of that box's reverb cores
are now built (see docs/lexicon-pcm81-reference.md). Stage 1 work on this
box has now started too: Diatonic Shift (the factory default program
named above) is built end-to-end, now including genuine real-time
monophonic pitch tracking - see docs/eventide-diatonic-shift.md for the
full history (a first version without pitch tracking, why that turned out
to be a real gap rather than a minor simplification, and the rebuild).

A second H3000 Instruction Manual excerpt reached "Algorithm 100 -
Diatonic Shift" itself (p.45-47) - the primary source this doc's "Open
item" was waiting on. That page's block diagram and parameter list are
now the direct basis for the Block's topology (mono-in, shared Delay +
Pitch Tracker, independent Left/Right Voice generators) and its
`HarmonicInterval` parameter set, rather than an original design guessed
from the service manual's general architecture notes alone. Remaining
gaps against that page (custom Scale 1/Scale 2 tables, Source
polyphonic/solo tracking tuning) are documented in
docs/eventide-diatonic-shift.md rather than fixed silently.

## Open item

The rest of the H3000 Instruction Manual (algorithms 101 onward - Layered
Shift, Dual Shift, Stereo Shift, Reverse Shift, Swept Combs, Swept Reverb,
Reverb Factory, Ultra-Tap, and more, through at least Band Delay at
p.92-94) has been read for general reference but none of those algorithms
are built yet. Revisit this doc's primary-source grounding if/when this
archive expands to a second H3000 algorithm.
