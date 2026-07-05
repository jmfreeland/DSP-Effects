# DSP-Effects

An archive of vintage-DSP-inspired audio effects: high-quality, reusable
building blocks recreating the character of classic hardware units
(starting with the Lexicon PCM81 and Eventide H3000), shipped as patches
for the [Polyend Endless](https://polyend.com/endless/) pedal and as a
native/plugin-testable DSP library.

Algorithms here are built **in the spirit of** the original hardware's
topology and character — not reverse-engineered or claimed bit-exact.

See [CLAUDE.md](CLAUDE.md) for architecture and build commands, and
[docs/](docs/) for per-algorithm design notes.

## Quick start

```
cmake -S . -B build
cmake --build build -j
./build/host/dsp_host_render concert_hall --out=out
```

Renders an impulse response and a test-tone burst through the current
algorithm to `out/*.wav` and prints a decay curve.
