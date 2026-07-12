# The Loom visual language

The design reference for everything visual in `plugin/` (and, later, the
single cross-algorithm "Loom" plugin). Three pillars, deliberately mixed:

1. **Modern node-editor structure** (Blender 2.8+, Bitwig): flat dark
   surfaces, nodes with colored header strips and neutral bodies, socket
   dots where wires attach, thin precise value arcs on knobs, small
   uppercase section labels. Structure and interaction feel current.
2. **Vintage device color** (Lexicon PCM81, Eventide H3000 front
   panels): each device family gets its own accent, taken from its
   hardware display - **Lexicon = green phosphor** (the PCM81's
   dot-matrix display), **Eventide = LED amber** (the H3000's meters and
   numerics). Accents drive knob arcs, highlights, wire annotations, and
   the diagram's display-style footer - so a Lexicon plugin glows green
   where an Eventide plugin glows amber, the way the real units did.
3. **90s SGI / Softimage XSI nostalgia**: surfaces are slate blue-gray
   rather than neutral black (the IRIX / XSI panel family), with
   restrained 1px bevel edges (light top, dark bottom) on nodes - an
   accent of era, not a Win95 chrome costume.

The executable half of this document is `plugin/source/LoomTheme.h/.cpp`:
one palette and one LookAndFeel that both editor halves draw from. If a
color or convention isn't in there, it isn't in the language yet.

## Surfaces

| Role | Colour | Notes |
|---|---|---|
| Diagram background | `#262b32` | dark slate, not black - the XSI cue |
| Node body | `#313842` | slightly lighter slate |
| Panel background | `#2a2f36` | between the two |
| Display strip / value wells | `#101418` | near-black inset, for accent-colored "display" text |

Bevels: nodes get a 1px light top edge (white @ 8%) and 1px dark bottom
edge (black @ 35%) inside their border. Nothing else is beveled.

## Stage roles

Every schema stage kind has one hue, used identically in the diagram
(node header strip) and the knob panel (section underline):

| Kind | Colour | Meaning |
|---|---|---|
| Input | `#4a6c8c` slate blue | audio enters |
| Processing | `#3c6b52` muted green | linear processing |
| Feedback | `#8a5a28` amber-rust | recirculating block - the manuals' own shaded box |
| Output | `#5f4370` plum | audio leaves |

Role colors are *muted* fills; they never carry interaction state.

## Family accents

| Family | Accent | Source |
|---|---|---|
| Lexicon | `#3fd97f` green phosphor | PCM81 dot-matrix display |
| Eventide | `#ffb028` LED amber | H3000 meters/numerics |

The accent is derived from the plugin name at runtime (contains
"Eventide" -> amber, else green) and drives: knob value arcs, hover
highlights (both directions of the knob<->stage cross-highlight), wire
annotation labels, drill-down affordances, the "< Back" breadcrumb, and
the footer display text. One hue per plugin - accents never mix.

## Diagram conventions

- **Flow** is horizontal, left to right, like the manuals' own block
  diagrams. Parallel branches straddle the center line.
- **Nodes**: colored header strip (role hue) holding the stage label;
  slate body holding the stage's parameter callouts (humanized from
  `Stage::parameterIds`). Drill-down nodes mark their header with the
  drill glyph and an accent border.
- **Wires** are thin white-slate orthogonal runs with arrowheads; wire
  *annotations* (conditions, gains, routing notes) are small italics in
  the family accent.
- **Socket dots** mark where wires leave/enter a node edge.
- **Junction dot** (filled circle) where one output splits to several
  destinations.
- **Sum glyph ⊕** (circled plus) where several wires merge into one
  input - the manuals' own summing junction.
- **Gain glyph ⊗ is reserved**: the manuals put one wherever a level
  control sits on a wire, but placing it honestly needs finer-grained
  schema stages than today's. Don't fake it with heuristics.
- **Terminal arrows**: input nodes with no upstream get a short entry
  arrow from the left edge; output nodes with no downstream get an exit
  arrow - the manuals' "L In ->" / "-> Left" stubs.
- **Feedback returns** route below the diagram; long skip busses route
  above it, stacked. Self-loops arc under their own node.
- **Footer display**: a near-black inset strip styled after the devices'
  displays - accent-colored text showing the hovered stage's detail.

## Controls

- **Knobs**: dark track ring, family-accent value arc, white pointer;
  value wells below in display styling. No skeuomorphic metal.
- **Section headers** (panel): small uppercase title, role-colored 3px
  underline; hover highlight tints the section in the family accent.
- Choice parameters are combo boxes, booleans are toggles - stock JUCE
  shapes recolored by the LookAndFeel, nothing custom.

## Don't

- Don't introduce hues outside the role + accent set; if something needs
  a new color, it needs a new *meaning* first, added here.
- Don't put interaction state (hover/selection) in role colors, or role
  meaning in accents.
- Don't fake manual glyphs (⊗) the schema can't back.
