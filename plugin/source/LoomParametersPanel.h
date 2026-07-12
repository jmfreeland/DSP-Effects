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
class LoomParametersPanel : public juce::Component
{
  public:
    LoomParametersPanel(juce::AudioProcessor& processor, const dsp::schema::AlgorithmSchema& schema);
    ~LoomParametersPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    // Height needed to lay out every section at the given width, so a
    // parent Viewport can size this component to fit its content.
    int preferredHeightForWidth(int width) const;

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
        std::vector<Cell> cells;
        juce::Rectangle<int> headerBounds; // filled in by resized()
    };

    void buildSections(juce::AudioProcessor& processor, const dsp::schema::AlgorithmSchema& schema);
    void collectStageSections(const dsp::schema::AlgorithmSchema& schema,
                              std::vector<juce::RangedAudioParameter*>& unclaimed, int depth);
    Cell makeCell(juce::RangedAudioParameter& parameter);
    int columnsForWidth(int width) const;

    juce::AudioProcessor* processor_ = nullptr;
    std::vector<Section> sections_;

    static constexpr int kCellWidth = 108;
    static constexpr int kCellHeight = 104;
    static constexpr int kSectionHeaderHeight = 30;
    static constexpr int kSectionGap = 10;
    static constexpr int kMargin = 8;
};
