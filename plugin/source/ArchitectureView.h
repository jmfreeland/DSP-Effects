#pragma once

#include "dsp/schema/AlgorithmSchema.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

// Renders a dsp::schema::AlgorithmSchema as a signal-flow diagram: one
// box per stage, stacked in schema order, with arrows drawn between
// whichever stages a Connection names (adjacent or not) and a small loop
// glyph for a stage that feeds back into itself (the reverb tank). Driven
// entirely by the schema - swapping in a different schema (e.g. a future
// alternate implementation of the same algorithm) needs no changes here.
//
// Stages with a non-null Stage::drillDown are clickable: clicking one
// navigates into that sub-schema (e.g. the Tank's own internals), with a
// "< Back" breadcrumb to return. Since the content size changes when
// navigating, onContentSizeChanged fires so a parent Viewport can re-fit.
class ArchitectureView : public juce::Component
{
  public:
    explicit ArchitectureView(const dsp::schema::AlgorithmSchema& rootSchema);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;

    // Height needed to lay out every stage at the given width, so a
    // parent Viewport can size this component to fit its content.
    int preferredHeightForWidth(int width) const;

    std::function<void()> onContentSizeChanged;

  private:
    struct BoxLayout
    {
        const dsp::schema::Stage* stage;
        juce::Rectangle<float> bounds;
    };

    void layoutBoxes(int width);
    const BoxLayout* findBox(const char* id) const;
    const BoxLayout* boxAt(juce::Point<float> position) const;
    void drawConnection(juce::Graphics& g, const dsp::schema::Connection& connection);
    juce::Rectangle<float> backButtonBounds() const;

    const dsp::schema::AlgorithmSchema* currentSchema_;
    std::vector<const dsp::schema::AlgorithmSchema*> history_;
    std::vector<BoxLayout> boxes_;

    static constexpr float kBoxHeight = 64.0f;
    static constexpr float kBoxGap = 34.0f;
    static constexpr float kMargin = 8.0f;
    static constexpr float kSideLaneWidth = 84.0f;
    static constexpr float kHeaderHeight = 64.0f;
};
