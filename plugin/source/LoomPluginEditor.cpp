#include "LoomPluginEditor.h"

namespace
{
constexpr int kEditorWidth = 560;
constexpr int kEditorHeight = 860;
constexpr int kSeparatorHeight = 6;
// Diagram share of the editor height; the rest is knobs. Both halves
// scroll independently, so neither needs to fit its content exactly.
constexpr float kDiagramShare = 0.42f;
}

LoomPluginEditor::LoomPluginEditor(juce::AudioProcessor& audioProcessor,
                                    const dsp::schema::AlgorithmSchema& schema)
  : AudioProcessorEditor(audioProcessor), architectureView_(schema),
    parametersPanel_(audioProcessor, schema)
{
    addAndMakeVisible(architectureViewport_);
    architectureViewport_.setViewedComponent(&architectureView_, false);
    architectureView_.onContentSizeChanged = [this] { updateArchitectureViewSize(); };

    addAndMakeVisible(parametersViewport_);
    parametersViewport_.setViewedComponent(&parametersPanel_, false);

    // Knob hover -> glow the stage box (or its root-level ancestor,
    // whichever the diagram's current navigation level actually shows).
    parametersPanel_.onSectionHovered = [this](const juce::String& stageId,
                                               const juce::String& rootStageId) {
        juce::StringArray ids;
        if (stageId.isNotEmpty())
        {
            ids.add(stageId);
        }
        if (rootStageId.isNotEmpty() && rootStageId != stageId)
        {
            ids.add(rootStageId);
        }
        architectureView_.setHighlightedStages(ids);
    };

    // Stage hover -> glow its parameter section(s).
    architectureView_.onStageHovered = [this](const dsp::schema::Stage* stage) {
        parametersPanel_.setHighlightedStage(stage != nullptr ? juce::String(stage->id)
                                                               : juce::String());
    };

    // Stage click (non-drill-down boxes) -> jump the knob panel to that
    // stage's section.
    architectureView_.onStageClicked = [this](const dsp::schema::Stage& stage) {
        auto target = parametersPanel_.sectionTopForStage(stage.id);
        if (target >= 0)
        {
            parametersViewport_.setViewPosition(0, target);
        }
    };

    setResizable(true, true);
    setSize(kEditorWidth, kEditorHeight);
}

void LoomPluginEditor::resized()
{
    auto bounds = getLocalBounds();
    auto diagramHeight = static_cast<int>(static_cast<float>(bounds.getHeight()) * kDiagramShare);

    architectureViewport_.setBounds(bounds.removeFromTop(diagramHeight));
    updateArchitectureViewSize();

    bounds.removeFromTop(kSeparatorHeight);
    parametersViewport_.setBounds(bounds);
    updateParametersPanelSize();
}

void LoomPluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    auto separatorY = architectureViewport_.getBottom();
    g.setColour(juce::Colours::white.withAlpha(0.2f));
    g.fillRect(0, separatorY + kSeparatorHeight / 2, getWidth(), 1);
}

void LoomPluginEditor::updateParametersPanelSize()
{
    auto panelWidth = parametersViewport_.getWidth() - parametersViewport_.getScrollBarThickness();
    parametersPanel_.setSize(panelWidth, parametersPanel_.preferredHeightForWidth(panelWidth));
}

void LoomPluginEditor::updateArchitectureViewSize()
{
    auto architectureWidth = architectureViewport_.getWidth() - architectureViewport_.getScrollBarThickness();
    architectureView_.setSize(architectureWidth, architectureView_.preferredHeightForWidth(architectureWidth));
}
