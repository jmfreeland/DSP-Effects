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
//
// Cross-highlighting with the parameter panel (see LoomPluginEditor):
// hovering any stage box fires onStageHovered, clicking a box without a
// drill-down fires onStageClicked (drill-down boxes keep navigating),
// and setHighlightedStages() glows whichever of the given stage IDs are
// visible at the current navigation level - so a knob hover can light up
// its stage here whether the view is showing the root diagram (the
// stage's top-level ancestor) or a drilled-into sub-diagram (the stage
// itself).
class ArchitectureView : public juce::Component
{
  public:
    explicit ArchitectureView(const dsp::schema::AlgorithmSchema& rootSchema);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    // Height needed to lay out every stage at the given width, so a
    // parent Viewport can size this component to fit its content.
    int preferredHeightForWidth(int width) const;

    // Glows any currently-visible stage box whose id is in `ids`
    // (empty = no highlight).
    void setHighlightedStages(const juce::StringArray& ids);

    std::function<void()> onContentSizeChanged;
    // Hovered stage (nullptr when the mouse leaves all boxes).
    std::function<void(const dsp::schema::Stage*)> onStageHovered;
    // Clicked stage - only fired for boxes without a drill-down (a
    // drill-down box's click navigates instead, as before).
    std::function<void(const dsp::schema::Stage&)> onStageClicked;

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
    juce::StringArray highlightedStageIds_;
    const dsp::schema::Stage* hoveredStage_ = nullptr;

    static constexpr float kBoxHeight = 64.0f;
    static constexpr float kBoxGap = 34.0f;
    static constexpr float kMargin = 8.0f;
    static constexpr float kSideLaneWidth = 84.0f;
    static constexpr float kHeaderHeight = 64.0f;
};
