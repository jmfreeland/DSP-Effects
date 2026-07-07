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
named above) is built end-to-end - see docs/eventide-diatonic-shift.md
for the Block/Graph/Patch/plugin and its known simplifications, chief
among them no real-time pitch tracking of the input (needed for *true*
per-note-correct diatonic harmony).

An H3000 Instruction Manual excerpt has now turned up too (see Source
above) - it corrected the pitch-range figure above and confirmed the
front-panel/MIDI/parameter-modulation architecture in general, but the
excerpt in hand stops one page short of "Algorithm 100 - Diatonic Shift"
itself (manual p.45), so Diatonic Shift's front end and parameter set
still aren't checked against a primary source the way the PCM81 Graphs
could be.

## Open item

Waiting on the rest of the H3000 Instruction Manual (starting at its
p.45, "The Algorithms") to check Diatonic Shift's front end and
parameter set - and any other H3000 algorithm's, should this archive
cover more than one - against a primary source the way the PCM81 User
Guide let us verify the Lexicon side's control surfaces directly.
