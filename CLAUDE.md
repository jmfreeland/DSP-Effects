# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

DSP-Effects is a growing archive of vintage-DSP-inspired audio effects, built primarily as patches for the [Polyend Endless](https://polyend.com/endless/) pedal via its `FxPatchSDK` (vendored under `sdk/`), with the same DSP code also runnable natively for offline testing and (eventually) as a DAW plugin.

Roadmap, not necessarily sequential — work can interleave across stages and devices:
- **Stage 1**: replicate the core DSP algorithms of the Lexicon PCM81 and Eventide H3000 *functionally* — right topology, right building blocks, working end to end.
- **Stage 2**: refine those implementations closer to the character of the original hardware — tone-matching, nuance, quirks.
- **Stage 3**: expand beyond those two units into new tools and an archive of other great vintage DSP devices.

Algorithms here are documented as **inspired by** the topology/character of the original hardware, never claimed as reverse-engineered or bit-exact clones — the proprietary algorithms aren't public.

## Branding & versioning

The repo stays `DSP-Effects`, but the effect line itself is branded **Loom**
(the JUCE plugin's `COMPANY_NAME`/`PRODUCT_NAME`, e.g. "Loom - Lexicon
Hall") — tentative, may evolve. Versioning is repo-wide semver (`project(DSPEffects VERSION x.y.z ...)`
in the top-level `CMakeLists.txt`, pre-1.0 while the archive is young) plus
an independent per-algorithm **Stage 1/2/3** label carried in that
algorithm's `docs/*.md` — the stage label communicates DSP maturity, not
the version number. Bump the repo minor version when an algorithm reaches
Stage 1; patch for fixes/refinement within a stage.

## Architecture

Everything of DSP substance lives in `dsp/` and is shared, unmodified, across three consumers:

- `dsp/include/dsp/` — portable, header-only C++20 building blocks with **no heap allocation, no exceptions, no RTTI** (the ARM target is built with `-fno-exceptions -fno-rtti`). Current primitives: `DelayLine` (non-owning, span-based circular buffer), `Allpass` / `ModulatedAllpass` (Schroeder diffusers), `OnePoleLowpass` (feedback-path damping), `LFO`, and `FeedbackMatrix.h`'s Householder mixing matrix for FDNs.
- `dsp/include/dsp/algorithms/` — full effect engines composed from those primitives (e.g. `LexiconHall.h`), independent of any SDK. An engine exposes `prepare(sampleRate, workingBuffer)`, per-parameter setters, `process(left, right)`, and `reset()`.
- `patches/<device>/<algo>/` — thin Polyend `Patch` adapters (e.g. `patches/lexicon/hall/PatchImpl.cpp`) that own one engine, map its parameters to the pedal's 3 knobs (`endless::ParamId::kParamLeft/Mid/Right`) and 2 footswitch actions (`kLeftFootSwitchPress/Hold`), and report LED color via `Patch::Color`. Each has its own tiny `Makefile` that just sets `PATCH_NAME`/`PATCH_SOURCES`/`EXTRA_INCLUDES` and includes `sdk/Patch.mk`.
- `sdk/` — vendored upstream `FxPatchSDK` (`source/Patch.h` interface, `internal/` ABI glue and linker script) plus `sdk/Patch.mk`, the shared cross-compile Makefile logic every patch includes. Treat `internal/` as upstream boilerplate; don't hand-edit it.
- `host/` — native (host-arch) CMake target `dsp_host_render` that instantiates an engine directly (no SDK involved), renders an impulse response and a test-tone burst to WAV in `--out=<dir>` (default `out/`), and prints an RT60-style decay curve. This is the fast iteration loop before touching real hardware.
- `plugin/` — JUCE-based VST3/Standalone wrapper ("Loom", CMake `FetchContent`d JUCE 8.0.14) around the same engines, for testing in Ableton/Bitwig. Gated behind the `DSP_EFFECTS_BUILD_PLUGIN` CMake option so a normal build doesn't fetch JUCE. One `juce_add_plugin` target per algorithm (currently `LexiconHallPlugin`), each with its own `PluginProcessor` in `plugin/source/`.

Because a `Patch`'s delay lines must live in the pedal's external working buffer (`Patch::kWorkingBufferSize` = 2,400,000 floats, `Patch::kSampleRate` = 48000) rather than internal RAM, every engine's `prepare()` takes a `std::span<float>` and carves it up itself (see `LexiconHall::requiredWorkingBufferSize()` / `subspan` usage) — don't give primitives owned storage.

## Commands

Native host harness (primary day-to-day build/test loop):
```
cmake -S . -B build
cmake --build build -j
./build/host/dsp_host_render lexicon_hall --out=out
```
Exit code is non-zero if any rendered buffer contains a non-finite sample or a WAV fails to write; the decay curve it prints is the quickest sanity check that an engine's RT60/damping behaves.

Quick single-header smoke test without CMake, e.g. while iterating on a new `dsp/` primitive or engine:
```
g++ -std=c++20 -Wall -Wextra -I dsp/include <file>.cpp -o /tmp/smoketest && /tmp/smoketest
```

Cross-compiling an individual patch for the Endless (requires the [GNU Arm Embedded Toolchain](https://developer.arm.com/Tools%20and%20Software/GNU%20Toolchain); not available in this sandbox):
```
cd patches/lexicon/hall
make TOOLCHAIN=/usr/bin/arm-none-eabi-
```
Produces `build/<PATCH_NAME>_<timestamp>.endl` — copy that file to the Endless's USB drive to deploy. `make -n` from a patch directory is useful to sanity-check the build graph without a toolchain installed.

Plugin build (fetches JUCE 8.0.14 on first configure; needs `libx11-dev`, `libxrandr-dev`, `libxinerama-dev`, `libxcursor-dev`, `libxcomposite-dev`, `libxi-dev`, `libasound2-dev`, `libfreetype-dev` on Linux):
```
cmake -S . -B build-plugin -DDSP_EFFECTS_BUILD_PLUGIN=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-plugin -j -t LexiconHallPlugin_VST3
```
Produces a `.vst3` under `build-plugin/plugin/LexiconHallPlugin_artefacts/Release/VST3/`.
