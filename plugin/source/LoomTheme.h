#pragma once

#include "dsp/schema/AlgorithmSchema.h"

#include <juce_gui_basics/juce_gui_basics.h>

// The Loom visual language in code: one palette and one LookAndFeel that
// both halves of the editor (ArchitectureView diagrams and
// LoomParametersPanel knobs) draw from, so stage-role colors and the
// device-family accent mean the same thing everywhere. The full language
// - three pillars: modern node-editor structure, vintage device accents,
// SGI/XSI slate surfaces - is written up in docs/loom-visual-language.md;
// this header is its executable half.
namespace loom
{
namespace colours
{
// Surfaces (slate blue-gray family - the SGI/XSI cue, not neutral black).
inline const juce::Colour kDiagramBackground { 0xff262b32 };
inline const juce::Colour kNodeBody { 0xff313842 };
inline const juce::Colour kPanelBackground { 0xff2a2f36 };
inline const juce::Colour kDisplayWell { 0xff101418 }; // near-black inset for "display" text

// Stage roles (node header strips, panel section underlines). Muted
// fills; interaction state never rides on these.
inline const juce::Colour kInput { 0xff4a6c8c };      // slate blue - audio enters
inline const juce::Colour kProcessing { 0xff3c6b52 }; // muted green - linear processing
inline const juce::Colour kFeedback { 0xff8a5a28 };   // amber-rust - recirculating block
inline const juce::Colour kOutput { 0xff5f4370 };     // plum - audio leaves

// Device-family accents, from the hardware's own displays: the PCM81's
// green phosphor dot-matrix and the H3000's amber LEDs. One per plugin;
// drives knob arcs, highlights, wire annotations, drill affordances.
inline const juce::Colour kLexiconPhosphor { 0xff3fd97f };
inline const juce::Colour kEventideAmber { 0xffffb028 };

// Wires.
inline const juce::Colour kWire = juce::Colour(0xffcfd6dd).withAlpha(0.55f);

// Bevels (restrained 1px, nodes only).
inline const juce::Colour kBevelLight = juce::Colours::white.withAlpha(0.08f);
inline const juce::Colour kBevelDark = juce::Colours::black.withAlpha(0.35f);
}

inline juce::Colour colourForStageKind(dsp::schema::StageKind kind)
{
    switch (kind)
    {
        case dsp::schema::StageKind::kInput:
            return colours::kInput;
        case dsp::schema::StageKind::kProcessing:
            return colours::kProcessing;
        case dsp::schema::StageKind::kFeedback:
            return colours::kFeedback;
        case dsp::schema::StageKind::kOutput:
            return colours::kOutput;
    }
    return juce::Colours::darkgrey;
}

// The family accent is derived from the plugin's product name ("Loom -
// Lexicon Hall", "Loom - Eventide Ultra-Tap"), so the shared editor
// needs no per-plugin wiring.
inline juce::Colour accentForPluginName(const juce::String& name)
{
    return name.containsIgnoreCase("Eventide") ? colours::kEventideAmber
                                                : colours::kLexiconPhosphor;
}

// Custom controls: an arc-style rotary (dark track ring, family-accent
// value arc, white pointer) that reads at the panel's small knob size
// better than the stock filled-circle JUCE rotary; surfaces and text
// wells recolored to the slate/display palette.
class LookAndFeel : public juce::LookAndFeel_V4
{
  public:
    explicit LookAndFeel(juce::Colour accent);

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override;

  private:
    juce::Colour accent_;
};
}
