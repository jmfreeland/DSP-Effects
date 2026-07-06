#pragma once

#include "dsp/schema/AlgorithmSchema.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

// Renders a dsp::schema::AlgorithmSchema as a signal-flow diagram: one
// box per stage, stacked in schema order, with arrows drawn between
// whichever stages a Connection names (adjacent or not) and a small loop
// glyph for a stage that feeds back into itself (the reverb tank). Driven
// entirely by the schema - swapping in a different schema (e.g. a future
// alternate implementation of the same algorithm) needs no changes here.
class ArchitectureView : public juce::Component
{
  public:
    explicit ArchitectureView(const dsp::schema::AlgorithmSchema& schema);

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Height needed to lay out every stage at the given width, so a
    // parent Viewport can size this component to fit its content.
    int preferredHeightForWidth(int width) const;

  private:
    struct BoxLayout
    {
        const dsp::schema::Stage* stage;
        juce::Rectangle<float> bounds;
    };

    void layoutBoxes(int width);
    const BoxLayout* findBox(const char* id) const;
    void drawConnection(juce::Graphics& g, const dsp::schema::Connection& connection);

    const dsp::schema::AlgorithmSchema& schema_;
    std::vector<BoxLayout> boxes_;

    static constexpr float kBoxHeight = 64.0f;
    static constexpr float kBoxGap = 34.0f;
    static constexpr float kMargin = 8.0f;
    static constexpr float kSideLaneWidth = 84.0f;
    static constexpr float kHeaderHeight = 64.0f;
};
