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

A `Stage` can also carry an optional `drillDown` pointer to a *nested*
`AlgorithmSchema` - clicking that stage in the UI navigates into a more
detailed sub-diagram (with a "< Back" breadcrumb), rather than every
stage being a dead-end label. `ReverbCoreSchemas.h`'s Tank and Diffusion
stages both drill into shared detail schemas (`tankDetailSchema()`/
`diffusionDetailSchema()`) since those internals are identical across
all 5 cores - only parameter values differ, so one detail schema each
is reused by every top-level schema rather than duplicated 5 times.

**Update 2:** the first slice of the *parameter* half is now built, as
a linkage rather than a full parameter description: `Stage` carries an
optional `parameterIds` span naming the plugin parameter (APVTS) IDs
that drive that stage. `plugin/source/LoomParametersPanel.*` (which
replaced the stock `GenericAudioProcessorEditor` in every plugin's
editor) walks the schema depth-first (stages, then each stage's
drillDown) and renders one titled knob-grid section per stage that
claims parameters - so the parameter panel reads in signal-flow order,
as a table of contents for the algorithm. Parameters no stage claims
land in a trailing "More Parameters" section, and a schema with no
`parameterIds` at all falls back to a single flat section - so plugins
stay fully usable while schemas gain annotations incrementally
(31 of 40 plugins are annotated: all 16 Lexicon - the five reverb-core
plugins each show the 4-Voice Reverb Shell as their root diagram, e.g.
`concertHallAlgorithmSchema()`, with the core as a drill-down - plus,
on the Eventide side, every algorithm with more than ~a dozen
parameters: Diatonic Shift, Swept Combs, Swept Reverb, Reverb Factory,
Ultra-Tap, Patch Factory, Stutter, Dense Room, Multi-Shift, Band Delay,
String Modeller, Phaser, Studio Sampler. Deliberately left on the
flat-panel fallback: the nine small Eventide algorithms with <=11
parameters, where grouping adds little, and the two mod factory
patch-bays, whose inline-generated per-destination IDs and
several-dozen-module surface deserve their own layout treatment
rather than this grouping. Quad>Hall's shared Splice is the one
deliberately unclaimed parameter - see that schema's comment). The
panel validates every schema-listed ID against the processor's live
parameter list in debug builds (jassert on drift), keeping the
hand-authored schema honest. A scratchpad cross-checker used during
annotation also verified every claimed ID against each processor's
registered parameters, including the generated per-voice/per-tap/
per-line families. This stage<->parameter mapping is also exactly the
linkage the bidirectional highlight (hover a knob, its stage glows in
the diagram; hover/click a stage, its knobs highlight/scroll into
view - built, see LoomPluginEditor) and the generic cross-algorithm
"Loom" UI need.

Not yet done: full parameter *descriptions* in the schema
(range/default/unit/setter - ranges still live in each plugin's
`createParameterLayout()`), and the schema is still hand/AI-authored
prose-derived, not generated from or generating the C++, per the
original tier-1 framing.

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
