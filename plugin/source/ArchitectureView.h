#pragma once

#include "dsp/schema/AlgorithmSchema.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <vector>

// Renders a dsp::schema::AlgorithmSchema as a manual-style signal-flow
// diagram, modeled on the source hardware manuals' own block diagrams
// (e.g. the PCM81 User Guide's Reverb Shell, p.3-2): horizontal
// left-to-right flow, small labeled boxes, parallel branches straddling
// the center line, orthogonal connectors with arrowheads, feedback
// returns routed underneath the row, circled-plus sum glyphs where wires
// merge, circled-times gain glyphs where a Connection::gainLabel names a
// scalar multiply on the wire, and each stage's parameter names called
// out in small text under its box (from Stage::parameterIds).
// Stage *detail* text doesn't fit manual-sized boxes, so the hovered
// stage's detail shows in a footer strip instead.
//
// Layout is derived from the schema alone: columns come from a
// longest-path pass over the connections that respect stage declaration
// order (schemas are authored in flow order), connections that point
// backward against that order are drawn as feedback returns below the
// diagram, and stages sharing a column stack symmetrically around the
// center line - the manual's own voices-above/voices-below look.
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
// visible at the current navigation level.
class ArchitectureView : public juce::Component
{
  public:
    explicit ArchitectureView(const dsp::schema::AlgorithmSchema& rootSchema);

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    // Natural diagram size: width grows with the schema's column count
    // (a parent Viewport scrolls horizontally when it exceeds the
    // visible area), height with its lane count plus feedback returns.
    int preferredWidth() const;
    int preferredHeightForWidth(int width) const;

    // The device-family accent (see LoomTheme.h) for highlights, wire
    // annotations, drill affordances, and the footer display.
    void setAccentColour(juce::Colour accent);

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
        int column = 0;
        int lane = 0;
        int numIncoming = 0; // non-self connections arriving (sum glyphs / sockets / terminals)
        int numOutgoing = 0; // non-self connections leaving (junction dots / sockets / terminals)
    };

    void computeLayout();
    const BoxLayout* findBox(const char* id) const;
    const BoxLayout* boxAt(juce::Point<float> position) const;
    void navigateTo(const dsp::schema::AlgorithmSchema* schema);
    void drawBox(juce::Graphics& g, const BoxLayout& box);
    void drawConnection(juce::Graphics& g, const dsp::schema::Connection& connection,
                        int& backEdgeIndex, int& skipEdgeIndex);
    void drawGainGlyph(juce::Graphics& g, juce::Point<float> center, const char* gainLabel,
                       bool withText = true);
    void drawFooter(juce::Graphics& g);
    juce::Rectangle<float> backButtonBounds() const;

    const dsp::schema::AlgorithmSchema* currentSchema_;
    std::vector<const dsp::schema::AlgorithmSchema*> history_;
    std::vector<BoxLayout> boxes_;
    int numColumns_ = 0;
    int maxLanesInColumn_ = 0;
    int numBackEdges_ = 0;
    int numSkipEdges_ = 0;
    float centerY_ = 0.0f;
    juce::Colour accent_ { 0xff3fd97f };
    juce::StringArray highlightedStageIds_;
    const dsp::schema::Stage* hoveredStage_ = nullptr;

    static constexpr float kBoxWidth = 150.0f;
    static constexpr float kBoxHeight = 58.0f; // role header strip + callout body
    static constexpr float kHeaderStripHeight = 20.0f;
    static constexpr float kColumnGap = 56.0f;
    static constexpr float kLaneHeight = kBoxHeight + 44.0f;
    static constexpr float kMargin = 10.0f;
    static constexpr float kTerminalLength = 26.0f; // the manuals' "L In ->" stubs
    static constexpr float kHeaderHeight = 64.0f;
    static constexpr float kFooterHeight = 34.0f;
    static constexpr float kBackEdgeSpacing = 14.0f;
    static constexpr float kSkipEdgeSpacing = 14.0f;
    static constexpr float kSelfLoopClearance = 30.0f;
};
