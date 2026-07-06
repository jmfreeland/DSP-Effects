# Declarative algorithm schema: topology, implemented

Idea: describe each engine's topology and parameters in a markup/schema
format (e.g. YAML or JSON) alongside its hand-written C++, so the same
description can drive documentation, visualization, a generic plugin UI,
and AI tooling reasoning about the algorithm — without re-deriving that
structure from source or primary-source PDFs each time.

**Update:** the topology half of tier 1 (below) is now built, once 5
reverb cores existed to show real variety rather than one shape -
`dsp/include/dsp/schema/AlgorithmSchema.h` (the struct) and
`ReverbCoreSchemas.h` (one instance per core), driving the JUCE plugins'
"Show Architecture" button (`plugin/source/ArchitectureView.*` renders
the diagram, `plugin/source/LoomPluginEditor.*` is the shared editor
shell every plugin now uses). See that header's doc comment for why it's
plain C++ (spans over static arrays) rather than an external YAML/JSON
file: it's consumed entirely by C++, so this keeps it a single compiled,
type-checked source of truth with no parser dependency - a deliberate
narrowing of the original idea below, not a rejection of it.

Not yet done: the *parameter* half (name/range/default/unit/setter, to
drive a generic cross-algorithm UI) - only topology exists so far. And
the schema is still hand/AI-authored prose-derived, not generated from
or generating the C++, per the original tier-1 framing.

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

**Original recommendation (done):** pursue tier 1, but not yet —
designing the schema's shape off a single example (Concert Hall) risks
baking in the wrong abstraction. Worth revisiting once 2-3 more
algorithms exist (e.g. a second reverb core plus the first H3000
pitch-shifter primitive) so the schema reflects real variety in topology
and parameter types rather than one algorithm's specific shape.

**Next step, if picked back up:** extend the schema with the parameter
list (not just topology) so a generic cross-algorithm "Loom" UI (the
future-direction note in `CLAUDE.md`) doesn't need per-plugin
`createParameterLayout()` code either - probably worth waiting for the
H3000's first algorithm to exist first, same reasoning as above, since
its parameter *types* (pitch ratios, mailbox routing) will look nothing
like a reverb's.
