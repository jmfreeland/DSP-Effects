# Future direction: a declarative algorithm schema

Idea: describe each engine's topology and parameters in a markup/schema
format (e.g. YAML or JSON) alongside its hand-written C++, so the same
description can drive documentation, visualization, a generic plugin UI,
and AI tooling reasoning about the algorithm — without re-deriving that
structure from source or primary-source PDFs each time.

## Is this achievable?

Yes. There's direct precedent: [Faust](https://faust.grame.fr/) is an
existing functional DSP language that describes signal-flow graphs of
primitives declaratively and compiles them to C++ (and other targets).
We wouldn't adopt Faust itself — this repo's engines are hand-tuned for
character and embedded constraints in a way a generic compiler output
usually isn't — but it confirms the underlying idea (a declarative
topology description carrying real DSP meaning, not just prose) holds up.

## Two tiers, and which one to actually build

1. **Lightweight schema as a second, machine-readable source of truth**
   (readily achievable, high value): a per-algorithm YAML/JSON file
   listing the primitive nodes used (from `dsp/`), the signal-flow graph
   connecting them, and the parameter list (name, range, default, unit,
   which setter it maps to). Hand-authored *alongside* the engine, not
   generated from it or generating it. This single file could then drive:
   - The generic "Loom" plugin's parameter UI (the future-direction note
     already in `CLAUDE.md`) without per-algorithm UI code.
   - Auto-rendered block diagrams (e.g. to Mermaid/Graphviz) for docs,
     replacing hand-drawn ASCII topology diagrams like the one in
     `docs/lexicon-pcm81-hall.md`.
   - A structured artifact that AI tooling (or a future contributor) can
     read directly to compare algorithms/devices, instead of re-deriving
     topology from the C++ or the original manuals every time.

2. **Full codegen from the schema** (a much bigger lift, not proposed
   yet): actually generating the DSP C++ from the markup via a small
   compiler/DSL. This risks fighting the project's own ethos of
   hand-tuned character over generic abstraction, and isn't necessary to
   get most of the benefit above — tier 1 already captures topology and
   parameters for AI/viz/UI purposes while keeping the DSP itself
   hand-written.

**Recommendation:** pursue tier 1, but not yet — designing the schema's
shape off a single example (Concert Hall) risks baking in the wrong
abstraction. Worth revisiting once 2-3 more algorithms exist (e.g. a
second reverb core plus the first H3000 pitch-shifter primitive) so the
schema reflects real variety in topology and parameter types rather than
one algorithm's specific shape.
