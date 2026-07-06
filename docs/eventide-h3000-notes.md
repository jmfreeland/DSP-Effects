# Eventide H3000 — reference notes

Source: Eventide H3000 Ultra-Harmonizer Service Manual (First Printing,
October 1989). These are hardware/architecture facts from the primary
source, kept here to ground later "H3000-style" algorithm work — they are
not, by themselves, a DSP algorithm (the service manual documents the
hardware, not the PEL firmware, which isn't public).

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
- Documented pitch range: **1 octave up, 2 octaves down**. Frequency
  response 5Hz-20kHz, distortion 0.01% (0.007% typical) at 1kHz, 0dB
  shift, full level.
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
per-note-correct diatonic harmony) and no H3000-equivalent user/parameter
manual to verify a front-end against the way the PCM81 one could be.

## Open item

Still waiting on an H3000 *user* guide (this doc is built from the
*service* manual - hardware architecture, not the UI/parameter surface)
to check Diatonic Shift's front end and any other H3000 algorithm's
parameter set against, the way the PCM81 User Guide let us verify the
Lexicon side's control surfaces directly.
