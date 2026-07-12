#pragma once

#include "dsp/schema/AlgorithmSchema.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <memory>
#include <vector>

// The parameter side of every Loom plugin editor: rotary knobs (plus
// combo boxes for choice parameters and toggles for booleans), grouped
// into titled sections by which schema Stage each parameter drives -
// so the panel reads as a table of contents for the algorithm's own
// signal flow, in flow order, instead of one flat slider list.
//
// Grouping comes entirely from Stage::parameterIds (see
// dsp/schema/AlgorithmSchema.h), walked depth-first so a stage's
// drill-down schema contributes its sections in place. Parameters the
// schema doesn't claim land in a trailing "More Parameters" section, and
// a schema with no parameterIds at all falls back to a single section
// holding everything - both keep every plugin fully usable while schemas
// gain annotations incrementally.
//
// In debug builds the panel validates every schema-listed ID against the
// processor's live parameter list (jassert on drift), keeping the
// hand-authored schema honest - see Stage::parameterIds' doc comment.
//
// Cross-highlighting with the architecture diagram (see LoomPluginEditor):
// hovering anywhere in a section fires onSectionHovered with that
// section's own stage id plus its root-level ancestor's id (so the
// diagram can glow whichever it is currently showing), and
// setHighlightedStage() glows the header of every section belonging to
// the given stage id (matching either the section's own stage or its
// root ancestor - so hovering the diagram's "core" box lights every
// core-internal section at once). sectionTopForStage() gives a parent
// the scroll target for click-to-jump.
class LoomParametersPanel : public juce::Component
{
  public:
    LoomParametersPanel(juce::AudioProcessor& processor, const dsp::schema::AlgorithmSchema& schema);
    ~LoomParametersPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    // Height needed to lay out every section at the given width, so a
    // parent Viewport can size this component to fit its content.
    int preferredHeightForWidth(int width) const;

    // The device-family accent (see LoomTheme.h) for the hover
    // highlight tint.
    void setAccentColour(juce::Colour accent);

    // Glows the headers of all sections whose own stage id or root
    // ancestor id matches (empty string = clear).
    void setHighlightedStage(const juce::String& stageId);

    // Y position of the first section matching the stage id (own or root
    // ancestor), or -1 if none - a parent Viewport's scroll target.
    int sectionTopForStage(const juce::String& stageId) const;

    // Fired with (own stage id, root ancestor stage id) when the mouse
    // moves over a section; both empty when it leaves all sections.
    std::function<void(const juce::String&, const juce::String&)> onSectionHovered;

  private:
    struct Cell
    {
        juce::RangedAudioParameter* parameter = nullptr;
        std::unique_ptr<juce::Label> nameLabel;
        // Exactly one of these is non-null, depending on parameter type.
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ComboBox> comboBox;
        std::unique_ptr<juce::ToggleButton> toggle;
        std::unique_ptr<juce::SliderParameterAttachment> sliderAttachment;
        std::unique_ptr<juce::ComboBoxParameterAttachment> comboAttachment;
        std::unique_ptr<juce::ButtonParameterAttachment> buttonAttachment;
    };

    struct Section
    {
        juce::String title;
        juce::String stageId;     // the schema stage this section came from ("" for fallback/More)
        juce::String rootStageId; // that stage's top-level ancestor in the root schema
        dsp::schema::StageKind kind = dsp::schema::StageKind::kProcessing;
        bool hasKind = false; // false for fallback/More sections (no role underline)
        std::vector<Cell> cells;
        juce::Rectangle<int> headerBounds; // filled in by resized()
        juce::Rectangle<int> bounds;       // header + cells, for hover hit-testing
    };

    void buildSections(juce::AudioProcessor& processor, const dsp::schema::AlgorithmSchema& schema);
    void collectStageSections(const dsp::schema::AlgorithmSchema& schema,
                              std::vector<juce::RangedAudioParameter*>& unclaimed, int depth,
                              const char* rootStageId);
    Cell makeCell(juce::RangedAudioParameter& parameter);
    int columnsForWidth(int width) const;
    const Section* sectionAt(juce::Point<int> position) const;
    static bool sectionMatches(const Section& section, const juce::String& stageId);

    juce::AudioProcessor* processor_ = nullptr;
    std::vector<Section> sections_;
    juce::String highlightedStageId_;
    const Section* hoveredSection_ = nullptr;
    juce::Colour accent_ { 0xff3fd97f };

    static constexpr int kCellWidth = 108;
    static constexpr int kCellHeight = 104;
    static constexpr int kSectionHeaderHeight = 30;
    static constexpr int kSectionGap = 10;
    static constexpr int kMargin = 8;
};
